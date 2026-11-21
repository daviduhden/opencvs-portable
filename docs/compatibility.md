# Compatibility matrix

This matrix records evidence for commit
`8801c7b29a208f45bed53d9d581ae8d99576e57e` from
[native verification 44928779089](https://github.com/daviduhden/opencvs-portable/actions/runs/44928779089)
and [experimental OpenBSD verification 44928781955](https://github.com/daviduhden/opencvs-portable/actions/runs/44928781955).
Comparisons with GNU CVS cover the commands and transport cases exercised on
that revision. Future revisions and prereleases require their own evidence.

| Target | Toolchain and configuration | Result |
| --- | --- | --- |
| Ubuntu 24.04 x86-64, glibc | GCC 13.3.0 Release and Debug | Four CTest suites passed per job. Release TGZ produced. |
| Ubuntu 24.04 x86-64, glibc | Clang 18.1.3 Release | Four suites passed. TGZ produced. |
| Ubuntu 24.04 x86-64, glibc | Clang 18.1.3 Release with ASan/UBSan | Four suites passed. |
| Alpine 3.23 x86-64, musl | GCC 15.2.0 Release | Four suites passed. TGZ produced. |
| Windows Server 2025 x86-64 | MSVC 19.52.37029.0 Release and Debug | Three suites passed per job. PE import check passed. Release ZIP produced. |
| Windows Server 2025 x86-64 | clang-cl 22.1.3 Release and Debug | Three suites passed per job. PE import check passed. Release ZIP produced. |
| Windows Server 2025 x86-64 | MinGW-w64 UCRT64 GCC 16.2.0 Release | Three suites and PE import check passed. ZIP produced. |
| Ubuntu 24.04 ARM64, glibc | GCC 13.3.0 Debug | Four suites passed. ARM64 packaging remains to be verified. |
| OpenBSD 8.0 x86-64 VM | Clang 22.1.6 Release and Debug | Three suites passed per job. Experimental target. |

All four Linux suites include unit, core, security and GNU CVS interoperability.
The Windows and OpenBSD runs cover unit, core and security without a GNU CVS
oracle. The Windows security suite skips the POSIX `RLIMIT_FSIZE`
failure-injection case. Transport interoperability uses the local test shim on
this revision. Package extraction checks and real SSH qualification apply to
subsequent runs after the corresponding tests land.

## Command evidence

Core cases exercise init, import, checkout, add, remove, commit, update, diff,
log, status, tag, annotate, export and conflict handling. GNU CVS tests exercise
archive interchange and both client/server directions. The results apply to the
options and conditions in those cases.

## Inherited limits

- pserver, kserver, gserver, and fork root methods are explicitly rejected by the
  imported client. pserver is not newly implemented by the port.
- The parsed compression option does not imply an implemented compression stream.
  Unsupported nonzero compression must produce a clear error.
- edit, editors, unedit, watch, and watchers are excluded from the original command
  dispatch table. Their source is retained without advertising availability.
- Several optional protocol response types have no handlers. Negotiation and
  error behavior must be tested with the actual peer rather than assumed.
- Windows cannot represent every POSIX name or permission. See
  [native Windows rules](windows.md) for explicit rejection requirements.
- Remote `import -d` requires source timestamp transport and is rejected
  explicitly. Local import can use the source file's modification time.

Local interoperability with GNU-created repositories and both client/server
transport directions need separate tests. Data integrity means comparing bytes,
revisions, tags, metadata, and failure recovery, not just accepting an RCS file.

Local `import -d` uses the source modification time for new vendor revisions.
The remote client reports an error for `import -d` because the port does not
transport source file timestamps. Remote `import -b` sends the selected vendor
branch to the server.

Archive publishing and Entries compaction flush file contents before
replacement. On POSIX, the containing directory is also synchronized and
a failure after publication is reported as such. Win32 flushes file data,
and directory crash durability is unverified. See the
[data integrity inventory](ci/DATA-INTEGRITY.md) for release evidence.
