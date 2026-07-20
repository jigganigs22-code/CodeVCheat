// CodeV Internal Cheat — 9 Features
// Built for iOS 18+ no-jailbreak (embedded dylib via insert_dylib / ESign / etc.)
// Compile: clang -arch arm64 -dynamiclib -isysroot $(xcrun --sdk iphoneos --show-sdk-path) -o libcheat.dylib cheat.c

#include <stdio.h>
#include <dispatch/dispatch.h>
#include <objc/objc.h>
#include <objc/message.h>

#include "offsets.h"
#include "hook.h"

// ============================================================================
// FEATURE 1: NO RECOIL
// ============================================================================
// sub_10322D168: returns recoil from this+0x144 as float
// Original: LDR S0,[X0,#0x144]; LDR S1,[X0,#0x148]; RET
// Patch: MOV W0,#0; RET; NOP; NOP  (returns 0.0f = zero recoil)
static void patch_no_recoil(void) {
    uint32_t *fn = (uint32_t *)(get_image_base() + OFF_GET_RECOIL);
    uint32_t patch[] = {
        0x52800000, // MOV W0, #0
        0xD65F03C0, // RET
        0xD503201F, // NOP
        0xD503201F, // NOP
    };
    patch_instructions(fn, patch, 4);
    printf("[cheat] No Recoil: patched GetRecoil @ %p\n", fn);
}

// ============================================================================
// FEATURE 2: ESP (WALLHACK — ALL ACTORS VISIBLE)
// ============================================================================
// sub_10243C524: calls vtable[149], stores result bool to *a3 (X2)
// We force *a3 = 0 (not hidden = visible)
// Patch: STRB WZR,[X2]; RET; NOP; NOP
static void patch_esp(void) {
    uint32_t *fn = (uint32_t *)(get_image_base() + OFF_IS_HIDDEN);
    uint32_t patch[] = {
        0x3900005F, // STRB WZR, [X2]
        0xD65F03C0, // RET
        0xD503201F, // NOP
        0xD503201F, // NOP
    };
    patch_instructions(fn, patch, 4);
    printf("[cheat] ESP: patched IsHidden @ %p\n", fn);
}

// ============================================================================
// FEATURE 3: NO SPREAD
// ============================================================================
// sub_10709721C: float spread_calc(float *weaponData, int aiming)
// Returns spread angle in S0. We return 0.0f.
static void patch_no_spread(void) {
    uint32_t *fn = (uint32_t *)(get_image_base() + OFF_SPREAD_CALC);
    uint32_t patch[] = {
        0x52800000, // MOV W0, #0  (S0 = 0.0f)
        0xD65F03C0, // RET
        0xD503201F, // NOP
        0xD503201F, // NOP
    };
    patch_instructions(fn, patch, 4);
    printf("[cheat] No Spread: patched SpreadCalc @ %p\n", fn);
}

// ============================================================================
// FEATURE 4: SPEED FIRE
// ============================================================================
// sub_1022F6C10: float RandomFireDelay(float min, float max)
// Returns random float between min and max.
// We force return 0.01f (near-instant between shots).
// 0.01f = 0x3C23D70A → MOVZ W8,#0xD70A; MOVK W8,#0x3C23,LSL#16
// We just return the float via W0.
static void patch_speed_fire(void) {
    uint32_t *fn = (uint32_t *)(get_image_base() + OFF_RANDOM_FIRE_DELAY);
    // 0.01f IEEE754 = 0x3C23D70A. Build in W0 (alias S0 for return).
    uint32_t patch[] = {
        arm64_movz_w(0, 0xD70A, 0), // MOVZ W0, #0xD70A
        arm64_movk_w(0, 0x3C23, 1), // MOVK W0, #0x3C23, LSL#16
        0xD65F03C0,                   // RET
        0xD503201F,                   // NOP
    };
    patch_instructions(fn, patch, 4);
    printf("[cheat] Speed Fire: patched RandomFireDelay @ %p\n", fn);
}

// ============================================================================
// FEATURE 5: SPEED HACK
// ============================================================================
// sub_10263C97C: PropertySetter(entity, FName, flags, float_value)
// Called with float_value=1.0f for MoveSpeed and DashSpeedMultiplier.
// We hook to change 1.0f → 2.0f (doubles movement speed).
static void *(*orig_property_setter)(void *, void *, int, float) = NULL;

static void *hook_property_setter(void *entity, void *fname, int flags, float value) {
    if (value == 1.0f) {
        value = 2.0f; // double speed
    }
    return orig_property_setter(entity, fname, flags, value);
}

static void patch_speed_hack(void) {
    void *target = (void *)(get_image_base() + OFF_PROPERTY_SETTER);
    hook_install(target, (void *)hook_property_setter, (void **)&orig_property_setter, 4);
    printf("[cheat] Speed Hack: hooked PropertySetter @ %p\n", target);
}

