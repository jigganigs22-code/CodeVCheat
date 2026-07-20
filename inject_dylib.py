#!/usr/bin/env python3
"""Inject libcheat.dylib into CodeV binary using LIEF."""
import lief
import sys
import os
import shutil

APP_DIR = r"C:\Users\m3hg9\Downloads\wowwww_extracted\Payload\CodeV.app"
BINARY_PATH = os.path.join(APP_DIR, "CodeV")
DYLIB_SRC = r"C:\Users\m3hg9\Downloads\CodeVCheat\libTDataMaster.dylib"
DYLIB_INSTALL_NAME = "@executable_path/libTDataMaster.dylib"

def main():
    # Backup
    backup = BINARY_PATH + ".backup"
    if not os.path.exists(backup):
        print(f"[*] Backing up original binary to {backup}")
        shutil.copy2(BINARY_PATH, backup)
    else:
        print(f"[*] Backup already exists: {backup}")

    # Copy dylib into .app bundle
    dylib_dst = os.path.join(APP_DIR, "libTDataMaster.dylib")
    print(f"[*] Copying libcheat.dylib into app bundle...")
    shutil.copy2(DYLIB_SRC, dylib_dst)
    print(f"    -> {dylib_dst}")

    # Parse Mach-O
    print(f"[*] Parsing Mach-O binary: {BINARY_PATH}")
    binary = lief.parse(BINARY_PATH)
    if binary is None:
        print("[!] Failed to parse Mach-O binary")
        sys.exit(1)

    # Check if already injected
    for cmd in binary.commands:
        if isinstance(cmd, lief.MachO.DylibCommand):
            if "libTDataMaster" in str(cmd.name):
                print("[!] libcheat.dylib already injected! Skipping.")
                return

    # Create LC_LOAD_DYLIB command
    print(f"[*] Creating LC_LOAD_DYLIB: {DYLIB_INSTALL_NAME}")
    dylib_cmd = lief.MachO.DylibCommand.load_dylib(DYLIB_INSTALL_NAME)

    # Add to binary
    print(f"[*] Adding load command to Mach-O...")
    binary.add(dylib_cmd)

    # Save
    print(f"[*] Saving modified binary...")
    binary.write(BINARY_PATH)
    print(f"[+] Successfully injected libcheat.dylib!")

    # Verify
    verify = lief.parse(BINARY_PATH)
    if verify:
        dylib_cmds = [cmd for cmd in verify.commands if isinstance(cmd, lief.MachO.DylibCommand)]
        print(f"[+] Verification: found {len(dylib_cmds)} DYLIB load commands")
        for cmd in dylib_cmds:
            print(f"    - {cmd.name}")

if __name__ == "__main__":
    main()
