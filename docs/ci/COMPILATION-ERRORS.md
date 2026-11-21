# Compilation errors

Evidence cutoff: November 20, 2026. This inventory covers the two accessible
[GitHub Actions runs](https://github.com/daviduhden/opencvs-portable/actions):
[44752958590](https://github.com/daviduhden/opencvs-portable/actions/runs/44752958590)
at `aca3007c760ff359b922144486f7eed3d2de06c3`, and
[44754537542](https://github.com/daviduhden/opencvs-portable/actions/runs/44754537542)
at `1038f5afb14df469723b711e71a76e8a6618f13c`.
The linked GitHub Actions runs are the source for job logs and artifacts.
The generated diagnostic index is retained locally at
`docs/ci/diagnostic-index.json`. Downloaded evidence resides in `/tmp/opencvs-ci/`.

Both concluded **failure**. Parser and audit jobs succeeded in both; every native
compilation job failed. No local build, configuration, or test was performed for
this audit. Source changes after those SHAs remain unverified until a new hosted run.

Latest-run compiler identification comes from the actual CMake configure logs:

| Job | Compiler | Furthest result |
| --- | --- | --- |
| linux gcc | GNU 13.3.0 | Compilation failed |
| linux clang | Clang 18.1.3 | Compilation failed |
| linux clang-sanitizers | Clang 18.1.3 | Compilation failed; no sanitizer runtime result |
| musl | GNU 15.2.0 | Compilation failed |
| windows msvc | MSVC 19.52.37029.0 | Compilation failed |
| windows clang-cl | Clang 22.1.3, MSVC-like CLI | Compilation failed |
| mingw | GNU 16.2.0 | Compilation failed |

Concrete diagnostic families include `windows_compat.h:80` redefining SDK
`struct timespec`; `regcomp.c:390,569,594,599,945` lacking
`_POSIX2_RE_DUP_MAX`; Windows `log.h:49` declaration parsing failures followed
by cascading declaration errors; musl `file.h:114` missing `MAXNAMLEN`;
`update.c:274` relying on incompatible `getdents` interfaces; MinGW
`file.h:74` colliding with SDK `FILE_UNKNOWN`; and MinGW `%z` format
attribute interpretation in `xmalloc.c:35,49`.

These are categories, not a claimed count of independent defects: the shared
header failures recur in many translation units. Fix shared declarations and
feature selection first, then inspect remaining diagnostics. Retain `-Werror`;
blanket warning suppression would conceal independent defects.

## Follow-up on fd0ed04

- **BUILD-301.** `diff3.c` reported an incomplete `struct stat` or undeclared `fstat` on Linux and OpenBSD. The merge path introduced descriptor metadata inspection without its defining header. The local fix includes the defining header directly.
- **BUILD-302.** Clang reported a missing `type` initializer in regenerated `date.c`. The last partial table sentinel remained in `src/date.y`. Fix the grammar and verify regeneration on both hosted generators.
- **BUILD-303.** Windows compilation exposed fallback regex diagnostics and missing POSIX declarations. See the portability and warning inventories for the related causes.

Evidence: [native matrix 44760299305](https://github.com/daviduhden/opencvs-portable/actions/runs/44760299305) and [OpenBSD 44760299315](https://github.com/daviduhden/opencvs-portable/actions/runs/44760299315), both at `fd0ed043ee5521637c2c9c738f583b8b4a17f2da`. Local changes require a new published revision for validation.

| Run | Compilation jobs | Job IDs | Result |
| --- | --- | --- | --- |
| 44760299305 | Ubuntu GCC x86-64 Release and Debug, GCC ARM64 Debug, Clang Release and Clang with sanitizers | 136371177055, 136371177117, 136371177047, 136371176901, 136371177292 | Blocked before executable tests |
| 44760299305 | Alpine musl and Windows MinGW | 136371176648, 136371176615 | Blocked before executable tests |
| 44760299305 | Windows MSVC and ClangCL, Debug and Release | 136371176866, 136371176872, 136371176941, 136371176832 | Blocked before executable tests |
| 44760299315 | OpenBSD Clang 22.1.6 Debug and Release | 136370732615, 136370733289 | Compatibility archive built. Full build blocked |

The native workflow used Ninja, strict warnings, and byacc output from its
successful parser job (136370730771). OpenBSD used Ninja and the VM's native
yacc. The aggregate logs and available diagnostic artifacts are under
`/tmp/opencvs-ci/`.

## Follow-up on b79f865

BUILD-401. MSVC still rejects narrowing in the fallback regex and date lexer, a potentially uninitialized log delimiter, and an unreachable server return. ClangCL reaches the linker, where lld-link reports duplicate MANIFEST resource ID 1. The local build change disables the linker-generated manifest while retaining the explicit resource.

Evidence at `b79f865ec6563a6f7943fda4fc607beed16cee76`: [native matrix 44918160599](https://github.com/daviduhden/opencvs-portable/actions/runs/44918160599), [OpenBSD 44918164184](https://github.com/daviduhden/opencvs-portable/actions/runs/44918164184). Local fixes await CI.

| Job group | Job IDs | Progress |
| --- | --- | --- |
| Parser and audit | 136888155660, 136888155930 | Passed |
| glibc GCC Release/Debug, ARM64 Debug, Clang, Clang sanitizers | 136888309088, 136888309148, 136888309103, 136888309096, 136888309117 | Linked and reached CTest |
| Alpine musl and MinGW | 136888309013, 136888309038 | Linked and reached CTest |
| MSVC Release and Debug | 136888309051, 136888309072 | Blocked during compilation |
| ClangCL Release and Debug | 136888309081, 136888309094 | Blocked at link |
| OpenBSD Release and Debug | 136888166914, 136888167179 | Linked and reached CTest |

The workflows used Ninja and strict warnings. The native matrix used byacc
and OpenBSD used the VM's yacc. Full logs and downloaded artifacts are in
`/tmp/opencvs-ci/`, including CTest and compiler diagnostics.

## Follow-up on 9d03e23

BUILD-501. MSVC Release and Debug failed with C4701 for regex alternation offsets and the generated date parser local pointer. ClangCL linked and reached CTest, confirming progress on the duplicate manifest. The ClangCL Debug test job was cancelled. Job IDs: MSVC 136890316295/136890316465, ClangCL 136890316308/136890316341.

Evidence at `9d03e23347299f4b652d26dbb828b7cced1ebea1`: [native matrix 44918836788](https://github.com/daviduhden/opencvs-portable/actions/runs/44918836788) (cancelled overall), [OpenBSD 44918836951](https://github.com/daviduhden/opencvs-portable/actions/runs/44918836951) (failed, jobs 136890189891 and 136890190132). Local changes await CI.
