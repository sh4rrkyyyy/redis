/**
 * x86_64 Calling Convention:
 * Arguments are passed via:
 *   - GP: %rdi, %rsi, %rdx, %rcx, %r8, %r9
 *   - FP: %xmm0, %xmm1, ..., %xmm7
 * Return value is passed via:
 *   - GP: %rax
 *   - FP: %xmm0, %xmm1
 * Callee preserves:
 *   - GP: %rbx, %rbp, %rsp, %r12, %r13, %r14, %r15
 * Others:
 *   - %r10 - Static Chain Pointer (?)
 *   - %r11 - Scratch Register
 *   - %xmm8, %xmm9, ..., %xmm15 - Scratch Registers
*/

#include "asm-macros.h"

    .global NAME(CactusContextEntry)
    .global NAME(CactusContextTrampoline)
    .global NAME(CactusContextActivate)

    .text

NAME(CactusContextTrampoline):
    TYPE_FUNCTION(CactusContextTrampoline)
    movq %rax, %rdi      /* CactusContextActivate returns suspended context in rax */
    jmp NAME(CactusContextEntry)
#ifndef __APPLE__
    .size NAME(CactusContextTrampoline), . - NAME(CactusContextTrampoline)
#endif

NAME(CactusContextActivate):
    TYPE_FUNCTION(CactusContextActivate)
    .cfi_startproc       /* CFI stands for call frame information */
    pushq %rbp
    .cfi_adjust_cfa_offset 8

    movq %rsp, %rbp
    .cfi_offset rbp, -16

    pushq %rbx
    .cfi_adjust_cfa_offset 8
    pushq %r12
    .cfi_adjust_cfa_offset 8
    pushq %r13
    .cfi_adjust_cfa_offset 8
    pushq %r14
    .cfi_adjust_cfa_offset 8
    pushq %r15
    .cfi_adjust_cfa_offset 8
    pushq %rsi           /* Save data */
    .cfi_adjust_cfa_offset 8

    .cfi_offset rbx, -0x18
    .cfi_offset r12, -0x20
    .cfi_offset r13, -0x28
    .cfi_offset r14, -0x30
    .cfi_offset r15, -0x38
    .cfi_offset rsi, -0x40

    movq %rsp, %rax      /* Save old SP */
    leaq 8(%rdi), %rsp   /* Load new SP and discard saved data. */
    .cfi_adjust_cfa_offset -8
    popq %r15
    .cfi_adjust_cfa_offset -8
    popq %r14
    .cfi_adjust_cfa_offset -8
    popq %r13
    .cfi_adjust_cfa_offset -8
    popq %r12
    .cfi_adjust_cfa_offset -8
    popq %rbx
    .cfi_adjust_cfa_offset -8

    popq %rbp
    .cfi_adjust_cfa_offset -8
    ret
    .cfi_endproc
#ifndef __APPLE__
    .size NAME(CactusContextActivate), . - NAME(CactusContextActivate)
    .section        .note.GNU-stack,"",@progbits
#endif
