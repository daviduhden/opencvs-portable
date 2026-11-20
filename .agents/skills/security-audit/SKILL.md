---
name: security-audit
description: Audit Portable OpenCVS parsers, paths, memory, processes, and repository integrity against concrete regression evidence.
---

# Security Audit

Start with [the initial findings](../../../docs/initial-audit.md), [the security model](../../../docs/security.md), and the actual changed callers. Prioritize protocol/RCS input, remote filenames, temporary creation, exclusive locks, symlink/reparse boundaries, integer growth and cleanup after partial failure.

Distinguish a static suspicion from an executed reproducer. Add synthetic malformed-input, binary/empty-file, process-argument, concurrency and failed-write regressions; execute them only on hosted runners with supported sanitizers. Review complete error paths and resource lifetimes rather than search only for unsafe function names. Do not globally disable diagnostics or leak checks to obtain a pass. No seccomp/pledge/unveil/token/Job Object claim is valid without an implemented and tested policy. Never use pledge exec/error or successful no-op security substitutes. Follow [private reporting guidance](../../../SECURITY.md).

For integrity findings, trace the ordering of validation, temporary writes,
flush, close, rename, and metadata publication across RCS, Entries, checkout,
and protocol consumers. Use isolated repositories for CI regressions. Track
remaining races and multi-file consistency limits in `docs/ci/DATA-INTEGRITY.md`.
