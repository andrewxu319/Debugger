#include "debugger.h"

#include <llvm/DebugInfo/DWARF/DWARFAcceleratorTable.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Object/ObjectFile.h>

#include <algorithm>
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
        : regs_{},
        pid_{ pid },
        wait_status_{},
        base_addr_{},
        msymtabs_{},
        breakpoints_lookup_{},
        breakpoints_{},
        demangler_{},
        mangled_buffer_{},
        demangled_buffer_{}
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
        
        build_msymtabs();
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
        if (breakpoints_lookup_.contains(regs_.rip - 1)) {
            reg_write(&regs_.rip, regs_.rip - 1);
            ptrace(PTRACE_POKETEXT, pid_, regs_.rip, breakpoints_lookup_[regs_.rip]->symbol->second.vaddr);
            ptrace(PTRACE_SINGLESTEP, pid_, nullptr, nullptr);
            waitpid(pid_, &wait_status_, 0);
            word old_word{ ptrace(PTRACE_PEEKTEXT, pid_, regs_.rip) };
            word new_word{ (old_word & 0xffffffffffffff00) | 0xcc };
            ptrace(PTRACE_POKETEXT, pid_, regs_.rip, new_word);
        }

        ptrace(PTRACE_CONT, pid_, nullptr, nullptr);
        waitpid(pid_, &wait_status_, 0);

        // program  

        // breakpoint hit
    }
    
    void Debugger::breakpoint(word vaddr, const std::pair<const std::string, FnSym>& symbol)
    {
        breakpoints_.emplace_back(std::make_unique<Breakpoint>(static_cast<word>(ptrace(PTRACE_PEEKTEXT, pid_, vaddr)), &symbol));
        breakpoints_lookup_[vaddr] = breakpoints_.back().get();
        breakpoints_sorted_ = false;
       
        word old_word{ ptrace(PTRACE_PEEKTEXT, pid_, vaddr + base_addr_) };
        word new_word{ (old_word & 0xffffffffffffff00) | 0xcc };
        ptrace(PTRACE_POKETEXT, pid_, vaddr + base_addr_, new_word);
    }

    void Debugger::breakpoint(std::string_view fn_name)
    {
        auto it{ msymtabs_.find(fn_name) };
        if (it == msymtabs_.end())
        {
            printf("Function not found!\n");
            // search thru dwarf objects
        }
        else
        {
            breakpoint(it->second.vaddr, (*it));
        }
    }
    
    void Debugger::del(size_t idx)
    {
        word vaddr{ breakpoints_[idx]->symbol->second.vaddr };
        ptrace(PTRACE_POKETEXT, pid_, vaddr + base_addr_, breakpoints_[idx]->content); // not working yets
        breakpoints_lookup_.erase(vaddr);
        breakpoints_.erase(breakpoints_.begin() + idx);
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
                        return a->symbol->second.vaddr < b->symbol->second.vaddr;
                    }
                );
            }

            printf("Breakpoints:\n");
            for (const std::unique_ptr<Breakpoint>& breakpoint : breakpoints_) {
                printf("Function: %s, virtual address: 0x%016llX (%llu), word content: 0x%016llX (%llu)\n", breakpoint->symbol->first.c_str(), breakpoint->symbol->second.vaddr, breakpoint->content);
            }
        }
    }
    
    void Debugger::print_reg(const word* reg)
    {
        reg_read();
        printf("0x%016llX (%lld)\n", *reg, *reg); // print signed value
    }
    
    void Debugger::reg_write(word* reg, word data)
    {
        *reg = data;
        ptrace(PTRACE_SETREGS, pid_, nullptr, &regs_);
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
        ptrace(PTRACE_GETREGS, pid_, nullptr, &regs_);
    }
}

