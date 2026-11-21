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

## Follow-up on b79f865

TEST-401. Unit tests passed on non-sanitized glibc and MinGW. Unit tests failed on musl, the sanitizer job, and both OpenBSD configurations. Core and security suites failed across configurations that reached CTest. Existing repeated-init and import cases cover the common setup failures. Keep independent failures visible after fixing setup.

Evidence at `b79f865ec6563a6f7943fda4fc607beed16cee76`: [native matrix 44918160599](https://github.com/daviduhden/opencvs-portable/actions/runs/44918160599), [OpenBSD 44918164184](https://github.com/daviduhden/opencvs-portable/actions/runs/44918164184). Local fixes await CI.

## Follow-up on 9d03e23

TEST-501. Both OpenBSD unit jobs passed. Core still reports an incorrect diff exit status and accepts committing an unresolved conflict. Security rejects the intended valid delta fixture before negative cases and the tag fixture assumed that slash was forbidden. The existing RCS symbol grammar permits slash and rejects period, so the rejection fixture now uses a period. The valid fixture now starts with the first delta instruction immediately after its opening quote. Fixture-only rewrites explicitly make sandbox files writable when needed.

Evidence at `9d03e23347299f4b652d26dbb828b7cced1ebea1`: [native matrix 44918836788](https://github.com/daviduhden/opencvs-portable/actions/runs/44918836788) (cancelled overall), [OpenBSD 44918836951](https://github.com/daviduhden/opencvs-portable/actions/runs/44918836951) (failed, jobs 136890189891 and 136890190132). Local changes await CI.

## Follow-up on ba0ad43

TEST-601. OpenBSD Debug and Release passed unit and security suites. Their core suite failed on missing output after export -d and an accepted unresolved conflict commit. Non-sanitized Linux also passed unit and security. All Windows unit suites failed the replacement path, and core/security failures include import setup cascades.

Evidence at `ba0ad436b3c0cc0ec04e72f9c98982765cb7c486`: [native matrix 44920499963](https://github.com/daviduhden/opencvs-portable/actions/runs/44920499963) and [OpenBSD 44920500268](https://github.com/daviduhden/opencvs-portable/actions/runs/44920500268). The local changes await CI.

## Follow-up on 4646494

TEST-701. OpenBSD passed all three suites in Debug and Release. Ordinary Linux core and security passed. Windows replacement blocks import and downstream cases. The sanitizer matrix and GNU interoperability still fail. Existing regressions cover these cases and remain enabled.

Evidence at `4646494cf36970b37689580587497610321318db`: [native matrix 44923879403](https://github.com/daviduhden/opencvs-portable/actions/runs/44923879403) and [OpenBSD 44923879331](https://github.com/daviduhden/opencvs-portable/actions/runs/44923879331). Local changes await validation.
## Follow-up on 3e66692

[Native run 44950770518](https://github.com/daviduhden/opencvs-portable/actions/runs/44950770518)
compiled on Ubuntu 24.04, Alpine/musl and Windows Server 2025. Windows MSVC,
clang-cl and MinGW reached CTest. Their unit fixture used an attribute-only
handle to deny deletion and failed the deletion, restoration and cleanup
assertions. Core/security results are separate from this fixture failure.
The replacement fixture opens the file for data reads without delete sharing,
matching the [Win32 sharing contract](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-createfilew).

Ubuntu x86-64 GCC Release and Clang ASan/UBSan reached CTest. The concurrent
tag case hit the harness deadline of 30 seconds, equal to one repository-lock
retry delay. GCC Debug completed both tags on that SHA. The case-specific
deadline now exceeds the lock's retry delay while the regular cases retain
their existing deadline. The OpenBSD 8.0 Debug and Release jobs passed in
[run 44950770201](https://github.com/daviduhden/opencvs-portable/actions/runs/44950770201).
