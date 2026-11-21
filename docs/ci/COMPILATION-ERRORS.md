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
