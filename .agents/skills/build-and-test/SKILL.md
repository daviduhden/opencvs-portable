---
name: build-and-test
description: Configure and assess Portable OpenCVS manual hosted builds, native test matrices, parser generation, and artifacts.
---

# Build And Test

Use [the build contract](../../../docs/building.md), [tests](../../../docs/testing.md), and [.github/workflows/ci.yml](../../../.github/workflows/ci.yml). Local product configuration, compilation, parser generation and execution are not authorized in the current project policy; do not run a local compiler or container as a workaround.

Dispatch `gh workflow run ci.yml --repo daviduhden/opencvs-portable --ref BRANCH` only for an authorized pushed revision. The parser job supplies date.c; consumers do not need Yacc. Inspect exact job logs, `ctest --output-on-failure`, sanitizer results and native DLL inspection. Use `gh run view RUN_ID --log-failed` and artifact downloads for evidence. Check core/security/interop coverage and explicit skips, compiler/OS/libc/architecture, and package provenance. Do not claim Windows from MinGW compilation alone or publish a package from a failed critical test job.

For a CI audit, retrieve paginated runs, job logs, and artifacts with `gh`.
Record compiler, target, configuration, and exact SHA in `docs/ci/`. Group
diagnostic cascades by root cause, then review equivalent call sites on every
target. Record jobs blocked before tests separately from executed failures.

When configuration succeeds but an executable fails to build, record the
last completed target. A compatibility archive or a generated parser artifact
does not establish that the full executable linked or that CTest started.

When many cases share a setup or cleanup failure, record the first failing
operation and separate it from independent assertions. Preserve the original
cases so the next run exposes downstream failures once setup works.

Record the effective test identity when checking GNU CVS interoperability.
Validate a known-good archive fixture before interpreting its negative cases.

Check real output files after commands that report success. For wire
interoperability, verify the test transport's argument contract separately
from protocol and repository failures.
