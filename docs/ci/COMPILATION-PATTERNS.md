# Systemic compilation patterns

Evidence cutoff: November 20, 2026. This inventory covers the two accessible
[GitHub Actions runs](https://github.com/daviduhden/opencvs-portable/actions):
[44752958590](https://github.com/daviduhden/opencvs-portable/actions/runs/44752958590)
at `aca3007c760ff359b922144486f7eed3d2de06c3`, and
[44754537542](https://github.com/daviduhden/opencvs-portable/actions/runs/44754537542)
at `1038f5afb14df469723b711e71a76e8a6618f13c`.
The linked GitHub Actions runs are the source for job logs and artifacts.
The generated diagnostic index is retained locally at
`docs/ci/diagnostic-index.json`. Downloaded evidence resides in `/tmp/opencvs-ci/`.

Both concluded **failure**. Parser and audit jobs succeeded in both; every native
compilation job failed. No local build, configuration, or test was performed for
this audit. Source changes after those SHAs remain unverified until a new hosted run.

1. Shared forced-header errors fan out into every source file. Resolve the first
   genuine declaration failure before treating parser cascades as independent bugs.
2. Configure checks must use the same feature macros and large-file definitions
   as production. A successful symbol lookup without a visible prototype is not
   sufficient for strict C99.
3. Select a fallback implementation and its public header together. Partial native
   availability must not expose the imported regex structure to native libc calls.
4. Feature constants matter: POSIX fnmatch alone does not guarantee FNM_CASEFOLD.
5. Explicit source lists keep platform dependencies auditable; adding a new C file
   must be an intentional CMake change, not an implicit glob side effect.
6. Unsupported sanitizer configurations must fail explicitly. The Windows option
   is no longer silently accepted without sanitizer instrumentation.
7. Ninja `-k 0` gathers independent compile failures while retaining nonzero exit.
   Tee/log pipelines must preserve the compiler's exit status.

These build-system repairs are prepared source changes; their actual results
remain pending a new authorized GitHub run.
