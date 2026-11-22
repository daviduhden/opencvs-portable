# Portable OpenCVS

Portable OpenCVS is an independent, native C port of OpenBSD's CVS
implementation for Linux and Windows. It preserves the OpenBSD implementation
and CVS/RCS formats rather than substituting GNU CVS code. This project is not
an official OpenBSD project and does not imply OpenBSD endorsement.

## Development status

This is development software, not a stable release. The native implementations,
feature checks, regression suites, and manual CI matrix are being established.
A platform or command is considered verified only when an actual matching CI
run passes; see [the compatibility matrix](docs/compatibility.md) and the run
artifacts. Do not use an unverified build as the only tool protecting important
repositories. Preserve recoverable repository and working-copy backups.

The upstream baseline is OpenBSD `src` revision
`4453305eb9b84bbd8b912a2794d5b2b12caa1b0f`, directory `usr.bin/cvs`.
[UPSTREAM](UPSTREAM) and the [source manifest](provenance/manifest.json) record
provenance. The [initial audit](docs/initial-audit.md) separates inherited issues
from portability changes and identifies limitations already present upstream.

## Platforms and dependencies

The implementation targets Linux with glibc or musl, and native Windows x86-64
with MSVC, clang-cl, or MinGW-w64. Windows uses Win32 process/filesystem APIs and
strict UTF-8/UTF-16 conversion. MSYS2 may provision a MinGW compiler in CI; it is
not an executable runtime dependency. ARM64 and additional operating systems
need their own build and execution evidence before being advertised as verified.

The product is C99-based. Build tools are CMake and an appropriate C compiler;
Ninja is recommended. Perl is test/maintenance infrastructure, not an executable
runtime dependency. External OpenSSH is needed only for SSH remote access. This
upstream revision does not require an OpenSSL/LibreSSL or zlib library.

## Build, test, install, and package

The manual [native portability workflow](.github/workflows/ci.yml) regenerates
the date parser in a temporary directory and compares it with the checked-in
`generated/date.c`. Build jobs and source distributions use the checked-in
parser. Users can build without Yacc. The [parser record](generated/GENERATOR.txt)
identifies the generator and the grammar used to produce it.

Hosted Linux build interface:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DPC_BUILD_TESTS=ON -DPC_STRICT_WARNINGS=ON
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
cmake --install build --prefix /chosen/prefix
cpack --config build/CPackConfig.cmake -G TGZ -B dist
```

For Debug use `-DCMAKE_BUILD_TYPE=Debug`. The sanitizer matrix uses
`-DPC_SANITIZERS=ON`. Windows multi-configuration generators use
`--config Release` and `ctest -C Release`. See [building](docs/building.md),
[Linux](docs/linux.md), and [Windows](docs/windows.md) for compiler-specific
instructions, runtime dependencies, and verified-result boundaries.

## Use

Use a disposable repository before trying valuable data:

```sh
opencvs -d /absolute/path/to/repository init
opencvs -d /absolute/path/to/repository checkout module
```

Windows invocation uses the native `opencvs.exe` binary and Windows paths as
documented in [the Windows guide](docs/windows.md). `opencvs` deliberately has a
different executable name from GNU `cvs`, allowing explicit interoperability
testing without accidentally selecting the wrong implementation.

Original command handlers include import, checkout, add, remove, commit,
update, diff, log, status, tag, annotate, export, and server operations.
Handler presence is not a claim that every option or transport is operational.
[Command and protocol compatibility](docs/compatibility.md) describes test gates
and unsupported upstream features. SSH is the intended remote transport; the
server processes CVS requests on standard streams behind that authenticated
transport. Upstream pserver, kserver, gserver, and fork access methods are not
implemented. Compression is not assumed merely because an option exists.

## Security and maintenance

Read [SECURITY.md](SECURITY.md), [the security model](docs/security.md), and
[the test guide](docs/testing.md). There is no blanket sandboxing claim. Native
path, process, temporary-file and protocol validation are independent of optional
OS restrictions. The project does not implement fake successful `pledge` or
`unveil` substitutes.

Contributors should start with [CONTRIBUTING.md](CONTRIBUTING.md),
[AGENTS.md](AGENTS.md), and [the architecture](docs/architecture.md).
[Upstream synchronization](docs/upstream-sync.md) stages selected revisions for
three-way review; it does not overwrite the working tree. Manual releases are
prereleases until the compatibility and security gates establish stable quality.

## Attribution and licenses

OpenCVS was developed by the OpenBSD project and the authors credited in the
source. Original notices and `$OpenBSD$` identifiers are retained. New portability
work does not transfer ownership of imported code. Newly written code is ISC-licensed under the configured human author's full copyright and license notice; imported code is not relicensed. See [LICENSE](LICENSE),
[the complete upstream notices](LICENSES/Upstream-notices.txt), and per-file
headers for the applicable terms.

The optional BSD regular-expression fallback includes the work of Henry Spencer
(1992–1994), as adapted by OpenBSD and other credited contributors. Portable
integration changes are distinct from that original implementation; its origin
is not attributed to this project's maintainer. The complete additional terms
are preserved in `compat/imported/regex/COPYRIGHT` and the binary notice bundle.

This product includes software developed or owned by Caldera International, Inc.
