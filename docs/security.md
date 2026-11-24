# Security model and audit procedure

This is an in-development CVS implementation processing attacker-influenced
filenames, working-copy metadata, protocol messages, RCS syntax and deltas.
Repository administration files, editors, environment variables and subprocess
transport configuration are additional trust boundaries. SSH protects the
connection; it does not validate those payloads or choose an authorized server
repository root by itself.

For a restricted SSH service, set `OPENCVS_SERVER_ROOT` in the trusted server
launcher to one existing absolute repository path, with no trailing directory
separator. Clients must request that exact spelling. The restriction also blocks
remote initialization. Keep this variable under the launcher's control and keep
the repository path ancestors and administrative configuration under trusted
ownership. With the variable unset, the service account's OS permissions govern
repository access. Use separate accounts or launchers for distinct access scopes.

Client publication checks existing path components, then opens and renames
files by path. Keep concurrent writers to parent directories outside that
trust boundary. A descriptor-relative POSIX pilot should hold the verified
parent through temporary creation, replacement and directory sync, and
preserve lock ownership and failure reporting. On Win32, first verify
handle-relative rename on the hosted filesystem. The current backend
records an earlier native failure with that mode. Neither platform should
claim protection from parent replacement until those checks pass.

Each server process accepts up to one million requests, 256 MiB of protocol
metadata and 64 GiB of aggregate input. Individual text lines retain the 16 MiB
limit, and individual uploads retain the signed 32-bit length limit. These caps
apply per session. Use service-level disk and process quotas for aggregate usage
across sessions.

## Operator recovery

If a diagnostic says an archive was published before a later sync or cleanup
failure, pause writers to the affected repository and preserve a backup of the
remaining archives. For an Attic transition, inspect both archive paths.
Compare the expected revision and bytes in a fresh checkout before retrying
the write. A transport failure after an acknowledgement also warrants checking
what reached the repository before retrying.

When a command reports a repository lock wait, identify any active writer and
let it finish. If the lock persists after a process exits, keep writers paused
and confirm the lock has no live owner before treating it as stale. Inspect
the affected archives for partial publication before removing a stale lock.

An administrator can configure `dlimit` in `CVSROOT/config`. Confirm the
effective limit under the same service launcher used in deployment. POSIX
uses `RLIMIT_DATA` and Windows uses a per-process committed-memory limit in
a Job Object. Apply service-level limits separately for several concurrent
sessions. See [Windows resource controls](windows.md#dates-and-resource-controls).

## Engineering checks

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
