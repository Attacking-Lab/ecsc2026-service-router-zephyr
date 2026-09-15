/*
 * Copyright (c) 2016 Cadence Design Systems, Inc.
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/arch/cpu.h>
#include <zephyr/kernel_structs.h>
#include <inttypes.h>
#include <xtensa/config/specreg.h>
#include <xtensa_backtrace.h>
#include <zephyr/arch/common/exc_handle.h>

#include <xtensa_internal.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(os, CONFIG_KERNEL_LOG_LEVEL);

#if defined(CONFIG_SIMULATOR_XTENSA) || defined(XT_SIMULATOR)
#include <xtensa/simcall.h>
#endif

char *xtensa_exccause(unsigned int cause_code)
{
#if defined(CONFIG_PRINTK) || defined(CONFIG_LOG)
	switch (cause_code) {
	case 0:
		return "illegal instruction";
	case 1:
		return "syscall";
	case 2:
		return "instr fetch error";
	case 3:
		return "load/store error";
	case 4:
		return "level-1 interrupt";
	case 5:
		return "alloca";
	case 6:
		return "divide by zero";
	case 8:
		return "privileged";
	case 9:
		return "load/store alignment";
	case 12:
		return "instr PIF data error";
	case 13:
		return "load/store PIF data error";
	case 14:
		return "instr PIF addr error";
	case 15:
		return "load/store PIF addr error";
	case 16:
		return "instr TLB miss";
	case 17:
		return "instr TLB multi hit";
	case 18:
		return "instr fetch privilege";
	case 20:
		return "inst fetch prohibited";
	case 24:
		return "load/store TLB miss";
	case 25:
		return "load/store TLB multi hit";
	case 26:
		return "load/store privilege";
	case 28:
		return "load prohibited";
	case 29:
		return "store prohibited";
	case 32: case 33: case 34: case 35: case 36: case 37: case 38: case 39:
		return "coprocessor disabled";
	case 63:
		/* i.e. z_except_reason */
		return "zephyr exception";
	case 64:
		return "kernel oops";
	default:
		return "unknown/reserved";
	}
#else
	ARG_UNUSED(cause_code);
	return "na";
#endif
}

void xtensa_fatal_error(unsigned int reason, const struct arch_esf *esf)
{
#ifdef CONFIG_EXCEPTION_DEBUG
	if (esf != NULL) {
		/* Don't want to get elbowed by xtensa_switch
		 * in between printing registers and dumping them;
		 * corrupts backtrace
		 */
		unsigned int key = arch_irq_lock();

		xtensa_dump_stack(esf);


#if defined(CONFIG_XTENSA_ENABLE_BACKTRACE)
#if XCHAL_HAVE_WINDOWED
		xtensa_backtrace_print(100, (int *)esf);
#endif
#endif
		arch_irq_unlock(key);
	}
#endif /* CONFIG_EXCEPTION_DEBUG */

	z_fatal_error(reason, esf);
}

#ifdef CONFIG_XTENSA_USER_FAULT_BAILOUT
/*
 * Tail of the unrecoverable exception path (see _xtensa_unrecoverable).
 *
 * Entered with a fresh register window, interrupts masked and a stack of our
 * own, with the user thread that could not be recovered still current. There
 * is no interrupted context left to describe, hence the NULL ESF, and nothing
 * to return to: z_fatal_error() terminates the thread and the scheduler moves
 * on to another one.
 */
FUNC_NORETURN void xtensa_user_fault_bailout(void)
{
#if XCHAL_HAVE_THREADPTR && defined(CONFIG_THREAD_LOCAL_STORAGE)
	extern Z_THREAD_LOCAL uint32_t is_user_mode;

	/* The thread was in user mode and its TLS still says so, the way the
	 * syscall entry clears it. Kernel code called on its behalf below
	 * (the fatal path prints) would otherwise take the user-mode route
	 * and issue syscalls from kernel mode.
	 */
	is_user_mode = 0;
#endif

	/* Not in interrupt context any more as far as the kernel is
	 * concerned, so the fatal path aborts the current thread instead of
	 * arranging a return into it.
	 */
	arch_curr_cpu()->nested = 0;

	z_fatal_error(K_ERR_CPU_EXCEPTION, NULL);

	CODE_UNREACHABLE;
}
#endif /* CONFIG_XTENSA_USER_FAULT_BAILOUT */

#if defined(CONFIG_SIMULATOR_XTENSA) || defined(XT_SIMULATOR)
void xtensa_simulator_exit(int return_code)
{
	__asm__ (
	    "mov a3, %[code]\n\t"
	    "movi a2, %[call]\n\t"
	    "simcall\n\t"
	    :
	    : [code] "r" (return_code), [call] "i" (SYS_exit)
	    : "a3", "a2", "memory");

	CODE_UNREACHABLE;
}

FUNC_NORETURN void arch_system_halt(unsigned int reason)
{
	xtensa_simulator_exit(255 - reason);
	CODE_UNREACHABLE;
}
#endif

FUNC_NORETURN void arch_syscall_oops(void *ssf)
{
	xtensa_arch_kernel_oops(K_ERR_KERNEL_OOPS, ssf);

	CODE_UNREACHABLE;
}

#ifdef CONFIG_USERSPACE
void z_impl_xtensa_user_fault(unsigned int reason)
{
	if ((_current->base.user_options & K_USER) != 0) {
		if ((reason != K_ERR_KERNEL_OOPS) &&
				(reason != K_ERR_STACK_CHK_FAIL)) {
			reason = K_ERR_KERNEL_OOPS;
		}
	}
	xtensa_arch_except(reason);
}

static void z_vrfy_xtensa_user_fault(unsigned int reason)
{
	z_impl_xtensa_user_fault(reason);
}

#include <zephyr/syscalls/xtensa_user_fault_mrsh.c>

#endif /* CONFIG_USERSPACE */
