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
