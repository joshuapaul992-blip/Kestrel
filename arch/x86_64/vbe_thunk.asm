; =============================================================================
;  arch/x86_64/vbe_thunk.asm  --  one BIOS video call from a halted kernel
;
;  The kernel panic screen (kernel/stopscreen.c) switches a BIOS-booted
;  machine's boot frame buffer to 800x600 the only way such a frame buffer
;  can change mode: through the video BIOS (VBE), which runs in real mode.
;  vbe.c copies this blob to the AP start-up page (0x8000, free once the
;  other CPUs are up; during a panic they are stopped) and calls it:
;
;      long mode -> 32-bit compatibility mode -> paging off, long mode off ->
;      16-bit protected mode -> real mode: VBE 4F00 (mode list), 4F01 for
;      each mode until an 800x600 direct-colour mode with a linear frame
;      buffer turns up (32 bpp preferred, else 24), 4F02 to set it ->
;      protected mode -> long mode, kernel GDT/IDT/stack/segment bases back
;
;  It returns 1 with the chosen mode's ModeInfoBlock at VBE_MODE_INFO, or 0
;  (no VBE 2.0, no such mode, or the BIOS refused). Interrupts stay off as
;  far as this code is concerned; vbe.c has masked every source first.
;  Absolute addresses are computed from VBE_BASE: the blob runs there only.
; =============================================================================

VBE_BASE      equ 0x8000
VBE_STACK     equ VBE_BASE               ; stack: down into free conventional memory (0x500-0x7FFF)
VBE_INFO      equ VBE_BASE + 0xC00       ; VbeInfoBlock, 512 bytes
VBE_MODE_INFO equ VBE_BASE + 0xE00       ; ModeInfoBlock, 256 bytes
%define V(x) (VBE_BASE + (x) - vbe_start)

global vbe_thunk_start, vbe_thunk_end

section .text
bits 64
vbe_thunk_start:
vbe_start:
    push    rbx
    push    rbp
    push    r12
    push    r13
    push    r14
    push    r15
    pushfq
    cli
    mov     [V(sv_rsp)], rsp
    sgdt    [V(sv_gdt)]
    sidt    [V(sv_idt)]
    mov     ax, cs
    mov     [V(sv_cs)], ax
    mov     ax, ss
    mov     [V(sv_ss)], ax
    mov     ax, ds
    mov     [V(sv_ds)], ax
    mov     ax, es
    mov     [V(sv_es)], ax
    mov     rax, cr0
    mov     [V(sv_cr0)], rax
    mov     rax, cr3
    mov     [V(sv_cr3)], rax
    mov     rax, cr4
    mov     [V(sv_cr4)], rax
    mov     ecx, 0xC0000080             ; EFER
    rdmsr
    mov     [V(sv_efer)], eax
    mov     [V(sv_efer) + 4], edx
    mov     ecx, 0xC0000100             ; FS base
    rdmsr
    mov     [V(sv_fsbase)], eax
    mov     [V(sv_fsbase) + 4], edx
    mov     ecx, 0xC0000101             ; GS base: loading GS in real mode overwrites it
    rdmsr
    mov     [V(sv_gsbase)], eax
    mov     [V(sv_gsbase) + 4], edx
    mov     rax, cr4
    and     rax, ~(1 << 17)             ; PCIDE must be off before paging can be
    mov     cr4, rax
    lgdt    [V(gdt_ptr)]
    push    0x08
    mov     rax, V(pm32)
    push    rax
    retfq                               ; -> 32-bit compatibility mode

bits 32
pm32:
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax
    mov     esp, VBE_STACK
    mov     eax, cr0
    and     eax, 0x7FFFFFFF             ; paging off: long mode becomes inactive
    mov     cr0, eax
    mov     ecx, 0xC0000080
    rdmsr
    and     eax, ~(1 << 8)              ; LME off
    wrmsr
    jmp     0x18:V(pm16)

bits 16
pm16:
    mov     ax, 0x20                    ; 16-bit data, 64 KiB limit: real-mode-ready caches
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax
    mov     eax, cr0
    and     eax, ~1                     ; PE off
    mov     cr0, eax
    jmp     0:V(rm)

rm:
    xor     ax, ax
    mov     ds, ax
    mov     es, ax
    mov     fs, ax
    mov     gs, ax
    mov     ss, ax
    mov     sp, VBE_STACK
    lidt    [V(rm_idt)]
    mov     word [V(result)], 0
    mov     word [V(best_mode)], 0xFFFF
    mov     byte [V(best_bpp)], 0

    mov     di, VBE_INFO                ; 4F00: controller info, asking for VBE 2.0+
    mov     dword [di], 'VBE2'
    mov     ax, 0x4F00
    int     0x10
    cmp     ax, 0x004F
    jne     .done
    mov     ax, [VBE_INFO + 14]         ; mode list: far pointer
    mov     [V(list_off)], ax
    mov     ax, [VBE_INFO + 16]
    mov     [V(list_seg)], ax

