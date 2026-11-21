# Compilation warnings

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

Warnings promoted to errors are compilation failures, not passing builds with
cosmetic output. Latest-run examples:

| Diagnostic | Evidence | Required response |
| --- | --- | --- |
| unused parameter | `watch.c:223,228`, `version.c:33`, many server callbacks | Mark intentionally unused parameters or remove genuinely unnecessary ones; preserve callbacks' ABI |
| pointer signedness | `update.c:551,559,561,563` | Correct buffer type at semantic boundary; do not cast away bounds problems |
| implicit fallthrough | `update.c:466` | Review control flow, then use compiler-supported annotation if intentional |
| incompatible pointer type | musl `update.c:274` getdents buffer | Replace raw BSD directory records with the portable directory API |
| format mismatch | MinGW `xmalloc.c:35,49` | Match format attribute dialect to the selected CRT; retain format checks |

The latest aggregate log contains no ordinary C `warning:` lines because strict
builds promote them. MSYS2's package-manager warning to terminate other MSYS2
programs is infrastructure output and is not a source-code diagnostic. No claim
of warning-free compilation is supported by these runs.

## Follow-up on fd0ed04

- **WARN-301.** GCC on glibc and musl rejected nullable `%s` arguments in the conditional diff format. The local change makes the conditional symbol explicit and validates it before use.
- **WARN-302.** MSVC reported narrowing, signed/unsigned mismatch, and potentially uninitialized test I/O state. Review each conversion's input bounds, error sentinel, and destination type.
- **WARN-303.** UCRT deprecated `sscanf` in both sticky-date readers and the wire mode parser. Both date readers now share a bounded decimal parser. The mode parser checks syntax and consumes the complete field.
- The date generators reported the existing ten shift/reduce conflicts. This remains a generator diagnostic. The local source changes do not alter grammar productions.

Evidence: [native matrix 44760299305](https://github.com/daviduhden/opencvs-portable/actions/runs/44760299305) and [OpenBSD 44760299315](https://github.com/daviduhden/opencvs-portable/actions/runs/44760299315), both at `fd0ed043ee5521637c2c9c738f583b8b4a17f2da`. Local changes require a new published revision for validation.

## Follow-up on b79f865

WARN-401. MSVC C4244 and C4267 identify mismatched representation widths in regex and date parsing. C4701 and C4702 identify the log delimiter and server control-flow diagnostics. Preserve strict flags. The yacc conflict diagnostic remains separate from compiler warnings.

Evidence at `b79f865ec6563a6f7943fda4fc607beed16cee76`: [native matrix 44918160599](https://github.com/daviduhden/opencvs-portable/actions/runs/44918160599), [OpenBSD 44918164184](https://github.com/daviduhden/opencvs-portable/actions/runs/44918164184). Local fixes await CI.

## Follow-up on 9d03e23

WARN-501. Review reaching definitions on error paths, where compiler dataflow flags potentially uninitialized regex offsets and the date parser pointer. Fix lexer issues in the grammar rather than patching generated C.

Evidence at `9d03e23347299f4b652d26dbb828b7cced1ebea1`: [native matrix 44918836788](https://github.com/daviduhden/opencvs-portable/actions/runs/44918836788) (cancelled overall), [OpenBSD 44918836951](https://github.com/daviduhden/opencvs-portable/actions/runs/44918836951) (failed, jobs 136890189891 and 136890190132). Local changes await CI.

## Follow-up on ba0ad43

WARN-601. The aggregate logs contain an MSYS2 update advisory about other processes, separate from project compiler diagnostics. Require native executable dependency checks after a successful Windows compile even when CTest fails.

Evidence at `ba0ad436b3c0cc0ec04e72f9c98982765cb7c486`: [native matrix 44920499963](https://github.com/daviduhden/opencvs-portable/actions/runs/44920499963) and [OpenBSD 44920500268](https://github.com/daviduhden/opencvs-portable/actions/runs/44920500268). The local changes await CI.
