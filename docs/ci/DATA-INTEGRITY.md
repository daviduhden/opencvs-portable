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

## Follow-up on fd0ed04

**DATA-301.** Sticky dates in Entries and Tag previously used independent numeric scans. A value that parses but normalizes to a different calendar date can select the wrong revision. Both readers now validate field bounds and verify the UTC round trip. The prepared regression checks that an invalid calendar date leaves Tag, Entries, and working bytes intact. CI verification is pending.

Evidence: [native matrix 44760299305](https://github.com/daviduhden/opencvs-portable/actions/runs/44760299305) and [OpenBSD 44760299315](https://github.com/daviduhden/opencvs-portable/actions/runs/44760299315), both at `fd0ed043ee5521637c2c9c738f583b8b4a17f2da`. Local changes require a new published revision for validation.

## Follow-up on b79f865

DATA-401. Repeated init hits EEXIST opening checkoutlist. Preserve existing repository files instead of truncating or regenerating them. DATA-402. An import source is not a working copy and must not gain Entries during cleanup. Review other read-only archive and directory walks for the same ownership issue.

Evidence at `b79f865ec6563a6f7943fda4fc607beed16cee76`: [native matrix 44918160599](https://github.com/daviduhden/opencvs-portable/actions/runs/44918160599), [OpenBSD 44918164184](https://github.com/daviduhden/opencvs-portable/actions/runs/44918164184). Local fixes await CI.

## Follow-up on 9d03e23

DATA-501. OpenBSD accepts committing an unresolved merge conflict. Its archive must remain unchanged until the working conflict is resolved. DATA-502. Windows archive rename failures require checking the replacement lifecycle and retaining the old archive on error. No successful Windows archive replacement is inferred.

Evidence at `9d03e23347299f4b652d26dbb828b7cced1ebea1`: [native matrix 44918836788](https://github.com/daviduhden/opencvs-portable/actions/runs/44918836788) (cancelled overall), [OpenBSD 44918836951](https://github.com/daviduhden/opencvs-portable/actions/runs/44918836951) (failed, jobs 136890189891 and 136890190132). Local changes await CI.

The local commit check now counts unresolved conflict markers as a blocking
conflict. The existing lifecycle case exercises rejection followed by a
resolved commit. Classification now releases earlier Entries and RCS state
before rebuilding it, and commit releases its module lookup result.

Follow-up static review found that Win32 treats zero file attributes as
“leave unchanged.” The read-only transition now uses NORMAL when no other
attributes remain. Rename reports restoration failures. If the move succeeds
and source metadata restoration fails, the new name already exists and its
attributes require review. This sequence is not a transactional metadata
replacement. CI verification remains pending.
