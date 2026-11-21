# Regression patterns and evidence gates

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

A regression requires a previously demonstrated passing behavior. None of the
accessible native builds passes, so this audit identifies risks and repaired
configuration failures, not invented runtime regressions.

The initial VS generator failure and Windows case collision disappeared in the
second run: all Windows jobs reached compilation. Compilation then exposed
header and portability defects previously hidden by configuration failure.

Future changes must compare the same suite across Release and Debug, GCC and
Clang, glibc and musl, x86-64 and ARM64, and native Windows compilers. New matrix
entries are verification plans until run links and results exist. Debug artifact
names are distinct from distribution artifacts; prereleases retain their six
specific Release artifacts. Experimental OpenBSD results must be reported as
such and must not imply Linux/Windows support.

Each repaired family should gain a focused behavioral test owned by the test
agent. Do not replace strict warnings with global suppressions or accept missing
reference tools as passing interoperability.

## Follow-up on fd0ed04

Check both readers when they share a metadata format, and review all callers when a format argument or process exit convention changes. The Windows child output check initializes its read result so short-circuit and loop-exit paths remain defined.

Evidence: [native matrix 44760299305](https://github.com/daviduhden/opencvs-portable/actions/runs/44760299305) and [OpenBSD 44760299315](https://github.com/daviduhden/opencvs-portable/actions/runs/44760299315), both at `fd0ed043ee5521637c2c9c738f583b8b4a17f2da`. Local changes require a new published revision for validation.

## Follow-up on b79f865

Before classifying each failing integration case, identify common setup and teardown failures. A successful import message followed by an abort is still a failure. Test the observable process failure contract across native spawn implementations, and inspect successful unit jobs for diagnostic output.

Evidence at `b79f865ec6563a6f7943fda4fc607beed16cee76`: [native matrix 44918160599](https://github.com/daviduhden/opencvs-portable/actions/runs/44918160599), [OpenBSD 44918164184](https://github.com/daviduhden/opencvs-portable/actions/runs/44918164184). Local fixes await CI.

## Follow-up on 9d03e23

Validate positive fixture controls before malformed-input cases. Record process privilege, archive writability, and the distinction between a runner setup failure and a product assertion. Do not use a root process to validate ordinary GNU CVS commits.

Evidence at `9d03e23347299f4b652d26dbb828b7cced1ebea1`: [native matrix 44918836788](https://github.com/daviduhden/opencvs-portable/actions/runs/44918836788) (cancelled overall), [OpenBSD 44918836951](https://github.com/daviduhden/opencvs-portable/actions/runs/44918836951) (failed, jobs 136890189891 and 136890190132). Local changes await CI.

Local follow-up covers two separated unified diff hunks and their exit
status, the archive digest and working bytes after a rejected conflict,
and Windows replacement of a read-only file with another read-only file.
The Windows case retains open test handles across the replacement.

## Follow-up on ba0ad43

A zero exit from export is insufficient. Verify the selected tag through checkout and verify the expected bytes and absence of administrative directories in the actual export target. SSH test transport should handle both native argument layouts without executing a shell.

Evidence at `ba0ad436b3c0cc0ec04e72f9c98982765cb7c486`: [native matrix 44920499963](https://github.com/daviduhden/opencvs-portable/actions/runs/44920499963) and [OpenBSD 44920500268](https://github.com/daviduhden/opencvs-portable/actions/runs/44920500268). The local changes await CI.

## Follow-up on 4646494

The Windows rename matrix isolates destination existence from handle ownership. Use that evidence before changing file replacement. Retain the reverse wire commit and sticky-reset regressions for server cleanup and revision ownership.

Evidence at `4646494cf36970b37689580587497610321318db`: [native matrix 44923879403](https://github.com/daviduhden/opencvs-portable/actions/runs/44923879403) and [OpenBSD 44923879331](https://github.com/daviduhden/opencvs-portable/actions/runs/44923879331). Local changes await validation.
