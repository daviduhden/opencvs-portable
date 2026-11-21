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

## Follow-up on fd0ed04

- **PORT-301.** OpenBSD Clang 22.1.6 configured Debug and Release and built the compatibility archive with native BSD containers and interfaces. The executable failed at BUILD-301 and BUILD-302.
- **PORT-302.** Windows fallback regex hit the SDK `INFINITY` macro, signedness and possible uninitialized-state diagnostics. The local fixes namespace the sentinel and use bounded, appropriate index types.
- **PORT-303.** The Windows account backend lacked `endpwent`, and an RCS consumer relied on `bzero`. The local changes release the backend's cache and use standard C memory initialization.

Evidence: [native matrix 44760299305](https://github.com/daviduhden/opencvs-portable/actions/runs/44760299305) and [OpenBSD 44760299315](https://github.com/daviduhden/opencvs-portable/actions/runs/44760299315), both at `fd0ed043ee5521637c2c9c738f583b8b4a17f2da`. Local changes require a new published revision for validation.

## Follow-up on b79f865

PORT-401. MinGW import constructs /D: before its first mkdir. Directory creation now walks native path prefixes and retains drive and UNC roots. PORT-402. OpenBSD may return a child from posix_spawn and report exec failure through its exit status. The test now requires ENOENT from spawn or failure status 127 from the child. Native fgetln EOF is checked with feof rather than using its length output after NULL.

Evidence at `b79f865ec6563a6f7943fda4fc607beed16cee76`: [native matrix 44918160599](https://github.com/daviduhden/opencvs-portable/actions/runs/44918160599), [OpenBSD 44918164184](https://github.com/daviduhden/opencvs-portable/actions/runs/44918164184). Local fixes await CI.

## Follow-up on 9d03e23

PORT-501. Native Windows import still fails: MinGW reports permission denied replacing RCS archives, while ClangCL Release rejects an existing directory reached via a short name. The local backend accepts verified short aliases for directories and handles the source read-only attribute across rename. The exact cause of the reported permission error still requires CI confirmation. Linux musl runs GNU CVS as root, which GNU CVS explicitly rejects for commits. CI and release now run musl CTest as an unprivileged account.

Evidence at `9d03e23347299f4b652d26dbb828b7cced1ebea1`: [native matrix 44918836788](https://github.com/daviduhden/opencvs-portable/actions/runs/44918836788) (cancelled overall), [OpenBSD 44918836951](https://github.com/daviduhden/opencvs-portable/actions/runs/44918836951) (failed, jobs 136890189891 and 136890190132). Local changes await CI.

## Follow-up on ba0ad43

PORT-601. Windows read-only replacement still fails with Permission denied, including the focused unit case. The prior short directory alias error no longer blocks import. Distinguish the stages of rename and restoration before attributing failure to file attributes or share modes. musl no longer reports GNU CVS refusing root commits.

Evidence at `ba0ad436b3c0cc0ec04e72f9c98982765cb7c486`: [native matrix 44920499963](https://github.com/daviduhden/opencvs-portable/actions/runs/44920499963) and [OpenBSD 44920500268](https://github.com/daviduhden/opencvs-portable/actions/runs/44920500268). The local changes await CI.

## Follow-up on 4646494

PORT-701. Across all five Windows jobs, renaming to a missing destination succeeds. Replacing an existing destination fails at MoveFileExW with Win32 error 5 and errno 13 regardless of the external test handles. Compare native replacement semantics rather than changing test assertions.

The local Windows backend uses `FileRenameInfoEx` with a validated destination directory handle. It requests replacement and read-only handling in one operation, leaving file attributes intact if the operation fails. Replacement requires an OS and filesystem that support the flags. The fallback for a missing destination uses `FileRenameInfo` with replacement disabled. The existing Windows unit cases cover missing and existing destinations and open handles. Atomic namespace replacement and durability after power loss are separate properties. [Microsoft FILE_RENAME_INFO reference](https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info)

Evidence at `4646494cf36970b37689580587497610321318db`: [native matrix 44923879403](https://github.com/daviduhden/opencvs-portable/actions/runs/44923879403) and [OpenBSD 44923879331](https://github.com/daviduhden/opencvs-portable/actions/runs/44923879331). Local changes await validation.
