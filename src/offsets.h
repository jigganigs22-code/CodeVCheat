#pragma once
#include <stdint.h>

// Image base (IDA fixed base; at runtime use get_image_base())
#define IDA_BASE 0x100000000ULL

// === FEATURE 1: NO RECOIL ===
// sub_10322D168: float getter, returns recoil value from this+0x144
// Hook: force return 0.0f (no recoil)
#define OFF_GET_RECOIL          (0x10322D168 - IDA_BASE)  // 3 insns: LDR S0,[X0,#0x144]; LDR S1,[X0,#0x148]; RET

// === FEATURE 2: ESP (Wallhack) ===
// sub_10243C524: calls vtable[149] on entity, stores bool to *a3
// Hook: force *a3 = 0 (not hidden = all actors visible)
#define OFF_IS_HIDDEN           (0x10243C524 - IDA_BASE)  // STRB result to [X19=X2]

// === FEATURE 3: NO SPREAD ===
// sub_10709721C: float spread_calc(float *weaponData, int aiming)
// Hook: force return 0.0f (zero spread)
#define OFF_SPREAD_CALC         (0x10709721C - IDA_BASE)

// === FEATURE 4: SPEED FIRE ===
// sub_1022F6C10: float RandomFireDelay(float min, float max)
// Called from FireInterval with (0.3f, 0.5f)
// Hook: force return 0.01f (near-instant fire)
#define OFF_RANDOM_FIRE_DELAY   (0x1022F6C10 - IDA_BASE)

// === FEATURE 5: SPEED HACK ===
// sub_10263C97C: PropertySetter(entity, FName, flags, float_value)
// Called with (entity, "MoveSpeed", 0, 1.0f) and (entity, "DashSpeedMultiplier", 0, 1.0f)
// Hook: if float_value == 1.0f, change to 2.0f
#define OFF_PROPERTY_SETTER     (0x10263C97C - IDA_BASE)

// === FEATURE 6: SPINBOT ===
// sub_106A3FE28: AddControllerYawInput(controller, input_data)
// Computes yaw delta, applies via vtable[0x7F0] call on controller
// Hook: call original, then add constant spin via vtable[0x7F0]
#define OFF_YAW_INPUT           (0x106A3FE28 - IDA_BASE)
#define VTABLE_OFFSET_YAW       0x7F0  // vtable slot for AddControllerYawInput

// === FEATURE 7: AIMLOCK ===
// sub_102398158: Visibility gate — checks byte+0x2A==1 && byte+0x2C==1
//   DO NOT NOP THIS — keeps game's visibility check so aimlock doesn't go through walls
// sub_10239460C: AimCore — PushLookAtCmd — applies aim toward float* position (a4)
//   Hook: add head offset to Z component → snaps aim to enemy HEAD instead of body
// sub_1023DE454: Aim target resolution — resolves entity by ID, calls visibility gate
// The game's own aim assist handles: target selection, visibility, fire detection
#define OFF_VISIBILITY_GATE     (0x102398158 - IDA_BASE)  // visibility check gate (DO NOT PATCH)
#define OFF_AIM_TARGET          (0x1023DE454 - IDA_BASE)  // aim target resolver
#define OFF_AIM_CORE            (0x10239460C - IDA_BASE)  // AimCore — PushLookAtCmd

// === FEATURE 8: ANTI-BAN ===
// sub_10020FC04: -[UIDevice isJailbroken] - ObjC method, checks Cydia/apt paths
// Hook: return false (0)
#define OFF_JAILBREAK_1         (0x10020FC04 - IDA_BASE)
#define OFF_JAILBREAK_2         (0x10020FCD8 - IDA_BASE)
#define OFF_JAILBREAK_3         (0x10020FDB0 - IDA_BASE)

// === FEATURE 9: ANTI-TAMPER ===
// sub_100ED44E4: +[TSEnvironment debuggingFlag] - checks debugger, tty, getppid
// sub_100ED4930: TuringShield - file existence jailbreak checks
// sub_1006C7AE4: MidasIAP debug check
// sub_100EBDBB4: Orchestrator - complex anti-tamper orchestration
// Hook all: return safe values (0 / false)
#define OFF_DEBUG_FLAG          (0x100ED44E4 - IDA_BASE)
#define OFF_TURING_SHIELD       (0x100ED4930 - IDA_BASE)
#define OFF_MIDAS_DEBUG         (0x1006C7AE4 - IDA_BASE)
#define OFF_ORCHESTRATOR        (0x100EBDBB4 - IDA_BASE)

// Recoil system global flag
#define OFF_RECOIL_FLAG         (0x1099EF2B0 - IDA_BASE)
