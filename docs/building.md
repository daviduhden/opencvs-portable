# Building and installation

## Execution boundary

During this task, all product configuration, compilation, parser generation,
runtime tests, sanitizers, and packaging runs execute on GitHub-hosted runners.
Do not perform a local build. The commands below define a reproducible interface
for CI and later authorized developer use; they do not supersede that restriction.

## Common interface

Requirements are CMake, a C99-capable compiler, and optionally Ninja. Perl and GNU
CVS are test tools, separate from the compiled product. The included date parser
allows users to build without Yacc. The manual parser job checks the checked-in
output against the pinned generator and records the grammar digest. The selected
engine revision builds without OpenSSL, LibreSSL or zlib.

The portable build uses the root `CMakeLists.txt` on all hosted platforms,
including OpenBSD. The inherited `src/Makefile` describes the upstream BSD
build through `bsd.prog.mk` and lists the grammar as a build input. Use the
root build to exercise feature probes, platform adapters and the versioned
parser used for portable packages.

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

For Windows, the manual matrix discovers the installed Visual Studio x64 SDK
with `vswhere`, initializes its developer environment, and uses Ninja with
`cl` or `clang-cl` and native Strawberry Perl. MinGW-w64 uses UCRT64 compiler tools
with native Windows Perl; MSYS2 is a build environment only. See
[Windows](windows.md) and [Linux](linux.md) for specific runtime boundaries.

CPack generates native ZIP/TGZ artifacts after the runner tests pass. Package
jobs extract the archives and run the contained executable on the native host.
The archive includes the original notices, manuals, documentation and build
metadata. `opencvs -v` reports the inherited engine version, currently
`OpenCVS 4.5`. Use `build-info.txt` to identify the Portable OpenCVS package
version configured in CMake and the source revision recorded by CI. Use the
artifact's ABI, runner and library records to determine runtime compatibility.
