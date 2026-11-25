# Architecture

The imported CVS engine remains in `src/`: command dispatch, working-copy entries,
RCS parsing/storage, diff/merge algorithms, and stdin/stdout CVS framing. The
initial import is traceable through [UPSTREAM](../UPSTREAM) and
[the source manifest](../provenance/manifest.json). Keep semantic changes separate
from platform adaptation so selected OpenBSD changes remain reviewable.

[portable.h](../compat/include/portable.h) is the common declaration boundary.
CMake generates `portable_config.h` from detected features; it is not the engine's
unrelated `src/config.h`. Shared missing libc behavior belongs in
[common.c](../compat/common.c). Exact BSD helper and container imports retain their
own metadata in [the compatibility manifest](../provenance/compat-manifest.json).

[pc_process.h](../compat/include/pc_process.h) defines `pc_spawn`, `pc_wait`,
`pc_pipe`, `pc_run`, and explicit command-to-argv parsing. POSIX and Win32
implementations share that contract; the Win32 implementation uses native handles
rather than a fork imitation. A string accepted as configuration is not permission
to interpret shell operators or command substitutions.

Windows runtime/filesystem adaptation lives under `compat/windows/`; startup and
argument conversion live in `compat/windows/startup.c`, and child process
creation lives in `platform/windows/process.c`. UTF-8 is the engine-facing
encoding and UTF-16 is the Win32 boundary. Binary I/O, path interpretation,
permissions, reparse points, atomic replacement, and handle ownership must have
observable semantics rather than ignored results.

## Command and data flow

- Startup initializes the platform adapter and builds the command arguments
  before dispatch. Server mode starts with its own workspace. Other
  commands resolve a repository root, then load local repository configuration
  or use the remote client. [cvs.c](../src/cvs.c)
- The walker supplies file and directory callbacks with working-copy and
  repository context. Entries owns the per-directory working-copy metadata.
  The repository layer enumerates archives and tracks writer locks. Working
  paths and archive paths can have different prefixes during checkout.
  [file.c](../src/file.c), [entries.c](../src/entries.c), [repository.c](../src/repository.c)
- The RCS parser reads archive metadata and loads revision text on demand.
  The archive layer reconstructs revisions and can publish changes when a
  writable archive is closed. Keep the caller's writer lock until publication
  or a publishing close has finished. [rcsparse.c](../src/rcsparse.c), [rcs.c](../src/rcs.c)
- The client sends working-copy state and applies responses. The transport
  handles protocol lines and length-delimited file bodies. The server maps
  requests into its temporary working area and invokes command code against
  the repository. The client checks command completion and transport exit
  status separately. [client.c](../src/client.c), [remote.c](../src/remote.c), [server.c](../src/server.c)

## Test boundaries

The local process/stream transport shim in tests exercises real CVS protocol
framing while avoiding an SSH daemon. It does not establish encryption,
authentication, host-key verification, or production SSH configuration. GNU CVS
is an independent interoperability peer, never an implementation dependency.

See [portability](portability.md), [security](security.md), and
[testing](testing.md) before changing a cross-platform boundary.
