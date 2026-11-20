# Security model and audit procedure

This is an in-development CVS implementation processing attacker-influenced
filenames, working-copy metadata, protocol messages, RCS syntax and deltas.
Repository administration files, editors, environment variables and subprocess
transport configuration are additional trust boundaries. SSH protects the
connection; it does not validate those payloads or choose an authorized server
repository root by itself.

[The initial audit](initial-audit.md) identifies inherited prefix-validation,
file-creation-mode, same-user lock, shell-command, zero-length mmap and allocation
addition hazards. Track remediation through code review and behavioral regression
results, not a checkbox that merely searches for a new function name.

Required properties include checked size addition/multiplication, complete I/O or
an explicit failure, bounded parser growth, failure-safe temporary paths, exclusive
lock ownership, no symlink/reparse-point escapes, strict Unicode conversion,
structured argv execution, and correct child/handle cleanup. An error after a
partial write must not be hidden as success. Preserve original data or make the
failure/recovery boundary explicit.

On Windows, validate native aliases and use real Win32 errors and ownership-aware
handle inheritance. On Linux, preserve POSIX return values and retry only genuinely
interrupted operations. A retry loop must not turn a permanent error into an
infinite loop. Do not equate Windows read-only bits with complete POSIX permissions.

No seccomp, restricted-token, Job Object, pledge, or unveil sandbox is claimed
merely because an API exists. A future optional mechanism needs native tests and
an explicit policy for subprocess/editor/hook operations. Do not use pledge
promises `exec` or `error`; do not implement empty successful pledge/unveil stubs
on other systems. Common memory and path safety must stand independently.

Run sanitizers and malformed-input tests on hosted runners. Future fuzz targets
should cover RCS token/string/delta parsing and CVS request/response boundaries,
with time and allocation budgets. Static analysis cannot establish absence of
races or data-loss bugs; cross-process lock and failed-write regressions matter.
Use the reporting channel described in [SECURITY.md](../SECURITY.md).
