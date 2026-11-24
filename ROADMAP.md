# Portability roadmap

The native and experimental OpenBSD workflows passed for
`a082ae830db901e85d7742baa5f639b33f2ccd73`. That revision exercised GCC,
Clang, MSVC, clang-cl and MinGW-w64, including the Linux ARM64 Debug job,
OpenBSD Debug and Release, and the native matrix's GNU CVS interoperability
tests. Results apply to the recorded revision and test cases.
[Native matrix](https://github.com/daviduhden/opencvs-portable/actions/runs/44953106619)
and [OpenBSD matrix](https://github.com/daviduhden/opencvs-portable/actions/runs/44953106762).

1. Maintain the strict native matrix and verify later revisions by exact SHA.
2. Preserve the regressions for I/O failure, binary archive handling and GNU
   CVS interoperability.
3. Evaluate parser resource limits and filesystem race boundaries against the
   documented deployment model before broader release qualification.
4. Require native package evidence before advertising an additional artifact
   format or architecture.

Earlier failures and their fixes are tracked in the [CI inventories](docs/ci).
