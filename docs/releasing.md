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

The [release workflow](../.github/workflows/release.yml) must run the same complete gated build/test matrix against
an explicitly selected prerelease tag before publishing. It requires a reviewed
existing annotated OpenPGP-signed tag; CI does not receive a private signing key.
Only the publication job receives `contents: write`. Dispatch is manual and
owner-gated; no push, PR or cron event creates a release.

[The tag check](../scripts/release-check.pl) verifies the existing annotated tag and target commit through GitHub; both must be Verified. [The release helper](../scripts/publish-prerelease.pl) validates the prerelease name, selected commit, expected
artifact set and nonempty files, generates SHA-256 checksums and compatibility
notes, and invokes `gh release create --prerelease --verify-tag`. It must stop
instead of overwriting an existing release or silently replacing its assets.
Source archives must include the generated date parser so recipients do not need
Yacc. Preserve every binary/source license notice.

Before dispatch, inspect the exact revision and its evidence, create/push the
reviewed signed prerelease tag through normal authorized Git operations, and
select that tag in the workflow. Publication is an external mutation: an agent
must have explicit release authorization for that specific version. A prior
request to audit or package is not authorization to publish.

Retain run URLs, compiler/libc/runner versions, checksums, per-platform test/skipped
results and known limitations. DEB/RPM and ARM64 output are not advertised until
those artifact formats and architectures have actually been produced and tested.
