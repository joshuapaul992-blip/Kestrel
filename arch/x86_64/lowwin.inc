; =============================================================================
;  arch/x86_64/lowwin.inc  --  entry and exit for address spaces with a low
;  window (include/kernel/uvm.h)
;
;  The IDT, LSTAR, TSS stacks and the GS base all give UVM_DIRECT_BASE
;  addresses, which every root maps, so the first instructions of an entry
;  run there whatever CR3 is. From ring 3 in a process with a low window,
;  percpu kern_cr3 is its kernel root and the stack is given by its direct
;  address too: ENTRY_KERNEL_CR3 loads the one and moves the other back,
;  and ENTRY_KERNEL_RIP continues at the kernel's usual addresses. On the
;  way out EXIT_USER_CR3 does the reverse. Everywhere else kern_cr3 and
;  user_cr3 are 0 and only the jump remains.
; =============================================================================

%define UVM_DIRECT_BASE 0xFFFF800000000000
%define GS_KERN_CR3     32
%define GS_USER_CR3     40
%define GS_SCRATCH      48

; From ring 3, after SWAPGS. All registers kept.
%macro ENTRY_KERNEL_CR3 0
    mov     [gs:GS_SCRATCH], rax
    mov     rax, [gs:GS_KERN_CR3]
    test    rax, rax
    jz      %%done
    mov     cr3, rax
    mov     rax, UVM_DIRECT_BASE
    sub     rsp, rax
%%done:
    mov     rax, [gs:GS_SCRATCH]
%endmacro

; Off the direct view. All registers kept.
%macro ENTRY_KERNEL_RIP 0
    push    rax
    mov     rax, %%here
    jmp     rax
%%here:
    pop     rax
%endmacro

; To ring 3, interrupts off, before the registers are restored: clobbers
; RAX, RCX and RDX. The rest of the way runs on the direct view.
%macro EXIT_USER_CR3 0
    mov     rax, [gs:GS_USER_CR3]
    test    rax, rax
    jz      %%done
    mov     rcx, UVM_DIRECT_BASE
    add     rsp, rcx
    lea     rdx, [rel %%direct]
    add     rdx, rcx
    jmp     rdx
%%direct:
    mov     cr3, rax
%%done:
%endmacro
