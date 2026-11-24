# Interoperability evidence

Evidence cutoff: November 20, 2026. This inventory covers the two accessible
[GitHub Actions runs](https://github.com/daviduhden/opencvs-portable/actions):
[44752958590](https://github.com/daviduhden/opencvs-portable/actions/runs/44752958590)
at `aca3007c760ff359b922144486f7eed3d2de06c3`, and
[44754537542](https://github.com/daviduhden/opencvs-portable/actions/runs/44754537542)
at `1038f5afb14df469723b711e71a76e8a6618f13c`.
Use the linked GitHub Actions runs to retrieve the job logs and artifacts.

Both concluded **failure**. Parser and audit jobs succeeded in both; every native
compilation job failed. No local build, configuration, or test was performed for
this audit. Source changes after those SHAs remain unverified until a new hosted run.

No GNU CVS interoperability case ran successfully or failed at an assertion in
these runs; compilation blocked the suite. GNU CVS dependency installation is
not interoperability verification. The in-tree SSH transport shim checks pipe
and protocol paths without proving behavior against a real SSH daemon.

CMake now identifies the selected reference tool by its `--version` output and
requires the Free Software Foundation identification for GNU CVS. This avoids
mislabeling OpenBSD's base OpenCVS as GNU CVS. `PC_REQUIRE_GNU_CVS=ON` makes
Linux coverage fail configuration rather than silently skip the reference suite.
OpenBSD VM coverage deliberately does not claim GNU CVS interchange when the
reference implementation is unavailable.

Record both directions separately: GNU client to OpenCVS server, and OpenCVS
client to GNU server, plus local RCS archive exchange, binary preservation,
keywords, tags/branches, conflicts and deletion. No interoperability guarantee
can be inferred from parser generation or matching file extensions.

## Follow-up on fd0ed04

The GNU CVS suites remain blocked by compilation. These runs provide no evidence on archive interchange or either wire direction. Keep the existing interoperability cases enabled for the next manual run.

Evidence: [native matrix 44760299305](https://github.com/daviduhden/opencvs-portable/actions/runs/44760299305) and [OpenBSD 44760299315](https://github.com/daviduhden/opencvs-portable/actions/runs/44760299315), both at `fd0ed043ee5521637c2c9c738f583b8b4a17f2da`. Local changes require a new published revision for validation.

## Follow-up on b79f865

INTEROP-401. OpenCVS client against GNU server rejects a repository path outside the selected root. Preserve the containment check until the expected response shape is established from the protocol. The opposite direction also failed, including setup failures. GNU archive interchange reached the RCS commit-ID ownership leak under LeakSanitizer.

Evidence at `b79f865ec6563a6f7943fda4fc607beed16cee76`: [native matrix 44918160599](https://github.com/daviduhden/opencvs-portable/actions/runs/44918160599), [OpenBSD 44918164184](https://github.com/daviduhden/opencvs-portable/actions/runs/44918164184). Local fixes await CI.

## Follow-up on 9d03e23

INTEROP-501. GNU server response path rejection persists. GNU client against OpenCVS also reports unexpected EOF. musl GNU CVS reports that root cannot commit. The root restriction is a CI environment issue. The wire failures remain separate and root containment remains in place.

Evidence at `9d03e23347299f4b652d26dbb828b7cced1ebea1`: [native matrix 44918836788](https://github.com/daviduhden/opencvs-portable/actions/runs/44918836788) (cancelled overall), [OpenBSD 44918836951](https://github.com/daviduhden/opencvs-portable/actions/runs/44918836951) (failed, jobs 136890189891 and 136890190132). Local changes await CI.

## Follow-up on ba0ad43

INTEROP-601. GNU-to-OpenCVS wire setup passes the remote command as a single argument while the fixture shim expected split arguments. The local harness accepts both forms with strict validation of the configured fixture executable and server verb. OpenCVS-to-GNU still rejects a repository path outside the selected root. Containment remains unchanged pending the exact protocol path semantics.

Evidence at `ba0ad436b3c0cc0ec04e72f9c98982765cb7c486`: [native matrix 44920499963](https://github.com/daviduhden/opencvs-portable/actions/runs/44920499963) and [OpenBSD 44920500268](https://github.com/daviduhden/opencvs-portable/actions/runs/44920500268). The local changes await CI.

## Follow-up on 4646494

INTEROP-701. GNU client checkout against OpenCVS now succeeds with the corrected fixture, then commit reports EOF after the server fails to flush Entries. Local Entries paths are anchored against cwd changes and the private server cache is discarded before deleting its sandbox. The opposite direction still rejects a server repository path outside the selected root. Keep containment pending protocol evidence.

Evidence at `4646494cf36970b37689580587497610321318db`: [native matrix 44923879403](https://github.com/daviduhden/opencvs-portable/actions/runs/44923879403) and [OpenBSD 44923879331](https://github.com/daviduhden/opencvs-portable/actions/runs/44923879331). Local changes await validation.