.next:
    mov     ax, [V(list_seg)]
    mov     fs, ax
    mov     si, [V(list_off)]
    mov     cx, [fs:si]
    cmp     cx, 0xFFFF
    je      .chosen
    add     word [V(list_off)], 2
    mov     [V(cur_mode)], cx
    xor     ax, ax
    mov     es, ax
    mov     di, VBE_MODE_INFO
    mov     ax, 0x4F01
    int     0x10
    cmp     ax, 0x004F
    jne     .next
    mov     ax, [VBE_MODE_INFO + 0]     ; attributes: supported, colour, graphics, LFB
    and     ax, 0x99
    cmp     ax, 0x99
    jne     .next
    cmp     word [VBE_MODE_INFO + 0x12], 800
    jne     .next
    cmp     word [VBE_MODE_INFO + 0x14], 600
    jne     .next
    cmp     byte [VBE_MODE_INFO + 0x1B], 6      ; direct colour
    jne     .next
    mov     al, [VBE_MODE_INFO + 0x19]  ; bits per pixel
    cmp     al, 32
    je      .take
    cmp     al, 24
    jne     .next
    cmp     byte [V(best_bpp)], 0
    jne     .next                       ; already have one
.take:
    mov     [V(best_bpp)], al
    mov     cx, [V(cur_mode)]
    mov     [V(best_mode)], cx
    cmp     al, 32
    jne     .next                       ; keep looking for a 32-bpp one
.chosen:
    mov     cx, [V(best_mode)]
    cmp     cx, 0xFFFF
    je      .done
    xor     ax, ax
    mov     es, ax
    mov     di, VBE_MODE_INFO           ; its ModeInfoBlock again, for the caller
    mov     ax, 0x4F01
    int     0x10
    cmp     ax, 0x004F
    jne     .done
    mov     bx, [V(best_mode)]
    or      bx, 0x4000                  ; linear frame buffer
    mov     ax, 0x4F02
    int     0x10
    cmp     ax, 0x004F
    jne     .done
    mov     word [V(result)], 1

.done:
    cli
    xor     ax, ax
    mov     ds, ax
    o32 lgdt [V(gdt_ptr)]
    mov     eax, cr0
    or      eax, 1                      ; PE
    mov     cr0, eax
    jmp     dword 0x08:V(pm32_back)

bits 32
pm32_back:
    mov     ax, 0x10
    mov     ds, ax
    mov     es, ax
    mov     ss, ax
    mov     eax, [V(sv_cr4)]
    and     eax, ~(1 << 17)             ; PCIDE comes back in long mode
    mov     cr4, eax
    mov     eax, [V(sv_cr3)]
    and     eax, 0xFFFFF000
    mov     cr3, eax
    mov     ecx, 0xC0000080
    mov     eax, [V(sv_efer)]
    mov     edx, [V(sv_efer) + 4]
    wrmsr
    mov     eax, [V(sv_cr0)]            ; PG back on: long mode active again
    mov     cr0, eax
    jmp     0x28:V(lm64)

bits 64
lm64:
    lgdt    [V(sv_gdt)]
    lidt    [V(sv_idt)]
    mov     rax, [V(sv_cr4)]
    mov     cr4, rax
    mov     rax, [V(sv_cr3)]
    mov     cr3, rax
    mov     ax, [V(sv_ss)]
    mov     ss, ax
    mov     ax, [V(sv_ds)]
    mov     ds, ax
    mov     ax, [V(sv_es)]
    mov     es, ax
    xor     eax, eax
    mov     fs, ax
    mov     gs, ax
    mov     ecx, 0xC0000100
    mov     eax, [V(sv_fsbase)]
    mov     edx, [V(sv_fsbase) + 4]
    wrmsr
    mov     ecx, 0xC0000101
    mov     eax, [V(sv_gsbase)]
    mov     edx, [V(sv_gsbase) + 4]
    wrmsr
    mov     rsp, [V(sv_rsp)]
    movzx   eax, word [V(sv_cs)]
    push    rax
    mov     rax, V(lm_kernel_cs)
    push    rax
    retfq                               ; kernel CS again
lm_kernel_cs:
    movzx   eax, word [V(result)]
    popfq
    pop     r15
    pop     r14
    pop     r13
    pop     r12
    pop     rbp
    pop     rbx
    ret

align 8
gdt:
    dq 0
    dq 0x00CF9A000000FFFF               ; 0x08 32-bit code
    dq 0x00CF92000000FFFF               ; 0x10 32-bit data
    dq 0x00009A000000FFFF               ; 0x18 16-bit code, 64 KiB
    dq 0x000092000000FFFF               ; 0x20 16-bit data, 64 KiB
    dq 0x00AF9A000000FFFF               ; 0x28 64-bit code
gdt_ptr:
    dw 6 * 8 - 1
    dq V(gdt)                           ; lgdt in 64-bit mode reads 8 bytes of base
rm_idt:
    dw 0x3FF
    dd 0
align 8
sv_rsp:     dq 0
sv_cr0:     dq 0
sv_cr3:     dq 0
sv_cr4:     dq 0
sv_efer:    dq 0
sv_fsbase:  dq 0
sv_gsbase:  dq 0
sv_gdt:     dw 0
            dq 0
sv_idt:     dw 0
            dq 0
sv_cs:      dw 0
sv_ss:      dw 0
sv_ds:      dw 0
sv_es:      dw 0
result:     dw 0
best_mode:  dw 0
cur_mode:   dw 0
list_off:   dw 0
list_seg:   dw 0
best_bpp:   db 0
vbe_thunk_end:
