# Interoperability evidence

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
