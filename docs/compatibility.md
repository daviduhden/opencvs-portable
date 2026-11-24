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

## OpenBSD compatibility policy

Portable OpenCVS follows OpenBSD OpenCVS. GNU CVS is an interoperability peer
for shared repository formats, commands and wire behavior. OpenBSD's
[project goals](https://www.openbsd.org/opencvs/goals.html) put security ahead
of GNU CVS compatibility. An explicit upstream exclusion applies across
platforms and build configurations.

The fidelity audit compared the
[imported revision](https://github.com/openbsd/src/tree/4453305eb9b84bbd8b912a2794d5b2b12caa1b0f/usr.bin/cvs)
with the
[OpenBSD master snapshot](https://github.com/openbsd/src/tree/5aaa8357582f2b2db6cb2aa2b0868f8274c3242f/usr.bin/cvs).
The directory entries and Git blob IDs matched. The last change to the subtree
in that snapshot was [7a78785](https://github.com/openbsd/src/commit/7a78785949c7ff8c2b6ad6430301a10431b26212).

### Deliberate upstream decisions

- OpenBSD dropped pserver because of its authentication mechanism. The port
  recognizes the root method so it can reject it before starting a transport.
  The server processes CVS requests over standard streams with authentication
  supplied externally, normally by SSH. See [OpenCVS cvsintro(7)](https://man.openbsd.org/cvsintro)
  and [the upstream pserver documentation removal](https://github.com/openbsd/src/commit/e9a6eff593a764542e2ae8642e553e8d64b3b47c).
- OpenBSD removed `login` and `logout` and recorded that they would remain
  unavailable. See [the command removal](https://github.com/openbsd/src/commit/11aa5671a5b0ebd930fae49469caddea384e0cd3).
- OpenBSD removed passing a port parsed from CVSROOT to the remote shell.
  The first portability commit reintroduced that behavior. The port retains
  upstream root syntax and port validation while leaving SSH port selection to
  SSH configuration or explicit options in `CVS_RSH`.
  See [the upstream removal](https://github.com/openbsd/src/commit/47c33804566cbc2c0b2589cdf34fcb81ef470a89)
  and [the first portability commit](https://github.com/daviduhden/opencvs-portable/commit/840919365e412c9c1a0726de8e47911235c0a233).
- OpenBSD retains global `-x` and `-b` for command-line compatibility. They
  are no-ops in the global option parser. Its trigger substitution explicitly
  rejects `RCSBIN`. Repository hooks and editors remain available through
  explicit argument vectors in the port.
  See [upstream option parsing](https://github.com/openbsd/src/blob/4453305eb9b84bbd8b912a2794d5b2b12caa1b0f/usr.bin/cvs/cvs.c)
  and [trigger substitutions](https://github.com/openbsd/src/blob/4453305eb9b84bbd8b912a2794d5b2b12caa1b0f/usr.bin/cvs/trigger.c).
- OpenBSD restricts `admin -s` to `Exp` and `dead`. Its history cites
  GNU CVS state changes that another invocation could not undo. See
  [the upstream state restriction](https://github.com/openbsd/src/commit/feca7f66fb14d7d1576bd719ea47b0915595341d).
- OpenBSD removed quote interpretation from its configuration argument
  tokenizer and removed the wire option `Global_option -V`. The port keeps
  both decisions. The separate tokenizer for operator-selected external
  programs still constructs explicit argument vectors.
  See [configuration tokenization](https://github.com/openbsd/src/commit/e6a55d17643b058d4cf16b28b27a4b18154aa11e)
  and [wire option removal](https://github.com/openbsd/src/commit/a694c629a2a4662135c570ad82baa67fd6df5452).
- OpenBSD added `-C` and `-U` to `diff` while leaving them out of `rdiff`.
  The separate option tables preserve this behavior.
  See [the original option change](https://github.com/openbsd/src/commit/72026f1ae0ba0d183d8acb0dfd804d7cdfa7b45e).

### Unsupported behavior and portability differences

- The client explicitly rejects kserver, gserver and fork. The same dispatch
  block covers pserver, whose security rationale is documented separately.
  The upstream manual's description of a `kserver` command has no matching
  command in the dispatch table.
- The global parser accepts `-z` levels from 0 to 9. The value has no
  consumer in either source tree, so it does not enable compression.
  The server rejects the unsupported `Gzip-stream` request.
  [OpenBSD removed the unused compression files pending an implementation](https://github.com/openbsd/src/commit/66b0b6623eb12ea21576c626c38f9de3d3b0dee9).
- The original dispatch table disables edit, editors, unedit, watch and
  watchers. Its source establishes the disabled state. The reason for
  disabling these commands is unconfirmed.
- Several optional protocol response types have no handlers. Negotiation and
  error behavior must be tested with the actual peer rather than assumed.
- The repository config recognizes `tag`, `umask` and `dlimit`, as in the
  imported implementation. The port's process and filesystem protections do
  not provide the syscall confinement of upstream OpenBSD's startup pledge.
  See [the security model](security.md).
- Global `.cvsrc` options use the normal option parser. The port keeps the
  option strings alive through command cleanup so that a root loaded from
  `.cvsrc` reaches the same transport checks as a command-line root.
- Windows cannot represent every POSIX name or permission. See
  [native Windows rules](windows.md) for explicit rejection requirements.
- Remote `import -d` requires source timestamp transport and is rejected
  explicitly. Local import can use the source file's modification time.

Local interoperability with GNU-created repositories and both client/server
transport directions need separate tests. Data integrity means comparing bytes,
revisions, tags, metadata, and failure recovery, not just accepting an RCS file.
The GNU CVS cases exercise shared operations over the existing protocol or
through the same RCS archives. Root-relative checkout responses and an empty
Entries timestamp for a modified upload preserve that exchange. RCS rewrites
preserve existing commit IDs already recognized by the upstream parser.
OpenBSD itself adopted [GNU-compatible Entries.Log records](https://github.com/openbsd/src/commit/d2442498516f1abb709a0dad4bd715df4f47f3e2)
and [file mode inheritance](https://github.com/openbsd/src/commit/c51cb395e54dc82a1b1d7391bf27f9373c5fe47a).
Native UTF-8/UTF-16 conversion, process spawning, locking and file replacement
preserve platform contracts. The operator-provided server root restriction
narrows access independently of transport authentication.

Local `import -d` uses the source modification time for new vendor revisions.
The remote client reports an error for `import -d` because the port does not
transport source file timestamps. Remote `import -b` sends the selected vendor
branch to the server.

Archive publishing and Entries compaction flush file contents before
replacement. On POSIX, the containing directory is also synchronized and
a failure after publication is reported as such. Win32 flushes file data,
and directory crash durability is unverified. See the
[data integrity inventory](ci/DATA-INTEGRITY.md) for release evidence.
