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

## Windows HTML Help

The `windows-help` target builds only the manual. It writes
`build/html-help/opencvs.chm`, the compiler log and the input project under the
build directory. Building the program itself does not require the help tools.

```sh
cmake --build build --target windows-help
```

To run just the help pipeline on the documentation producer, without
configuring the C build, use CMake script mode.

```sh
cmake -DPC_CHM_OUTPUT_DIR="$PWD/build/html-help" -P cmake/BuildHtmlHelp.cmake
```

Install Perl, `mandoc`, 7-Zip and either Microsoft HTML Help Workshop
(`hhc.exe`) or Free Pascal `chmcmd`. The scripts discover the tools on the
build host. Use `PC_CHM_PERL`, `PC_CHM_MANDOC`, `PC_CHM_7ZIP` or
`PC_CHM_COMPILER` to select an executable explicitly. The manual CI and
release workflows use distro-packaged Free Pascal on Ubuntu, where the same
job can lint the manuals with `mandoc` and compile Windows HTML Help with
full-text indexing. [Free Pascal CHM tools](https://wiki.freepascal.org/Chm)

Add new `mdoc` or `man` sources using a section suffix such as `.1`, `.5` or
`.3p`. The shared inventory discovers them for CHM and Unix installation.
Use the standard manual cross-reference macros for links to other manuals.
The generator derives the section tree and alphabetical entries from topic
names and headings. References to manuals outside this project retain their
text. The project's CMake version is displayed on the start page. Sorted
inputs and generated project text are reproducible. Compilers can embed their
own metadata in the resulting CHM.

The generator checks manual syntax, topic coverage, local targets and static
markup. To run these checks without the CHM compiler, use
`cmake -DPC_CHM_LINT_ONLY=ON -P cmake/BuildHtmlHelp.cmake`. After compilation,
the verifier opens and tests the container using
7-Zip and checks topic bytes, navigation and the full-text search stream.
Review `compiler.log` and open the final package on Windows to check the
contents, index, search and cross-reference display in the viewer.

Windows installation and CPack require the generated CHM. Set
`PC_CHM_FILE` at configuration time to use a prebuilt, verified CHM from the
same source revision. The Windows CI jobs do this with the shared help
artifact. Unix installs use the original manuals.

## Packages

CPack generates native ZIP/TGZ artifacts after the runner tests pass. Package
jobs extract the archives and run the contained executable on the native host.
The archive includes the original notices, manuals, documentation and build
metadata. `opencvs -v` reports the inherited engine version, currently
`OpenCVS 4.5`. Use `build-info.txt` to identify the Portable OpenCVS package
version configured in CMake and the source revision recorded by CI. Use the
artifact's ABI, runner and library records to determine runtime compatibility.