// ============================================================================
// FEATURE 6: SPINBOT
// ============================================================================
// sub_106A3FE28: AddControllerYawInput(controller, input_data)
// At the end: vtable[0x7F0](controller, yaw_float)
// We hook to call original, then add constant spin via same vtable slot.
// Spin speed: 3.0 degrees/tick (0x40400000)
static void (*orig_yaw_input)(void *, void *) = NULL;

// Global: keep controller pointer for spin ticks
static void *g_controller = NULL;

static void hook_yaw_input(void *controller, void *input_data) {
    orig_yaw_input(controller, input_data);
    g_controller = controller;

    // Add spin: call vtable[0x7F0/8] with spin constant
    void **vtable = *(void ***)controller;
    typedef void (*yaw_fn)(void *, float);
    yaw_fn add_yaw = (yaw_fn)vtable[VTABLE_OFFSET_YAW / 8];
    if (add_yaw) {
        add_yaw(controller, 3.0f); // 3 degrees per tick spin
    }
}

static void patch_spinbot(void) {
    void *target = (void *)(get_image_base() + OFF_YAW_INPUT);
    hook_install(target, (void *)hook_yaw_input, (void **)&orig_yaw_input, 4);
    printf("[cheat] Spinbot: hooked YawInput @ %p\n", target);
}

// ============================================================================
// FEATURE 7: AIMLOCK — HEAD SNAPPING (with visibility check)
// ============================================================================
// How it works:
//   1. Game's aim assist finds nearest entity during fire input
//   2. Visibility gate (sub_102398158) checks if target is visible
//      — we DO NOT disable this — prevents aiming through walls
//   3. AimCore (sub_10239460C) pushes look-at command with target position
//   4. Our hook intercepts AimCore and adds Z+90 offset → aims at HEAD
//
// Result: when firing, aim snaps to nearest VISIBLE enemy's head.
// Visibility gate stays active → no wall-snapping.
// Fire detection is built-in → only activates when shooting.

static void (*orig_aim_core)(void *, void *, void *, float *, void *, int) = NULL;

static void hook_aim_core(void *a1, void *a2, void *a3, float *aim_pos, void *a5, int a6) {
    if (aim_pos) {
        aim_pos[2] += 90.0f;
    }
    orig_aim_core(a1, a2, a3, aim_pos, a5, a6);
}

static void patch_aimlock(void) {
    void *target = (void *)(get_image_base() + OFF_AIM_CORE);
    hook_install(target, (void *)hook_aim_core, (void **)&orig_aim_core, 4);
    printf("[cheat] Aimlock: hooked AimCore (head snap) @ %p\n", target);
    printf("[cheat] Aimlock: visibility gate left intact — no wall aiming\n");
}

// ============================================================================
// FEATURE 8: ANTI-BAN (Jailbreak Detection Bypass)
// ============================================================================
// sub_10020FC04: -[UIDevice isJailbroken] — checks for Cydia, apt, etc.
// sub_10020FCD8: isJailbroken (copy)
// sub_10020FDB0: isJailbroken (copy)
// We patch each to: MOV W0, #0; RET  (return false)
static void patch_antiban(void) {
    uint32_t patch[] = {
        0x52800000, // MOV W0, #0
        0xD65F03C0, // RET
        0xD503201F, // NOP
        0xD503201F, // NOP
    };

    uint32_t *fn;
    fn = (uint32_t *)(get_image_base() + OFF_JAILBREAK_1);
    patch_instructions(fn, patch, 4);
    printf("[cheat] Anti-Ban: patched isJailbroken #1 @ %p\n", fn);

    fn = (uint32_t *)(get_image_base() + OFF_JAILBREAK_2);
    patch_instructions(fn, patch, 4);
    printf("[cheat] Anti-Ban: patched isJailbroken #2 @ %p\n", fn);

    fn = (uint32_t *)(get_image_base() + OFF_JAILBREAK_3);
    patch_instructions(fn, patch, 4);
    printf("[cheat] Anti-Ban: patched isJailbroken #3 @ %p\n", fn);
}

// ============================================================================
// FEATURE 9: ANTI-TAMPER (Debug / Integrity Bypass)
// ============================================================================
// sub_100ED44E4: +[TSEnvironment debuggingFlag] — checks debugger, tty, getppid
// sub_100ED4930: TuringShield — file existence checks for jailbreak
// sub_1006C7AE4: MidasIAP debug check
// sub_100EBDBB4: Orchestrator — complex anti-tamper orchestration
// We patch all to return 0 / safe values.
static void patch_antitamper(void) {
    uint32_t safe_patch[] = {
        0x52800000, // MOV W0, #0
        0xD65F03C0, // RET
        0xD503201F, // NOP
        0xD503201F, // NOP
    };

    // debuggingFlag — this is an ObjC method, so we can swizzle or direct patch
    uint32_t *fn;
    fn = (uint32_t *)(get_image_base() + OFF_DEBUG_FLAG);
    patch_instructions(fn, safe_patch, 4);
    printf("[cheat] Anti-Tamper: patched debuggingFlag @ %p\n", fn);

    // TuringShield
    fn = (uint32_t *)(get_image_base() + OFF_TURING_SHIELD);
    patch_instructions(fn, safe_patch, 4);
    printf("[cheat] Anti-Tamper: patched TuringShield @ %p\n", fn);

    // MidasDebug
    fn = (uint32_t *)(get_image_base() + OFF_MIDAS_DEBUG);
    patch_instructions(fn, safe_patch, 4);
    printf("[cheat] Anti-Tamper: patched MidasDebug @ %p\n", fn);

    // Orchestrator
    fn = (uint32_t *)(get_image_base() + OFF_ORCHESTRATOR);
    patch_instructions(fn, safe_patch, 4);
    printf("[cheat] Anti-Tamper: patched Orchestrator @ %p\n", fn);
}

