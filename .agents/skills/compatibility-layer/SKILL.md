---
name: compatibility-layer
description: Design and verify missing libc and platform interfaces used by Portable OpenCVS while preserving upstream semantics.
---

# Compatibility Layer

Read [the architecture](../../../docs/architecture.md), [portability rules](../../../docs/portability.md), and `compat/include/portable.h`/`pc_process.h`. Confirm a real call site and failed feature probe before adding a fallback. Add provenance to the compatibility manifest for imported code and preserve original notices.

Keep prototypes and ownership precise. In particular, `pc_fgetln` is stream-associated and cleanup uses `pc_fclose`; a single shared getline buffer is not equivalent. Check size addition as well as multiplication. Missing permissions, locking, limits or sandboxing must return documented errors rather than fake success. Keep Windows socket/handle and POSIX descriptor semantics distinct. Extend meaningful behavior tests and run them on hosted targets; metadata/header compilation alone does not certify runtime equivalence.

For each new or changed code file, embed the complete `LICENSES/ISC.txt` notice
in its comment syntax. Scope the added copyright to portability changes in
imported files and retain original notices. Run `perl scripts/check-license-notices.pl`;
a short SPDX marker does not replace the full header.

## Maintenance & Continuous Learning

Update this skill when changes invalidate guidance within its scope.
