# Building and installation

## Execution boundary

During this task, all product configuration, compilation, parser generation,
runtime tests, sanitizers, and packaging runs execute on GitHub-hosted runners.
Do not perform a local build. The commands below define a reproducible interface
for CI and later authorized developer use; they do not supersede that restriction.

## Common interface

Requirements are CMake, a C99-capable compiler, and optionally Ninja. Perl and GNU
CVS are test tools; neither is linked into the product. An existing generated
`generated/date.c` avoids an end-user Yacc dependency. The bootstrap parser CI job
supplies that file and a generator-version record. No OpenSSL/LibreSSL or zlib
library is required by the selected engine revision.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DPC_BUILD_TESTS=ON -DPC_STRICT_WARNINGS=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
cmake --install build --prefix /chosen/prefix
```

Select Debug through `-DCMAKE_BUILD_TYPE=Debug`. Use `-DPC_SANITIZERS=ON` on the
supported GCC/Clang sanitizer runner. Do not globally suppress warnings to make a
new compiler pass. Multi-configuration generators require `--config Release`
for the build/install and `-C Release` for CTest/CPack.

For Windows, the manual matrix uses Visual Studio 2022 x64 with the `v143` or
`ClangCL` toolset and native Strawberry Perl. MinGW-w64 uses UCRT64 compiler tools
with native Windows Perl; MSYS2 is a build environment only. See
[Windows](windows.md) and [Linux](linux.md) for specific runtime boundaries.

CPack generates native ZIP/TGZ artifacts only after their runner tests pass.
Installation/package rules must include the original notices, manuals, and
project documentation. Use the recorded runner/library versions to determine
runtime compatibility; a glibc binary is not a universal Linux binary.
