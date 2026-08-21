#include "debugger.h"

#include "utils/logging.h"
#include "utils/reg_numbers.h"

#include <llvm/DebugInfo/DWARF/DWARFAcceleratorTable.h>
#include <llvm/DebugInfo/DWARF/DWARFCompileUnit.h>
#include <llvm/DebugInfo/DWARF/DWARFDebugFrame.h>
#include <llvm/DebugInfo/DWARF/LowLevel/DWARFUnwindTable.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Object/ObjectFile.h>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cxxabi.h>
#include <fcntl.h>
#include <fstream>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

using namespace llvm;

namespace debugger
{
    Debugger::Debugger(int pid, const char* program)
        : pid_{ pid }
    {
        auto obj_file_expected{ object::ObjectFile::createObjectFile(program) };
        if (!obj_file_expected) {
            consumeError(obj_file_expected.takeError());
            printf("Failed to open LLVM object file!\n");
            return;
        }
        obj_file_ = std::move(*obj_file_expected);
        obj_ = obj_file_.getBinary();
        dwarf_ctx_ = DWARFContext::create(*obj_);
        triple_ = obj_->makeTriple();

        disassembler_.setup(obj_, triple_);
        
        build_msymtabs();

        // get .eh_frame section (for stack tracing)
        Expected<const DWARFDebugFrame*> eh_frame_expected = dwarf_ctx_->getEHFrame();
        if (!eh_frame_expected) {
            consumeError(eh_frame_expected.takeError());
            logging::error();
            return;
        }
        eh_frame_ = eh_frame_expected.get();
    }

    Debugger::~Debugger()
    {
        if (!WIFEXITED(wait_status_) && !WIFSIGNALED(wait_status_)) {
            kill(pid_, SIGKILL);
            waitpid(pid_, nullptr, 0);
        }
    }
    
    void Debugger::init()
    {
        waitpid(pid_, &wait_status_, 0);
        
        if (WIFSTOPPED(wait_status_)) {
            printf("Starting!\n");
        }
        
        ptrace(PTRACE_SETOPTIONS, pid_, nullptr, PTRACE_O_EXITKILL);

        get_base_addr();
    }
    
    void Debugger::cont()
    {
        reg_read();
        word v_rip{ regs_.rip - base_addr_ };
        step_through_breakpoint(v_rip);
        // need to check if debuggee was stopped by a sigtrap

        ptrace(PTRACE_CONT, pid_, nullptr, nullptr);
        regs_updated_ = false;
        waitpid(pid_, &wait_status_, 0);

        // program  

        // breakpoint hit

        // reg_read();
        // assert(breakpoints_lookup_.contains(regs_.rip - base_addr_));
        // // LATER: UPDATE CURRENT CU PROPERLY
        // current_cu_ = breakpoints_lookup_[regs_.rip - base_addr_]->info.
    }
    
    void Debugger::breakpoint(word vaddr)
    {
        object::SectionedAddress sectioned_address{ vaddr, object::SectionedAddress::UndefSection };
        auto info_expected{ symbolizer_.symbolizeCode(*obj_, sectioned_address) }; // this stores source file name and line number
        if (!info_expected) {
            consumeError(info_expected.takeError());
        }
        
        breakpoints_.emplace_back(std::make_unique<Breakpoint>(
            info_expected.get(),
            vaddr,
            static_cast<byte>(ptrace(PTRACE_PEEKTEXT, pid_, vaddr + base_addr_) & 0xFF)
        ));
        breakpoints_lookup_[vaddr] = breakpoints_.back().get();
        breakpoints_sorted_ = false;
       
        set_byte(vaddr, 0xcc);
    }

    void Debugger::breakpoint(std::string_view arg)
    {
        size_t colon_pos{ arg.find(':') };
        if (colon_pos == std::string::npos) {
            auto it{ msymtabs_.find(arg) };
            if (it == msymtabs_.end())
            {
                printf("Function not found!\n");
                // search thru dwarf objects
            }
            else
            {
                breakpoint(it->second.vaddr);
            }
        } else {
            // break at line
            std::string_view file_name{ arg.substr(0, colon_pos) };
            std::string_view line_sv{ arg.substr(colon_pos + 1) };
            size_t line;
            auto [_, exception] { std::from_chars(line_sv.data(), line_sv.data() + line_sv.size(), line) };
            if (exception == std::errc()) {
                breakpoint(file_name, line);
            } else {
                printf("Invalid line number!\n");
            }
        }
    }
    
