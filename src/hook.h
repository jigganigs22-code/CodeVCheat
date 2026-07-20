#pragma once
// ARM64 inline hook engine for iOS dylib injection.
// No CydiaSubstrate/Substrate dependency — pure trampoline hooks.

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dlfcn.h>
#include <mach-o/dyld.h>

// --- Helpers ---

static inline uintptr_t get_image_base(void) {
    return (uintptr_t)_dyld_get_image_header(0);
}

static long get_page_size(void) {
    static long ps = 0;
    if (!ps) ps = sysconf(_SC_PAGESIZE);
    return ps;
}

static bool make_writable(void *addr, size_t size) {
    long ps = get_page_size();
    void *base = (void *)((uintptr_t)addr & ~(ps - 1));
    return mprotect(base, size + ((uintptr_t)addr - (uintptr_t)base), PROT_READ | PROT_WRITE | PROT_EXEC) == 0;
}

// --- ARM64 instruction encoding ---

static inline uint32_t arm64_nop(void) { return 0xD503201F; }

// B <offset> (unconditional branch, ±128MB)
static inline uint32_t arm64_b(void *from, void *to) {
    int64_t off = (int64_t)to - (int64_t)from;
    int32_t imm = (int32_t)(off / 4);
    return (uint32_t)((0x14u << 26) | (imm & 0x03FFFFFF));
}

// MOV Wd, #imm16 (zero-extend)
static inline uint32_t arm64_mov_w(uint8_t rd, uint32_t imm16) {
    return (uint32_t)((0x52800000) | ((uint32_t)rd << 5) | (imm16 & 0xFFFF));
}

// MOVZ Wd, #imm16, LSL#hw*16
static inline uint32_t arm64_movz_w(uint8_t rd, uint16_t imm16, uint8_t hw) {
    return (uint32_t)(0x52800000 | ((uint32_t)hw << 21) | ((uint32_t)imm16 << 5) | rd);
}

// MOVK Wd, #imm16, LSL#hw*16
static inline uint32_t arm64_movk_w(uint8_t rd, uint16_t imm16, uint8_t hw) {
    return (uint32_t)(0x72800000 | ((uint32_t)hw << 21) | ((uint32_t)imm16 << 5) | rd);
}

// --- Trampoline hook engine ---

#define MAX_HOOKS 48

typedef struct {
    void    *target;
    void    *hook;
    void    *trampoline;
    uint32_t orig[8];   // saved original instructions (up to 32 bytes)
    int      orig_size; // bytes saved
} hook_entry_t;

static hook_entry_t g_hooks[MAX_HOOKS];
static int g_hook_count = 0;

// Install an inline hook. Saves first `save_count` instructions (4 bytes each)
// to a trampoline, replaces them with B to hook_fn.
// out_original: if non-NULL, receives pointer to trampoline (call orig via trampoline).
// If save_count is 0, no trampoline is created (complete replacement).
static bool hook_install(void *target, void *hook_fn, void **out_original, int save_count) {
    if (g_hook_count >= MAX_HOOKS) return false;
    if (save_count < 0 || save_count > 7) return false;

    size_t save_bytes = save_count * 4;
    hook_entry_t *e = &g_hooks[g_hook_count];
    e->target = target;
    e->hook   = hook_fn;
    e->orig_size = save_bytes;
    memcpy(e->orig, target, save_bytes);

    if (save_count > 0) {
        // Allocate trampoline: saved instructions + B back
        size_t tramp_size = save_bytes + 4; // +4 for the B back instruction
        e->trampoline = mmap(NULL, tramp_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (e->trampoline == MAP_FAILED) return false;

        memcpy(e->trampoline, e->orig, save_bytes);
        uint32_t *tramp = (uint32_t *)((char *)e->trampoline + save_bytes);
        *tramp = arm64_b(tramp, (char *)target + save_bytes); // branch back past hook
    } else {
        e->trampoline = NULL;
    }

    // Make target writable
    make_writable(target, save_bytes > 0 ? save_bytes : 4);

    // Write B hook_fn + NOPs
    uint32_t *tgt = (uint32_t *)target;
    tgt[0] = arm64_b(target, hook_fn);
    for (int i = 1; i < save_count || i < 4; i++) {
        if (i < save_count) {
            tgt[i] = arm64_nop(); // NOP out the saved instructions (they're in trampoline)
        } else if (i < 4) {
            tgt[i] = arm64_nop(); // pad to at least 4 instructions (B + 3 NOPs)
        }
    }

    if (out_original) *out_original = e->trampoline;
    g_hook_count++;
    return true;
}

// Simpler: directly patch N instructions at target address (no trampoline).
// Used for complete replacements (return constant, etc.)
static void patch_instructions(void *target, const uint32_t *insns, int count) {
    make_writable(target, count * 4);
    memcpy(target, insns, count * 4);
}
