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