// ============================================================================
// FEATURE 10: ANTI-DETECTION — Hide from dyld module enumeration
// ============================================================================
// The anti-cheat (tersafe) enumerates loaded modules via _dyld_image_count()
// and _dyld_get_image_name() to find suspicious libraries like "libcheat.dylib".
// We hook all dyld image APIs to skip our entry, making us invisible.

static uint32_t g_cheat_index = UINT32_MAX;
static const char *g_cheat_name = NULL;

// Original dyld functions
static uint32_t (*orig_image_count)(void) = NULL;
static const struct mach_header *(*orig_get_image_header)(uint32_t) = NULL;
static const char *(*orig_get_image_name)(uint32_t) = NULL;
static intptr_t (*orig_get_image_slide)(uint32_t) = NULL;

static void find_our_image(void) {
    uint32_t count = orig_image_count();
    for (uint32_t i = 0; i < count; i++) {
        const char *name = orig_get_image_name(i);
        if (name && strstr(name, "libTDataMaster")) {
            g_cheat_index = i;
            g_cheat_name = name;
            printf("[cheat] Anti-Detection: found our image at index %u: %s\n", i, name);
            return;
        }
    }
    printf("[cheat] Anti-Detection: WARNING — could not find our image\n");
}

static uint32_t hooked_image_count(void) {
    uint32_t count = orig_image_count();
    if (g_cheat_index != UINT32_MAX) return count - 1;
    return count;
}

static const struct mach_header *hooked_get_image_header(uint32_t index) {
    if (g_cheat_index != UINT32_MAX && index >= g_cheat_index) index++;
    return orig_get_image_header(index);
}

static const char *hooked_get_image_name(uint32_t index) {
    if (g_cheat_index != UINT32_MAX && index >= g_cheat_index) index++;
    return orig_get_image_name(index);
}

static intptr_t hooked_get_image_slide(uint32_t index) {
    if (g_cheat_index != UINT32_MAX && index >= g_cheat_index) index++;
    return orig_get_image_slide(index);
}

static void patch_anti_detection(void) {
    orig_image_count = _dyld_image_count;
    orig_get_image_header = _dyld_get_image_header;
    orig_get_image_name = _dyld_get_image_name;
    orig_get_image_slide = _dyld_get_image_vmaddr_slide;

    find_our_image();

    void *count_addr = dlsym(RTLD_DEFAULT, "_dyld_image_count");
    void *header_addr = dlsym(RTLD_DEFAULT, "_dyld_get_image_header");
    void *name_addr = dlsym(RTLD_DEFAULT, "_dyld_get_image_name");
    void *slide_addr = dlsym(RTLD_DEFAULT, "_dyld_get_image_vmaddr_slide");

    if (count_addr) hook_install(count_addr, (void *)hooked_image_count, NULL, 0);
    if (header_addr) hook_install(header_addr, (void *)hooked_get_image_header, NULL, 0);
    if (name_addr) hook_install(name_addr, (void *)hooked_get_image_name, NULL, 0);
    if (slide_addr) hook_install(slide_addr, (void *)hooked_get_image_slide, NULL, 0);

    printf("[cheat] Anti-Detection: dyld enumeration hooks installed\n");
}

// ============================================================================
// CONSTRUCTOR — Entry point when dylib is loaded
// ============================================================================
__attribute__((constructor))
static void cheat_init(void) {
    printf("[cheat] CodeV Internal loaded. Image base: 0x%lx\n", get_image_base());

    // Anti-detection MUST be installed IMMEDIATELY — before tersafe scans modules
    patch_anti_detection();

    // Delay game hooks to let the game initialize
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(5.0 * NSEC_PER_SEC)),
                   dispatch_get_main_queue(), ^{
        printf("[cheat] Installing hooks...\n");

        // Feature 1: No Recoil
        patch_no_recoil();

        // Feature 2: ESP
        patch_esp();

        // Feature 3: No Spread
        patch_no_spread();

        // Feature 4: Speed Fire
        patch_speed_fire();

        // Feature 5: Speed Hack
        patch_speed_hack();

        // Feature 6: Spinbot
        patch_spinbot();

        // Feature 7: Aimlock
        patch_aimlock();

        // Feature 8: Anti-Ban
        patch_antiban();

        // Feature 9: Anti-Tamper
        patch_antitamper();

        printf("[cheat] All hooks installed. %d total hooks active.\n", g_hook_count);
    });
}
