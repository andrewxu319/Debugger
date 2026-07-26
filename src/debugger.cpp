#include "debugger.h"

#include <cstdio>
#include <cxxabi.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <thread>
#include <chrono>

namespace debugger
{
    Debugger::Debugger(int pid, const char* program)
        : regs_{},
        pid_{ pid },
        wait_status_{},
        vaddr_by_fn_{},
        breakpoints_{}
    {
        int file_descriptor{ open(program, O_RDONLY) };
        elf_ = elf::elf{ elf::create_mmap_loader(file_descriptor) };
        dwarf_ = dwarf::dwarf{ dwarf::elf::create_loader(elf_) };

        build_symbol_map();
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
    }
    
    void Debugger::cont()
    {
        reg_read();
        if (breakpoints_.contains(regs_.rip - 1)) {
            reg_write(&regs_.rip, regs_.rip - 1);
            ptrace(PTRACE_POKETEXT, pid_, regs_.rip, breakpoints_[regs_.rip]);
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
    
    void Debugger::breakpoint(word vaddr)
    {
        breakpoints_[vaddr] = ptrace(PTRACE_PEEKTEXT, pid_, vaddr);
       
        word old_word{ ptrace(PTRACE_PEEKTEXT, pid_, vaddr + base_addr_) };
        word new_word{ (old_word & 0xffffffffffffff00) | 0xcc };
        ptrace(PTRACE_POKETEXT, pid_, vaddr + base_addr_, new_word);
    }

    void Debugger::breakpoint(std::string_view fn_name)
    {
        auto it{ vaddr_by_fn_.find(fn_name) };
        if (it == vaddr_by_fn_.end())
        {
            printf("Function not found!\n");
            // search thru dwarf objects
        }
        else
        {
            breakpoint(it->second.vaddr);
        }
    }
    
    void Debugger::print_reg(const word* reg)
    {
        reg_read();
        printf("0x%016llX (%lld)\n", *reg, *reg);
    }
    
    void Debugger::reg_write(word* reg, word data)
    {
        *reg = data;
        ptrace(PTRACE_SETREGS, pid_, nullptr, &regs_);
    }
    
    std::unique_ptr<char, void(*)(void*)> Debugger::demangle(const char* fn_name)
    {
        int status{};
        std::unique_ptr<char, void(*)(void*)>demangled_name{ abi::__cxa_demangle(fn_name, NULL, NULL, &status), std::free };
        if (status != 0) {
            demangled_name.reset();
        }
        return demangled_name;
    }
    
    void Debugger::build_symbol_map()
    {
/*
eventually: parse through index files
        try {
            const elf::section& gdb_index_section{ elf_.get_section(".gdb_index") };
            std::string_view gdb_index{ static_cast<const char*>(gdb_index_section.data()), gdb_index_section.size() };
        } catch (const std::out_of_range&) {
            const elf::section& debug_names_section{ elf_.get_section(".debug_names") };
            std::string_view debug_names{ static_cast<const char*>(debug_names_section.data()), debug_names_section.size() };
        }
*/
        for (elf::section section : elf_.sections()) {
            if (section.get_hdr().type == elf::sht::symtab /*|| section.get_hdr().type == elf::sht::dynsym*/) {
                for (elf::sym sym : section.as_symtab()) {
                    auto& data{ sym.get_data() };
                    if (data.type() == elf::stt::func) {
                        std::string mangled_name{ sym.get_name() };
                        std::unique_ptr<char, void(*)(void*)> demangled_name_unique_ptr{ demangle(mangled_name.data()) };
                        if (demangled_name_unique_ptr) {
                            std::string demangled_name{ demangled_name_unique_ptr.get() };
                            vaddr_by_fn_[demangled_name] = { data.value, data.size };
                            fn_by_vaddr_[data.value] = { demangled_name, data.size };
                        } else {
                            vaddr_by_fn_[mangled_name] = { data.value, data.size };
                            fn_by_vaddr_[data.value] = { mangled_name, data.size };
                        }
                    }
                }
            }
        }
    }
    
    void Debugger::reg_read()
    {
        ptrace(PTRACE_GETREGS, pid_, nullptr, &regs_);
    }
}

