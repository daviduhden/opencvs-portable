# Runtime errors

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

No product runtime was reached by the accessible native jobs. The compile failure
prevents an executable from being considered usable. There are therefore **no
observed product runtime failures or successes** in this evidence set; this does
not imply absence of runtime defects.

Future evidence must retain exit code, stderr/stdout, exact command, platform,
compiler, configuration, failing sandbox path, and source SHA. Reproduce only in
private disposable CVS repositories. Treat hangs, crashes, unexpected success,
and incorrect output as separate failure classes. Native Windows child-process,
Unicode, locking and rename behavior require native Windows runtime tests;
Linux compilation or a PE import listing cannot establish them.
