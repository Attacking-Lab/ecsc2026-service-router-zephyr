/*
 * Copyright (c) 2026 Intel Corporation
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef ZEPHYR_ARCH_XTENSA_INCLUDE_XTENSA_SPILL_BOUNDS_H_
#define ZEPHYR_ARCH_XTENSA_INCLUDE_XTENSA_SPILL_BOUNDS_H_

#include <zephyr/zsr.h>

/*
 * Bounds on register window spill/fill targets.
 *
 * The window vectors move registers through stack pointers taken from the
 * interrupted context, and they run with PS.EXCM set, i.e. with kernel
 * privilege. A thread that controls its own registers therefore controls
 * the address of a privileged load or store. These two registers hold the
 * address range the running context is allowed to spill into; they are
 * loaded when a thread is resumed (see _restore_context) and cleared while
 * running on the interrupt stack (see CROSS_STACK_CALL).
 *
 * A zero ZSR_SPILL_HI means "no bounds established" and disables the check,
 * so an unset or not-yet-initialised context behaves exactly as before.
 */
#define ZSR_SPILL_LO	ZSR_EXTRA0
#define ZSR_SPILL_HI	ZSR_EXTRA1
#define ZSR_SPILL_TMP	ZSR_EXTRA2

#ifdef _ASMLANGUAGE

/*
 * Refuse a spill/fill through \base unless it lands inside the running
 * context's stack.
 *
 * \scratch is clobbered, so it must be a register that is dead at this point
 * (in the overflow handlers nothing is dead, so the caller stashes one in
 * ZSR_SPILL_TMP around the check). Only special registers are read, so the
 * check itself can never fault - which matters, because a fault here is a
 * double exception.
 *
 * A violation branches to \bad, which every user of this macro points at
 * _xtensa_unrecoverable (the branch range is short, so it is a local label
 * next to a jump rather than the symbol itself).
 */
.macro SPILL_BOUNDS_CHECK base, scratch, bad
	rsr	\scratch, ZSR_SPILL_HI
	beqz	\scratch, 8f
	bgeu	\base, \scratch, \bad
	rsr	\scratch, ZSR_SPILL_LO
	bltu	\base, \scratch, \bad
8:
.endm

#endif /* _ASMLANGUAGE */

#endif /* ZEPHYR_ARCH_XTENSA_INCLUDE_XTENSA_SPILL_BOUNDS_H_ */
