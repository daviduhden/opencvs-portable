---
name: build-and-test
description: Configure and assess Portable OpenCVS manual hosted builds, native test matrices, parser generation, and artifacts.
---

# Build And Test

Use [the build contract](../../../docs/building.md), [tests](../../../docs/testing.md), and [.github/workflows/ci.yml](../../../.github/workflows/ci.yml). Local product configuration, compilation, parser generation and execution are not authorized in the current project policy; do not run a local compiler or container as a workaround.

Dispatch `gh workflow run ci.yml --repo daviduhden/opencvs-portable --ref BRANCH` only for an authorized pushed revision. Native jobs build the versioned date.c. The parser job regenerates separately and checks parity with the recorded byacc package and grammar hash. Inspect exact job logs, CTest case results, sanitizer results and native DLL inspection. Use `gh run view RUN_ID --log-failed` and artifact downloads for evidence. Check core/security/interop coverage and explicit skips, compiler/OS/libc/architecture, and package provenance. Verify the extracted source builds with its own parser. Do not claim Windows from MinGW compilation alone or publish a package from a failed critical test job.

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

Record whether failures vary with destination existence, open handles, or
cleanup order. Compare the smallest existing regression variants before
making another platform-specific change.

Set a concurrency case's subprocess deadline beyond the lock retry interval
when a contender can legitimately sleep before succeeding. Keep the override
local to that case and report how long the contender waited.

## Maintenance & Continuous Learning

After `validate-perl` rewrites files, rerun it for the final static result.
Use lexical Perl handles and bind them to localized names for `open3` direct
file redirection. Preserve binary mode and separate stdout and stderr capture.

Run the native Windows jobs after include sorting. Linux may select libc regex
and never compile the fallback, and the Windows dirent shim can read feature
flags before the rest of the forced compatibility header is processed.

Run Agent Knowledge Validation against the full change range. When code and
guidance land in separate commits, select the commit before both so the
validator can see the implementation and its corresponding instruction updates.
