# Architecture

The imported CVS engine remains in `src/`: command dispatch, working-copy entries,
RCS parsing/storage, diff/merge algorithms, and stdin/stdout CVS framing. The
initial import is traceable through [UPSTREAM](../UPSTREAM) and
[the source manifest](../upstream/manifest.json). Keep semantic changes separate
from platform adaptation so selected OpenBSD changes remain reviewable.

[portable.h](../compat/include/portable.h) is the common declaration boundary.
CMake generates `portable_config.h` from detected features; it is not the engine's
unrelated `src/config.h`. Shared missing libc behavior belongs in
[common.c](../compat/common.c). Exact BSD helper and container imports retain their
own metadata in [the compatibility manifest](../upstream/compat-manifest.json).

[pc_process.h](../compat/include/pc_process.h) defines `pc_spawn`, `pc_wait`,
`pc_pipe`, `pc_run`, and explicit command-to-argv parsing. POSIX and Win32
implementations share that contract; the Win32 implementation uses native handles
rather than a fork imitation. A string accepted as configuration is not permission
to interpret shell operators or command substitutions.

Windows runtime/filesystem adaptation lives under `compat/windows/`; startup and
process entrypoints live under `platform/windows/`. UTF-8 is the engine-facing
encoding and UTF-16 is the Win32 boundary. Binary I/O, path interpretation,
permissions, reparse points, atomic replacement, and handle ownership must have
observable semantics rather than ignored results.

The local process/stream transport shim in tests exercises real CVS protocol
framing while avoiding an SSH daemon. It does not establish encryption,
authentication, host-key verification, or production SSH configuration. GNU CVS
is an independent interoperability peer, never an implementation dependency.

See [portability](portability.md), [security](security.md), and
[testing](testing.md) before changing a cross-platform boundary.
