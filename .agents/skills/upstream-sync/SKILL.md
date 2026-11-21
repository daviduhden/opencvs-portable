---
name: upstream-sync
description: Stage and reconcile selected OpenBSD OpenCVS source revisions while preserving this port and its license provenance.
---

# Upstream Sync

Read [the staging procedure](../../../docs/upstream-sync.md) and the two manifests before selecting a revision. Use `gh api` to inspect the official commit and require its complete SHA. Run [upstream-stage.pl](../../../scripts/upstream-stage.pl) into a new external directory; it provides baseline/candidate trees and overlapping-change flags, not permission to overwrite `src/`.

Review every added/removed source and auxiliary dependency, including Caldera and regex notices. Resolve three-way differences on an isolated branch while preserving the argv/process boundary, Windows wide APIs, feature detection and security fixes. Do not copy all of OpenBSD or reset to a moving master. Regenerate the parser only through the hosted parser job. After a reviewed import, run the complete manual native matrix and update UPSTREAM/manifests with exact evidence. A stage/report or compilation-only pass is not a compatibility certification.

## Maintenance & Continuous Learning

Update this skill when changes invalidate guidance within its scope.
