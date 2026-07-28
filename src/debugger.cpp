#include "debugger.h"

#include <llvm/DebugInfo/DWARF/DWARFAcceleratorTable.h>
#include <llvm/Object/ELFObjectFile.h>
#include <llvm/Object/ObjectFile.h>

#include <cstdio>
#include <cxxabi.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <thread>
#include <chrono>

using namespace llvm;

namespace debugger
{
    Debugger::Debugger(int pid, const char* program)
        : regs_{},
        pid_{ pid },
        wait_status_{},
        msymtabs_{},
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
        auto it{ msymtabs_.find(fn_name) };
        if (it == msymtabs_.end())
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
            if (!demangler_.partialDemangle(mangled_buffer_.data())) {
                if (!demangler_.isFunction()) continue;
                demangler_.getFunctionBaseName(demangled_buffer_, &demangled_buffer_size_);
                uint64_t fn_size{ object::ELFSymbolRef{ sym_ }.getSize() };
                msymtabs_.try_emplace(std::string{ demangled_buffer_, demangled_buffer_size_ }, FnSym{ *vaddr, fn_size });

            } else {
                uint64_t fn_size{ object::ELFSymbolRef{ sym_ }.getSize() };
                msymtabs_.try_emplace(std::string{ name->str(), name->size() }, FnSym{ *vaddr, fn_size });
            }

        }
    }
    
    void Debugger::reg_read()
    {
        ptrace(PTRACE_GETREGS, pid_, nullptr, &regs_);
    }
}

