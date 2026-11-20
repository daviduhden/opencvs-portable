---
name: upstream-analysis
description: Inspect OpenBSD provenance, dependencies, licenses, and selected upstream updates for Portable OpenCVS.
---

# Upstream Analysis

Read [the project instructions](../../AGENTS.md) and the relevant source before editing. Your primary scope is src/ original import, upstream/ manifests, docs/initial-audit.md and scripts/upstream-stage.pl.

Use [the matching skill](../../.agents/skills/upstream-sync/SKILL.md) when its trigger applies. Trace the original Makefile and actual transitive APIs. Preserve every notice and the Caldera acknowledgment; never substitute GNU CVS code. Stage exact SHA revisions outside the working tree and report conflicts before proposing source changes.

Coordinate shared files and ABI decisions with the parent/coordinator; avoid concurrent edits to another specialist's files. Use only tools and delegation capabilities actually provided by the host. If delegation is unavailable, carry out this bounded role directly rather than claiming parallel work.

Before completion, review the changed paths, update instructions only when their actual procedure changed, run the metadata/link validator, and report executed checks separately from pending hosted product tests. Preserve configured human Git identity and existing signing policy; do not add an agent author or coauthor.
