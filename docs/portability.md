# Portability rules

Keep format/protocol behavior in `src/` and platform semantics behind explicit
interfaces. Feature checks decide whether a libc replacement is needed; OS names
alone do not prove a function or a particular semantic guarantee exists.

Preserve original errors and ownership. A compatibility implementation must define
allocation overflow, zero sizes, EOF, interruption, descriptor/handle lifetime,
file offsets, mode effects, and cleanup. Never return success for an unimplemented
permission, lock, process, limit, or security operation.

The shared process API takes argv rather than shell program text. The Windows
filesystem boundary performs UTF-8/UTF-16 conversion and validates native path
aliases. Linux uses real POSIX facilities. Unsupported representations are errors,
not silent lossy conversions. Keep binary bytes and CVS/RCS framing independent
of host text-mode defaults.

BSD container and libc sources are recorded in
[the compatibility manifest](../upstream/compat-manifest.json). Avoid replacing
system regex headers while linking a different regex ABI. Only the relevant
fallback build should use the imported regex/fnmatch declarations. The BSD getopt
implementation is namespaced on every platform so resets have one meaning.

See [the initial dependency inventory](initial-audit.md) for actual calls; do not
add unused compatibility functions merely because other OpenBSD programs need
them. There is no crypto-library or direct TCP requirement in this import.
