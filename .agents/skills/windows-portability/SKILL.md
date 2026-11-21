---
name: windows-portability
description: Implement and review native Win32 behavior for Portable OpenCVS without WSL, Cygwin, or MSYS runtime dependencies.
---

# Windows Portability

Read [the Windows contract](../../../docs/windows.md) and inspect `compat/windows/`, `platform/windows/`, and [pc_process.h](../../../compat/include/pc_process.h) before changing shared engine code. Use strict UTF-8/UTF-16 conversions and wide Win32 APIs; never fall back silently to the ANSI code page.

Review reserved names, ADS, drive/UNC roots, long paths, trailing aliases, case collisions, reparse points, binary mode, replace/lock semantics and handle inheritance. Process launch takes structured argv; do not imitate fork or invoke cmd.exe for convenience. The current transport is external native OpenSSH with pipes, not a socket implementation; do not claim Winsock coverage that does not exist. Run native MSVC, clang-cl and MinGW tests only in their hosted CI jobs, including the native filesystem probe and [PE import inspection](../../../scripts/check-windows-imports.pl).

## Maintenance & Continuous Learning

To verify deletion is denied by an existing handle, open that handle for data
I/O without `FILE_SHARE_DELETE`. An attribute-only handle does not exercise
the same sharing rules. Check restoration of attributes before closing the
blocking handle.

Update this skill when changes invalidate guidance within its scope.
