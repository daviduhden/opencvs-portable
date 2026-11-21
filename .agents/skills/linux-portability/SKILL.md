---
name: linux-portability
description: Resolve Portable OpenCVS POSIX and libc differences using actual glibc and musl hosted-runner evidence.
---

# Linux Portability

Start with [Linux behavior](../../../docs/linux.md), [the initial API inventory](../../../docs/initial-audit.md), and `compat/common.c`, `compat/linux/process.c`, `compat/include/portable.h`, and the CMake feature configuration. Check availability and semantics separately: GNU libc exposure is not proof of musl availability.

Preserve BSD getopt/optreset namespacing, fgetln stream ownership, checked allocation arithmetic, full I/O, errno, and EINTR-only process waits. Keep native system functions when the required semantics are present. Do not add a blanket GNU-mode dependency or suppress warnings to avoid fixing a type/format error. Builds and product execution belong only in the manual GCC/Clang/musl/sanitizer CI jobs. Report runner architecture and libc separately; do not extrapolate ARM64, Fedora or other distributions from Ubuntu results.

## Maintenance & Continuous Learning

Update this skill when changes invalidate guidance within its scope.
