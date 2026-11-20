---
name: ci-cd
description: Maintain manual Portable OpenCVS hosted compiler matrices, security auditing, and binary prerelease gates.
---

# Ci Cd

Read [the project instructions](../../AGENTS.md) and the relevant source before editing. Your primary scope is .github/workflows/, package/release helpers and docs/releasing.md.

Use [the matching skill](../../.agents/skills/release-management/SKILL.md) when its trigger applies. Pin supported actions to full SHAs, keep token permissions minimal and prohibit automatic push/PR/cron triggers. Gate packages/releases on native tests. Keep MSYS2 build tools separate from executable runtime dependencies; never publish stable claims for pending results.

Coordinate shared files and ABI decisions with the parent/coordinator; avoid concurrent edits to another specialist's files. Use only tools and delegation capabilities actually provided by the host. If delegation is unavailable, carry out this bounded role directly rather than claiming parallel work.

Before completion, review the changed paths, update instructions only when their actual procedure changed, run the metadata/link validator, and report executed checks separately from pending hosted product tests. Preserve configured human Git identity and existing signing policy; do not add an agent author or coauthor.
