; =============================================================================
;  arch/x86_64/uaccess.asm  --  fault-tolerant access to user memory
;
;  Every instruction here that touches a user address is listed in the
;  exception table (section .ex_table: pairs of <faulting RIP, fixup RIP>).
;  When such an instruction faults, the page-fault handler (idt.c) first
;  tries to resolve the fault (demand paging, copy-on-write); if it cannot,
;  it resumes at the fixup address instead of treating the fault as a kernel
;  bug. The C wrappers in kernel/uaccess.c check the address range first, so
;  only user addresses ever reach these routines.
;
;  SysV ABI: rdi, rsi, rdx, rcx = arguments, rax = result.
; =============================================================================

bits 64
section .text

%macro EXTABLE 2
    section .ex_table
    align 8
    dq %1, %2
    section .text
%endmacro

; -----------------------------------------------------------------------------
; size_t uaccess_copy(void *dst, const void *src, size_t n)
; Returns the number of bytes NOT copied (0 on success). One of dst/src is a
; user address. Copies of 64 bytes or more align dst to 8 with `rep movsb`,
; move the bulk with `rep movsq` and the tail with `rep movsb`: QEMU's
; software CPU runs `rep movsb` a byte at a time, and real CPUs are as fast
; or faster with quadwords. Each string instruction leaves rcx = units left
; when it faults; the fixups turn that into bytes left. A fault in the middle
; of a quadword can report up to 7 bytes too few as copied.
; -----------------------------------------------------------------------------
global uaccess_copy
uaccess_copy:
    mov     rcx, rdx
    cmp     rdx, 64
    jb      .bytes
    mov     rcx, rdi
    neg     rcx
    and     ecx, 7                      ; head: bytes up to dst's alignment
    sub     rdx, rcx
.head:
    rep movsb
    mov     rcx, rdx
    shr     rcx, 3
    and     edx, 7
.quads:
    rep movsq
    mov     rcx, rdx
.bytes:
    rep movsb
    xor     eax, eax
    ret
.fault_head:
    lea     rax, [rcx + rdx]
    ret
.fault_quads:
    lea     rax, [rdx + rcx * 8]
    ret
.fault_bytes:
    mov     rax, rcx
    ret
    EXTABLE uaccess_copy.head, uaccess_copy.fault_head
    EXTABLE uaccess_copy.quads, uaccess_copy.fault_quads
    EXTABLE uaccess_copy.bytes, uaccess_copy.fault_bytes

; -----------------------------------------------------------------------------
; size_t uaccess_clear(void *dst, size_t n): zero user memory; bytes NOT cleared
; (quadwords for the bulk, as uaccess_copy)
; -----------------------------------------------------------------------------
global uaccess_clear
uaccess_clear:
    mov     rcx, rsi
    xor     eax, eax
    cmp     rsi, 64
    jb      .bytes
    mov     rcx, rdi
    neg     rcx
    and     ecx, 7
    sub     rsi, rcx
.head:
    rep stosb
    mov     rcx, rsi
    shr     rcx, 3
    and     esi, 7
.quads:
    rep stosq
    mov     rcx, rsi
.bytes:
    rep stosb
    ret                                 ; rax = 0
.fault_head:
    lea     rax, [rcx + rsi]
    ret
.fault_quads:
    lea     rax, [rsi + rcx * 8]
    ret
.fault_bytes:
    mov     rax, rcx
    ret
    EXTABLE uaccess_clear.head, uaccess_clear.fault_head
    EXTABLE uaccess_clear.quads, uaccess_clear.fault_quads
    EXTABLE uaccess_clear.bytes, uaccess_clear.fault_bytes

; -----------------------------------------------------------------------------
; int64_t uaccess_strncpy(char *dst, const char *usrc, size_t max)
; Copies up to and including the NUL. Returns the string length (without the
; NUL), `max` if no NUL was found in the first max bytes (dst then holds max
; bytes, unterminated), or -1 on a fault.
; -----------------------------------------------------------------------------
global uaccess_strncpy
uaccess_strncpy:
    xor     eax, eax
.loop:
    cmp     rax, rdx
    jae     .done
.load:
    movzx   ecx, byte [rsi + rax]
    mov     [rdi + rax], cl
    test    cl, cl
    jz      .done
    inc     rax
    jmp     .loop
.done:
    ret
.fault:
    mov     rax, -1
    ret
    EXTABLE uaccess_strncpy.load, uaccess_strncpy.fault

; -----------------------------------------------------------------------------
; int uaccess_get32(const uint32_t *uaddr, uint32_t *out)   0 or -1
; int uaccess_get64(const uint64_t *uaddr, uint64_t *out)
; int uaccess_put32(uint32_t *uaddr, uint32_t v)
; int uaccess_put64(uint64_t *uaddr, uint64_t v)
; -----------------------------------------------------------------------------
global uaccess_get32
uaccess_get32:
.ld:
    mov     eax, [rdi]
    mov     [rsi], eax
    xor     eax, eax
    ret
.fault:
    mov     eax, -1
    ret
    EXTABLE uaccess_get32.ld, uaccess_get32.fault

global uaccess_get64
uaccess_get64:
.ld:
    mov     rax, [rdi]
    mov     [rsi], rax
    xor     eax, eax
    ret
.fault:
    mov     eax, -1
    ret
    EXTABLE uaccess_get64.ld, uaccess_get64.fault

global uaccess_put32
uaccess_put32:
.st:
    mov     [rdi], esi
    xor     eax, eax
    ret
.fault:
    mov     eax, -1
    ret
    EXTABLE uaccess_put32.st, uaccess_put32.fault

global uaccess_put64
uaccess_put64:
.st:
    mov     [rdi], rsi
    xor     eax, eax
    ret
.fault:
    mov     eax, -1
    ret
    EXTABLE uaccess_put64.st, uaccess_put64.fault

; -----------------------------------------------------------------------------
; int uaccess_cmpxchg32(uint32_t *uaddr, uint32_t old, uint32_t new, uint32_t *cur)
; Atomic compare-and-exchange on user memory (futexes, robust lists).
; *cur = the value found. Returns 0 (whether or not it matched) or -1 on a
; fault. `lock cmpxchg` is a full barrier on x86.
; -----------------------------------------------------------------------------
global uaccess_cmpxchg32
uaccess_cmpxchg32:
    mov     eax, esi
.op:
    lock cmpxchg [rdi], edx
    mov     [rcx], eax
    xor     eax, eax
    ret
.fault:
    mov     eax, -1
    ret
    EXTABLE uaccess_cmpxchg32.op, uaccess_cmpxchg32.fault