    void Debugger::breakpoint(std::string_view file_name, size_t line)
    {
        bool found{ false };
        word vaddr;

        // find the line's vaddr
        for (const std::unique_ptr<llvm::DWARFUnit>& unit : dwarf_ctx_->compile_units()) {
            if (llvm::DWARFCompileUnit* cu{ llvm::dyn_cast<llvm::DWARFCompileUnit>(unit.get()) }) {
                const DWARFDebugLine::LineTable* line_table{ dwarf_ctx_->getLineTableForUnit(cu) };
            if (!line_table) continue;
            for (const auto& row : line_table->Rows) {
                std::string row_file_name{};
                if (line_table->getFileNameByIndex(
                    row.File,
                    cu->getCompilationDir(),
                    llvm::DILineInfoSpecifier::FileLineInfoKind::BaseNameOnly, // todo: disambiguate if multiple matches
                    row_file_name)
                ) {
                    if (row.Line == line && row_file_name == file_name) {
                        // if (found) {
                        //     printf("Found multiple results!");
                        // }

                        found = true;
                        vaddr = row.Address.Address;
                        break;
                        }
                    }
                }
            }
        }

        if (!found) {
            printf("Didn't find line!\n");
        }

        breakpoint(vaddr);
    }
    
    void Debugger::del(size_t idx)
    {
        if (idx >= breakpoints_.size()) {
            printf("Invalid index!\n");
        }
        word vaddr{ breakpoints_[idx]->vaddr };
        set_byte(vaddr, breakpoints_[idx]->data);
        breakpoints_lookup_.erase(vaddr);
        breakpoints_.erase(breakpoints_.begin() + idx);

        // if we're on the deleted breakpoint right now, rip needs to be rewound
        reg_read();
        if (vaddr + base_addr_ == regs_.rip - 1) {
            set_reg(&regs_.rip, regs_.rip - 1);
        }
    }
    
    void Debugger::backtrace()
    {
        reg_read();
        word v_rip{ regs_.rip - base_addr_ };
        
        backtrace_recurse(v_rip, 0, regs_);
    }
    
    void Debugger::backtrace_recurse(word vaddr, size_t depth, const user_regs_struct& current_frame_regs)
    {
        constexpr size_t depth_limit{ 128 };

        // get function name & line number
        object::SectionedAddress sectioned_address{ vaddr, object::SectionedAddress::UndefSection };
        auto info_expected{ symbolizer_.symbolizeCode(*obj_, sectioned_address) }; // this stores source file name and line number
        if (!info_expected) {
            consumeError(info_expected.takeError());
        }
        const llvm::DILineInfo& info{ info_expected.get() };
        std::string_view function_name{ info.FunctionName };
        printf(
            "#%d\tFunction name: %s, file name: %s, line %d, address 0x%016llX\n",
            depth,
            function_name.data(),
            sys::path::filename(info.FileName).data(),
            info.Line,
            vaddr
        );

        // find fde
        const dwarf::FDE* fde{};
        for (const dwarf::FrameEntry& entry : *eh_frame_) {
            if (entry.getKind() == dwarf::FrameEntry::FrameKind::FK_FDE) {
                const dwarf::FDE* current_fde{ static_cast<const dwarf::FDE*>(&entry) };
                word start{ current_fde->getInitialLocation() };
                word len{ current_fde->getAddressRange() };
                if (vaddr >= start && vaddr < start + len) {
                    fde = current_fde;
                }
            }
        }
        if (!fde) {
            return;
        }

        // unwind table
        Expected<dwarf::UnwindTable> unwind_table_expected{ dwarf::createUnwindTable(fde) };
        if (!unwind_table_expected) {
            consumeError(unwind_table_expected.takeError());
            logging::error();
            return;
        }
        const dwarf::UnwindTable& unwind_table{ unwind_table_expected.get() };
        const dwarf::UnwindRow* unwind_row{};
        for (const dwarf::UnwindRow& current_row : unwind_table) {
            if (current_row.getAddress() <= vaddr) { // check if this actually gets the virtual address
                unwind_row = &current_row;
            } else {
                break;
            }
        }

        // cfa (canonical frame address). caller address / stack pointer
        const dwarf::UnwindLocation& cfa_rule{ unwind_row->getCFAValue() };

        user_regs_struct caller_frame_regs{};
        const dwarf::RegisterLocations& reg_locations{ unwind_row->getRegisterLocations() };
        word cfa{ evaluate_location(cfa_rule, 0, current_frame_regs, 0) };
        for (uint32_t reg_number : reg_locations.getRegisters())
        {
            std::optional<dwarf::UnwindLocation> location{ reg_locations.getRegisterLocation(reg_number) };
            word reg_value{ evaluate_location(*location, cfa, current_frame_regs, reg_number) };

            auto it{ utils::reg_numbers_x86_64.find(reg_number) };
            caller_frame_regs.*(it->second) = reg_value;
        }
        caller_frame_regs.rsp = cfa; // if caller frame's cfa rule uses rsp

        // // cie (common information entry)
        const dwarf::CIE* cie{ fde->getLinkedCIE() };
        word ra_reg{ cie->getReturnAddressRegister() };
        auto ra_rule{ unwind_row->getRegisterLocations().getRegisterLocation(ra_reg) };
        word ra{ evaluate_location(ra_rule.value(), cfa, current_frame_regs, ra_reg) - 1 };
        word v_ra{ ra - base_addr_};

        if (function_name != "main" && vaddr != 0 && depth < depth_limit) {
            backtrace_recurse(v_ra, depth + 1, caller_frame_regs);
        }
    }
    
    word Debugger::evaluate_location(const llvm::dwarf::UnwindLocation& rule, word cfa, const user_regs_struct& regs, uint32_t reg_number)
    {
        switch (rule.getLocation()) {
        case dwarf::UnwindLocation::Location::Same:
            return regs.*utils::reg_numbers_x86_64.at(reg_number);
        case dwarf::UnwindLocation::Location::CFAPlusOffset:
            {
                word val{ cfa + rule.getOffset() };
                if (rule.getDereference()) {
                    return ptrace(PTRACE_PEEKTEXT, pid_, val);
                } else {
                    return val; // add base_addr_??
                }
            }
        case dwarf::UnwindLocation::Location::RegPlusOffset:
            {
                word reg_val{ regs.*utils::reg_numbers_x86_64.at(rule.getRegister()) };
                word val{ reg_val + rule.getOffset() };
                if (rule.getDereference()) {
                    return ptrace(PTRACE_PEEKTEXT, pid_, val);
                } else {
                    return val;
                }
            }
        case dwarf::UnwindLocation::Location::Constant:
            return static_cast<word>(rule.getOffset());
        default:
            logging::error();
        }
    }
    
    void Debugger::info(std::string_view cmd)
    {
        if (cmd == "break") {
            // number of breakpoints is most likely too small to justify doing anything fancy (at least for now)
            if (!breakpoints_sorted_) {
                // sort by vaddr
                std::sort(
                    breakpoints_.begin(),
                    breakpoints_.end(), 
                    [] (const std::unique_ptr<Breakpoint>& a, const std::unique_ptr<Breakpoint>& b) {
                        return a->vaddr < b->vaddr;
                    }
                );
                breakpoints_sorted_ = true;
            }

            printf("Breakpoints:\n");
            for (const std::unique_ptr<Breakpoint>& breakpoint : breakpoints_) {
                printf(
                    "File: %s, Line: %d, virtual address: 0x%016llX, byte data: 0x%02hhX (%hhu)\n",
                    sys::path::filename(breakpoint->info.FileName).data(),
                    breakpoint->info.Line,
                    breakpoint->vaddr,
                    breakpoint->data,
                    breakpoint->data
                );
            }
        }
    }
    
    void Debugger::print_reg(const word* reg)
    {
        reg_read();
        printf("0x%016llX (%lld)\n", *reg, *reg); // print signed value
    }
    
    void Debugger::set_reg(word* reg, word data)
    {
        *reg = data;
        ptrace(PTRACE_SETREGS, pid_, nullptr, &regs_);
    }
    
    void Debugger::step()
    {
        
    }
    
    void Debugger::next()
    {
        // assumptions: not starting at end of function. no function calls / returns involved

        reg_read();
        word start_vaddr{ regs_.rip - base_addr_ };
        llvm::DWARFDebugLine::Row start_row{ get_src_row_info(start_vaddr) };
        if (!step_through_breakpoint(start_vaddr)) {
            ptrace(PTRACE_SINGLESTEP, pid_, nullptr, nullptr);
            regs_updated_ = false;
            waitpid(pid_, &wait_status_, 0);
        }
        while (true) {
            reg_read();
            word current_inst_vaddr{ regs_.rip - base_addr_ };
            Disassembler::Inst inst{ disassembler_.analyze_inst(current_inst_vaddr) };
            if (disassembler_.is_call(inst)) {
                // call
                // lightweight break()
                size_t current_inst_size{ inst.size };
                word temp_breakpoint_vaddr{ current_inst_vaddr + current_inst_size };
                byte temp_breakpoint_data{ static_cast<byte>(ptrace(PTRACE_PEEKTEXT, pid_, temp_breakpoint_vaddr + base_addr_) & 0xFF) };
                set_byte(temp_breakpoint_vaddr, 0xcc);

                // lightweight cont()        
                ptrace(PTRACE_CONT, pid_, nullptr, nullptr);
                regs_updated_ = false;
                waitpid(pid_, &wait_status_, 0);

                // lightweight del()
                set_byte(temp_breakpoint_vaddr, temp_breakpoint_data);
                set_reg(&regs_.rip, temp_breakpoint_vaddr + base_addr_);

                return;
            } else {
                // not call
                llvm::DWARFDebugLine::Row current_row{ get_src_row_info(current_inst_vaddr) };
                if (current_row.File == start_row.File && current_row.Line > start_row.Line) { // todo: check if we exited current function & went to a function with larger vaddr
                    return;
                } else {
                    ptrace(PTRACE_SINGLESTEP, pid_, nullptr, nullptr);
                    regs_updated_ = false;
                    waitpid(pid_, &wait_status_, 0);
                }
            }
        }
    }
    
    void Debugger::get_base_addr()
    {
        std::string maps_path{ "/proc/" + std::to_string(pid_) + "/maps" };
        std::ifstream maps_file{ maps_path };
        std::string line;
        if (getline(maps_file, line)) {
            size_t dash_idx{ line.find('-') };
            if (dash_idx != std::string::npos) {
                std::string start_addr_str{ line.substr(0, dash_idx) };
                base_addr_ = std::stoull(start_addr_str, nullptr, 16);
            }
        }
    }
    
    void Debugger::build_msymtabs()
    {
        for (const object::SymbolRef& sym_ : obj_->symbols()) {
            Expected<object::SymbolRef::Type> type{ sym_.getType() };
            if (!type) {
                consumeError(type.takeError());
                continue;
            }
            if (*type != object::SymbolRef::ST_Function) {
                continue;
            }

            Expected<StringRef> name{ sym_.getName() };
            if (!name) {
                consumeError(name.takeError());
                continue;
            }
            if (name->empty()) {
                continue;
            }

            Expected<word> vaddr{ sym_.getAddress() };
            if (!vaddr) {
                consumeError(vaddr.takeError());
                continue;
            }
            if (!*vaddr) {
                continue;
            }

            if (name.get().size() >= mangled_buffer_.capacity()) {
                printf("Mangled function name is too long!\n");
                continue;
            }
            mangled_buffer_.assign(*name);
            mangled_buffer_.push_back('\0');

            size_t demangled_buffer_size_{ demangled_buffer_capacity_ };
            const char* name_data;
            size_t name_size;
            if (!demangler_.partialDemangle(mangled_buffer_.data())) {
                if (!demangler_.isFunction()) continue;
                demangler_.getFunctionBaseName(demangled_buffer_, &demangled_buffer_size_);
                name_data = demangled_buffer_;
                name_size = demangled_buffer_size_;

            } else {
                name_data = name->str().data();
                name_size = name->size();
            }

            // for hashing purposes
            if (name_size > 0 && name_data[name_size - 1] == '\0') {
                name_size--;
            }

            uint64_t fn_size{ object::ELFSymbolRef{ sym_ }.getSize() };
            msymtabs_.try_emplace(std::string{ name_data, name_size }, FnSym{ *vaddr, fn_size });

        }
    }
    
    void Debugger::reg_read()
    {
        if (!regs_updated_) {
            ptrace(PTRACE_GETREGS, pid_, nullptr, &regs_);
        }
    }
    
    void Debugger::set_byte(word vaddr, byte val)
    {
        word old_word{ static_cast<word>(ptrace(PTRACE_PEEKTEXT, pid_, vaddr + base_addr_)) };
        word new_word{ (old_word & 0xffffffffffffff00) | val };
        ptrace(PTRACE_POKETEXT, pid_, vaddr + base_addr_, new_word);        
    }
    
    // v_rip: current pc. address of 0xcc, + 1 (unrewinded)
    bool Debugger::step_through_breakpoint(word v_rip)
    {
        word v_rip_rewinded{ v_rip - 1 };
        bool is_breakpoint{ breakpoints_lookup_.contains(v_rip_rewinded) };
        if (is_breakpoint) {
            set_reg(&regs_.rip, v_rip_rewinded + base_addr_);
            set_byte(v_rip_rewinded, breakpoints_lookup_[v_rip_rewinded]->data);
            ptrace(PTRACE_SINGLESTEP, pid_, nullptr, nullptr);
            regs_updated_ = false;
            waitpid(pid_, &wait_status_, 0);
            set_byte(v_rip_rewinded, 0xcc);
        }

        return is_breakpoint;
    }
    
    llvm::DWARFDebugLine::Row Debugger::get_src_row_info(word vaddr)
    {
        // maybe read rip again?
        // cache current cu?
        current_cu_ = dwarf_ctx_->getCompileUnitForCodeAddress(vaddr);
        assert(current_cu_);

        const auto& line_table{ *dwarf_ctx_->getLineTableForUnit(current_cu_) };
        uint32_t row_idx{ line_table.lookupAddress(llvm::object::SectionedAddress{ vaddr }) };
        return line_table.Rows[row_idx];
    }
}

