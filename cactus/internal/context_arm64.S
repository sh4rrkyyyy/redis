/**
 * AArch64 Calling Convention:
 * Arguments are passed via:
 *   - GP: x0 to x7
 *   - FP: v0 to v7
 * Return value is passed via:
 *   - GP: x0
 *   - FP: v0, v1
 * Callee preserves:
 *   - GP: x19-x28, x29
 *   - FP: v8-v15
 * Others:
 *   - v0 to v7, x0 to x8 - Arguments
 *   - x8 - Indirect return value address
 *   - x9 to x15 - Scratch registers
 *   - x16, x17 - Intra-procedure-call scratch
 *   - x18 - Platform register
 *   - v16-v31 - Scratch FP registers
*/

#include "asm-macros.h"

    .global NAME(CactusContextEntry)
    .global NAME(CactusContextTrampoline)
    .global NAME(CactusContextActivate)

    .text

NAME(CactusContextTrampoline):
    TYPE_FUNCTION(CactusContextTrampoline)
    b NAME(CactusContextEntry)
#ifndef __APPLE__
    .size NAME(CactusContextTrampoline), . - NAME(CactusContextTrampoline)
#endif

// https://student.cs.uwaterloo.ca/~cs452/docs/rpi4b/aapcs64.pdf
NAME(CactusContextActivate):
    TYPE_FUNCTION(CactusContextActivate)
    .cfi_startproc             /* CFI stands for call frame information */
    sub sp, sp, #0xA0          /* sizeof(MachineContext) - 8 (8 bytes are added later when saving `x1`) */
    .cfi_def_cfa_offset 0xA0 
    .cfi_return_column x30

    stp x19, x20, [sp, #0x00]
    stp x21, x22, [sp, #0x10]
    stp x23, x24, [sp, #0x20]
    stp x25, x26, [sp, #0x30]
    stp x27, x28, [sp, #0x40]
    stp x29, x30, [sp, #0x50]
    stp  d8,  d9, [sp, #0x60]
    stp d10, d11, [sp, #0x70]
    stp d12, d13, [sp, #0x80]
    stp d14, d15, [sp, #0x90]

    str x1, [sp, #-0x08]!
    .cfi_adjust_cfa_offset 0x08
    .cfi_offset x1, -0xA8

    mov x1, x0

    .cfi_offset d15, -0x08
    .cfi_offset d14, -0x10
    .cfi_offset d13, -0x18
    .cfi_offset d12, -0x20
    .cfi_offset d11, -0x28
    .cfi_offset d10, -0x30
    .cfi_offset  d9, -0x38
    .cfi_offset  d8, -0x40

    .cfi_offset x30, -0x48
    .cfi_offset x29, -0x50
    .cfi_offset x28, -0x58
    .cfi_offset x27, -0x60
    .cfi_offset x26, -0x68
    .cfi_offset x25, -0x70
    .cfi_offset x24, -0x78
    .cfi_offset x23, -0x80
    .cfi_offset x22, -0x88
    .cfi_offset x21, -0x90
    .cfi_offset x20, -0x98
    .cfi_offset x19, -0xA0

    mov x0, sp                 /* Save old SP. */
    mov sp, x1                 /* Load new SP. */

    add sp, sp, #0x08
    .cfi_adjust_cfa_offset -0x08
    ldr x1, [sp, #-0x08]

    ldp x19, x20, [sp, #0x00]
    ldp x21, x22, [sp, #0x10]
    ldp x23, x24, [sp, #0x20]
    ldp x25, x26, [sp, #0x30]
    ldp x27, x28, [sp, #0x40]
    ldp x29, x30, [sp, #0x50]
    ldp  d8,  d9, [sp, #0x60]
    ldp d10, d11, [sp, #0x70]
    ldp d12, d13, [sp, #0x80]
    ldp d14, d15, [sp, #0x90]

    add sp, sp, #0xA0
    .cfi_def_cfa_offset 0x00

    ret

    .cfi_endproc
#ifndef __APPLE__
    .size NAME(CactusContextActivate), . - NAME(CactusContextActivate)
    .section .note.GNU-stack,"",@progbits
#endif
