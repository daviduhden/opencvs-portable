# Releases and binary artifacts

Initial releases are prereleases. Do not mark a version stable while native
Windows, interoperability, security or data-integrity gates fail or remain
unverified. A compiler pass alone is not a release qualification.

The manual native matrix packages successful target builds with CPack. Artifact
names identify source SHA, architecture, ABI/platform and compiler. Linux glibc
and musl archives are separate; neither is a universal Linux binary. Windows ZIPs
must include `opencvs.exe`, the original notices, documentation and version data,
and must pass native PE import inspection. No Authenticode certificate is created
or assumed.

Use `build-info.txt` to identify the configured Portable OpenCVS package
version and the source revision. The selected signed tag identifies the hosted
prerelease. The output of `opencvs -v` identifies the inherited OpenCVS engine.

The [release workflow](../.github/workflows/release.yml) gates the selected
prerelease tag on Linux x86-64 Release (glibc GCC, Clang, Clang with
sanitizers and musl) and Windows x86-64 Release (MSVC, clang-cl and MinGW).
GNU CVS interoperability is required in each Linux job. Debug, ARM64 and
experimental OpenBSD evidence comes from the separate manual workflows.
Release requires a reviewed existing annotated OpenPGP-signed tag.
CI does not receive a private signing key.
Only the publication job receives `contents: write`. Dispatch is manual and
owner-gated. Leave `publish` false to run the full signed-tag qualification
and download the manifests, notes and checksums from `release-qualification`.
An explicitly authorized dispatch with `publish` true can publish only after
the same qualification passes.

[The tag check](../scripts/release-check.pl) verifies the existing annotated tag and target commit through GitHub; both must be Verified. [The release helper](../scripts/publish-prerelease.pl) validates the prerelease name, selected commit, expected
artifact set and nonempty files, generates SHA-256 checksums and compatibility
notes, and can invoke `gh release create --prerelease --verify-tag`.
Each producer records the validated archive's digest, source SHA, ABI and
compiler. Qualification and publication compare these values with the received
archive and the selected target. Publication rechecks the annotated tag object
and its target immediately before creating the release and stops if a release
already exists.
The parser job compares the checked-in output with the pinned generator.
Native package jobs extract the actual archive, compare the contained
executable with the tested binary and run it. The source package job
validates notices and builds and runs the extracted source without Yacc.

Before dispatch, inspect the exact revision and its evidence, create/push the
reviewed signed prerelease tag through normal authorized Git operations, and
select that tag in the workflow. Publication is an external mutation: an agent
must have explicit release authorization for that specific version. A prior
request to audit or package is not authorization to publish.

Retain run URLs, compiler/libc/runner versions, checksums, per-platform test/skipped
results and known limitations. DEB/RPM and ARM64 output are not advertised until
those artifact formats and architectures have actually been produced and tested.
