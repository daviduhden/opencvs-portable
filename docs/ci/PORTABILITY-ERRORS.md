# Portability errors

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

| Pattern | Affected platform | Repair status |
| --- | --- | --- |
| Hardcoded Visual Studio 17 generator | First-run windows-2025 image has VS2026 | Latest run confirms discovery/Ninja reached compilation; initial configuration issue resolved |
| UPSTREAM file and upstream directory case collision | Windows case-insensitive filesystem | Renamed directory to provenance; latest configure succeeded |
| SDK structure ownership guessed from private guards | MSVC/clang-cl timespec | Added CMake declaration probe; Windows fallback integration pending hosted validation |
| BSD getdents record assumptions | glibc/musl/MinGW | Core owner replacing with POSIX directory iteration; pending hosted validation |
| FNM_CASEFOLD visibility differs from fnmatch existence | glibc under explicit feature macros | Added flag probe and matched fallback selection; pending hosted validation |
| Whole imported include directory shadows native regex ABI | Mixed-native/fallback platforms | Isolated selected generated fallback headers; pending hosted validation |
| Linkable symbol not necessarily declared | asprintf/vasprintf | Declaration+link checks under target feature macros; pending hosted validation |

No Windows runtime, OpenBSD VM, ARM64, or Debug success is established by the
historical runs. New manual matrix entries create future coverage, not evidence.
