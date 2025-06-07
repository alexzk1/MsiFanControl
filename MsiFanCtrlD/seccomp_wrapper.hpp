#pragma once

#include "communicator.h"

#include <cm_ctors.h>
#include <communicator_common.h>
#include <fcntl.h>
#include <linux/futex.h>
#include <seccomp.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <bits/types.h>
#include <cstddef>
#include <iostream>
#include <memory>
#include <ostream>
#include <type_traits>
#include <utility>
#include <vector>

/// This file/class defines allowed calls to the Linux Kernel. It should contain bare minimum
/// daemon's code needs to work. Everything else we block to prevent possible attacks.

class CSecCompWrapper
{
  public:
    NO_COPYMOVE(CSecCompWrapper);

    static std::shared_ptr<CSecCompWrapper> Allocate()
    {
        return std::shared_ptr<CSecCompWrapper>(new CSecCompWrapper());
    }

    ~CSecCompWrapper()
    {
        if (ctx)
        {
            seccomp_release(ctx);
            ctx = nullptr;
        }
    }

    /// @brief Tries to install rules for our daemon and engage it in kernel.
    /// @returns false if rules failed to engage.
    /// @note if it returned true there is no way to lower the rules.
    [[nodiscard]]
    bool Engage() const
    {
        if (!ctx)
        {
            std::cerr << "Kernel security context could not be created for libseccomp."
                      << std::endl;
            return false;
        }
        if (!InstallRules())
        {
            std::cerr << "Failed to load kernel security rules using libseccomp." << std::endl;
            return false;
        }
        return 0 == seccomp_load(ctx);
    }

  private:
    scmp_filter_ctx ctx{nullptr};

    CSecCompWrapper() :
        ctx(seccomp_init(SCMP_ACT_KILL_PROCESS))
    {
    }

    template <typename taType>
    static auto Condition(unsigned int argIndex, scmp_compare condition, taType value)
    {
        if constexpr (std::is_arithmetic_v<taType>)
        {
            if constexpr (sizeof(taType) == 4)
            {
                return SCMP_CMP32(argIndex, condition, value, 0u);
            }
            if constexpr (sizeof(taType) == 8)
            {
                return SCMP_CMP64(argIndex, condition, static_cast<scmp_datum_t>(value), 0u);
            }
        }
        else
        {
            static_assert(sizeof(taType) <= sizeof(scmp_datum_t));
            // NOLINTNEXTLINE
            return SCMP_CMP(argIndex, condition, reinterpret_cast<scmp_datum_t>(value), 0u);
        }
    }

    template <typename taType>
    static auto Equals(unsigned int argIndex, taType value)
    {
        return Condition<taType>(argIndex, scmp_compare::SCMP_CMP_EQ, value);
    }

    template <typename... taChecks>
    [[nodiscard]]
    bool InstallAllowRule(int syscall, taChecks &&...checks) const
    {
        const auto r = seccomp_rule_add(ctx, SCMP_ACT_ALLOW, syscall, sizeof...(checks),
                                        std::forward<taChecks>(checks)...);
        return r == 0;
    }

    ///@brief Applies rules we want.
    [[nodiscard]]
    bool InstallRules() const
    {
        if (ctx)
        {
            // TODO: Add more rules here.
            return InstallOpenAt() && InstallMMapUnmap() && InstallMProtect()
                   && InstallAllowRule(SCMP_SYS(fstat)) && InstallAllowRule(SCMP_SYS(write))
                   && InstallAllowRule(SCMP_SYS(read)) && InstallAllowRule(SCMP_SYS(close))

                   && InstallAllowRule(SCMP_SYS(unlink))
                   && InstallAllowRule(SCMP_SYS(fchmod), Equals<__mode_t>(1u, 0666))

                   && InstallAllowRule(SCMP_SYS(exit_group)) && InstallAllowRule(SCMP_SYS(exit))
                   && InstallAllowRule(SCMP_SYS(waitpid)) && InstallAllowRule(SCMP_SYS(waitid))
                   && InstallAllowRule(SCMP_SYS(wait4)) && InstallAllowRule(SCMP_SYS(futex))

                   && InstallAllowRule(SCMP_SYS(clock_nanosleep))
                   && InstallAllowRule(SCMP_SYS(nanosleep))
                   && InstallAllowRule(SCMP_SYS(rt_sigtimedwait))
                   && InstallAllowRule(SCMP_SYS(rt_sigprocmask))
                   && InstallAllowRule(SCMP_SYS(rt_sigaction))

                   && InstallAllowRule(SCMP_SYS(fallocate), Equals<int>(1u, 0),
                                       Equals<__off_t>(2u, 0),
                                       Equals<__off_t>(3u, kWholeSharedMemSize))
                   && InstallAllowRule(SCMP_SYS(ftruncate),
                                       Equals<__off_t>(1u, kWholeSharedMemSize));
        }
        return false;
    }

    [[nodiscard]]
    bool InstallMMapUnmap() const
    {
        bool res = true;

        const auto installCommMMap = [this](size_t len) {
            return InstallAllowRule(
              SCMP_SYS(mmap), Equals<void *>(0u, NULL), Equals<size_t>(1u, len),
              Equals<int>(2u, PROT_READ | PROT_WRITE), Equals<int>(3u, MAP_SHARED));
        };

        res = res && installCommMMap(kWholeSharedMemSize) && installCommMMap(kBackupSharedSize);
        res = res
              && InstallAllowRule(SCMP_SYS(mmap), Equals<void *>(0u, NULL),
                                  /*Skipping size at index 1*/
                                  Equals<int>(2u, PROT_NONE),
                                  Equals<int>(3u, MAP_PRIVATE | MAP_ANONYMOUS));

        res = res && InstallAllowRule(SCMP_SYS(munmap));
        return res;
    }

    [[nodiscard]]
    bool InstallMProtect() const
    {
        const auto install = [this](int prot) {
            return InstallAllowRule(SCMP_SYS(mprotect), Equals<int>(2u, prot));
        };

        return install(PROT_READ | PROT_WRITE) && install(PROT_READ) && install(PROT_WRITE);
    }

    [[nodiscard]]
    bool InstallOpenAt() const
    {
        const static std::vector<int> oflags = {
          O_RDONLY,
          O_RDONLY | O_CLOEXEC,
          O_WRONLY | O_CREAT | O_TRUNC,
          O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW | O_CLOEXEC,
          O_RDWR | O_NOFOLLOW | O_CLOEXEC,
          O_WRONLY | O_CREAT | O_TRUNC,
        };

        bool res = true;
        for (const int oflag : oflags)
        {
            res = res
                  && InstallAllowRule(SCMP_SYS(openat), Equals<int>(0u, AT_FDCWD),
                                      Equals<int>(2u, oflag));
            if (!res)
            {
                break;
            }
        }
        return res;
    }
};
