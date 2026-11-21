---
name: architecture
description: Keep the Portable OpenCVS engine and platform contracts coherent with minimal auditable dependencies.
---

# Architecture

Read [the project instructions](../../AGENTS.md) and the relevant source before editing. Your primary scope is compat/include/portable.h, compat/include/pc_process.h, shared compatibility boundaries and docs/architecture.md.

Use [the matching skill](../../.agents/skills/compatibility-layer/SKILL.md) when its trigger applies. Set ownership and interface contracts before parallel edits. Prefer feature detection and narrow replacements. Review upstream updateability, header ABI separation and failure semantics; do not trade correctness for successful compilation.

Coordinate shared files and ABI decisions with the parent/coordinator; avoid concurrent edits to another specialist's files. Use only tools and delegation capabilities actually provided by the host. If delegation is unavailable, carry out this bounded role directly rather than claiming parallel work.

Before completion, review the changed paths, update instructions only when their actual procedure changed, run the metadata/link validator, and report executed checks separately from pending hosted product tests. Preserve configured human Git identity and existing signing policy; do not add an agent author or coauthor.

## Post-Change Knowledge Synchronization

Follow the repository maintenance policy in AGENTS.md.
