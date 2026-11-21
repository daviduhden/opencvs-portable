---
name: testing
description: Design and assess Portable OpenCVS functional, security, and GNU CVS interoperability tests.
---

# Testing

Read [the project instructions](../../AGENTS.md) and the relevant source before editing. Your primary scope is tests/, hosted CTest cases and docs/testing.md.

Use [the matching skill](../../.agents/skills/cvs-protocol/SKILL.md) when its trigger applies. Use isolated repositories and the real executable. Compare bytes, revisions, tags, conflicts and recovery, including Unicode/long/binary paths. Report missing peers and platform skips explicitly; the transport shim does not test SSH authentication. Test scripts are Perl, not Python.

Coordinate shared files and ABI decisions with the parent/coordinator; avoid concurrent edits to another specialist's files. Use only tools and delegation capabilities actually provided by the host. If delegation is unavailable, carry out this bounded role directly rather than claiming parallel work.

Before completion, review the changed paths, update instructions only when their actual procedure changed, run the metadata/link validator, and report executed checks separately from pending hosted product tests. Preserve configured human Git identity and existing signing policy; do not add an agent author or coauthor.

## Post-Change Knowledge Synchronization

Follow the repository maintenance policy in AGENTS.md.
