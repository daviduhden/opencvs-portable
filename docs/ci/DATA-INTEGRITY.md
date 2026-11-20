# Data integrity evidence and release boundary

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

The accessible runs establish no successful CVS write/read cycle. They therefore
do not establish safe commit, update, import, merge, binary handling, Unicode
names, archive rewrites, or repository locking. No stable release is justified.

High-priority future gates are exact-byte binary round trips; RCS archive reopen;
conflict preservation; no writes outside the sandbox; root-boundary/path checks;
exclusive locking; interrupted atomic replacement; Unicode and long paths;
Windows reserved names/ADS/reparse behavior; and explicit failed operations that
leave repositories unchanged. Test helpers must remove only generated private
sandboxes, never user repositories.

All source/feature changes described in these inventories are uncommitted and
unpublished audit work until the owner authorizes publication. No product build,
configuration, test, repository mutation or live CVS data experiment was run
locally. Future results must identify the tested SHA instead of attributing
historical failed-run logs to current working-tree code.
