---
name: release-management
description: Prepare and publish authorized Portable OpenCVS prereleases using gated native CI, verified artifacts, and checksums.
---

# Release Management

Read [the release procedure](../../../docs/releasing.md) and [compatibility evidence](../../../docs/compatibility.md). Require the exact reviewed tag/commit, an existing annotated OpenPGP-signed prerelease tag, complete passing native matrices, expected architecture/ABI artifacts, original notices and source containing the generated parser. Keep credentials and private signing keys out of CI.

Use the manual release workflow and [the maintained release helper](../../../scripts/publish-prerelease.pl); inspect prepared notes and SHA-256 checksums before the external publication step. Publish only when existing explicit user authorization or an owner dispatch covers that specific tag/assets; do not ask again if it already does. A packaging/audit request alone does not authorize publication. The initial workflow enforces prerelease status and stops on an existing release instead of replacing assets. Never invent signing certificates or label unverified Windows/ARM64/DEB/RPM output stable.
