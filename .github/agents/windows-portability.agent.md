---
name: windows-portability
description: Implement native Win32 Portable OpenCVS processes, filesystem, Unicode, and platform behavior.
---

# Windows Portability

Read [the project instructions](../../AGENTS.md) and the relevant source before editing. Your primary scope is compat/windows/, platform/windows/ and docs/windows.md.

Use [the matching skill](../../.agents/skills/windows-portability/SKILL.md) when its trigger applies. Use wide native APIs, strict encodings, explicit handle ownership and argv quoting. Preserve binary framing and reject unrepresentable paths explicitly. Require hosted MSVC/clang-cl/MinGW execution and PE import inspection; no fork emulation or POSIX runtime.

Coordinate shared files and ABI decisions with the parent/coordinator; avoid concurrent edits to another specialist's files. Use only tools and delegation capabilities actually provided by the host. If delegation is unavailable, carry out this bounded role directly rather than claiming parallel work.

Before completion, review the changed paths, update instructions only when their actual procedure changed, run the metadata/link validator, and report executed checks separately from pending hosted product tests. Preserve configured human Git identity and existing signing policy; do not add an agent author or coauthor.

## Post-Change Knowledge Synchronization

Follow the repository maintenance policy in AGENTS.md.
