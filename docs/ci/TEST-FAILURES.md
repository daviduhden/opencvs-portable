# Test failures and unexecuted tests

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

No CTest suite completed in the accessible native jobs. Linux and Windows test
steps were skipped after compilation failure; the musl combined shell step
stopped at its compiler failure before CTest. Do not count suites as failing
individual assertions when they never ran, and do not count skipped tests as
passes.

Parser generation and the audit job succeeded, but neither exercises CVS data
operations. New coverage includes native unit, core, security, and Linux GNU CVS
interoperability suites. Debug MSVC/clang-cl, Debug Linux x86-64/ARM64, and an
experimental real OpenBSD VM Debug/Release workflow are prepared, not executed.

Failure artifacts preserve compiler output, CMake feature-probe diagnostics, and
CTest temporary logs when present. Missing CTest logs after compilation failure
are expected; preserve the compiler failure rather than manufacturing a test
result.

## Follow-up on fd0ed04

The parser and audit jobs completed. Product tests, regression tests, and sanitizer runs remained blocked. The local sticky-date regression checks rejection and byte preservation in a temporary working copy.

Evidence: [native matrix 44760299305](https://github.com/daviduhden/opencvs-portable/actions/runs/44760299305) and [OpenBSD 44760299315](https://github.com/daviduhden/opencvs-portable/actions/runs/44760299315), both at `fd0ed043ee5521637c2c9c738f583b8b4a17f2da`. Local changes require a new published revision for validation.
