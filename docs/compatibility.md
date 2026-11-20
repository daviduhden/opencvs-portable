# Compatibility matrix

The project starts from the actual imported handlers, not a promise of complete
GNU CVS equivalence. Record successful workflow run URLs and exact SHAs here as
they become available. A pending row must not be described as supported in release
notes or binary labels.

| Target | Required evidence | Current evidence |
| --- | --- | --- |
| Linux x86-64, glibc, GCC | Clean strict build; core/security/interop CTest; package inspection | Pending CI execution |
| Linux x86-64, glibc, Clang | Same native execution gates | Pending CI execution |
| Linux x86-64, Clang sanitizers | ASan/UBSan core/security/interop execution | Pending CI execution |
| Linux x86-64, musl | Native musl build/execution in hosted Alpine container | Pending CI execution |
| Windows x86-64, MSVC | Native build, core/security tests, PE import inspection | Pending CI execution |
| Windows x86-64, clang-cl | Same Windows gates with LLVM toolset | Pending CI execution |
| Windows x86-64, MinGW-w64 | Same Windows gates, no MSYS/Cygwin runtime | Pending CI execution |
| ARM64 | Native build and runtime evidence | Not yet verified |

## Command evidence

The test suite targets init, import, checkout, add, remove, commit, update, diff,
log, status, tag, annotate, export, conflict handling, and local/remote repository
interchange. These remain verification targets until the corresponding cases pass.
Keep option-level limitations in the run report; one passing invocation does not
establish every command option.

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

Local interoperability with GNU-created repositories and both client/server
transport directions need separate tests. Data integrity means comparing bytes,
revisions, tags, metadata, and failure recovery, not just accepting an RCS file.
