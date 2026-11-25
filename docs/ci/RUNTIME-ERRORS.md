# Runtime errors

Static follow-up at `f20b51ce284c14ff4cb478c0264645fe97679304` identified
two lifetime issues. The local release callback could leave the process
in an unversioned operand directory, affecting the next relative operand.
The command argument builder retained its .cvsrc vector and backing storage
through process exit. The local patch restores the starting directory before
return or deletion and makes command cleanup own the default arguments.
Hosted tests for the changed revision are pending.

Evidence cutoff: November 20, 2026. This inventory covers the two accessible
[GitHub Actions runs](https://github.com/daviduhden/opencvs-portable/actions):
[44752958590](https://github.com/daviduhden/opencvs-portable/actions/runs/44752958590)
at `aca3007c760ff359b922144486f7eed3d2de06c3`, and
[44754537542](https://github.com/daviduhden/opencvs-portable/actions/runs/44754537542)
at `1038f5afb14df469723b711e71a76e8a6618f13c`.
Use the linked GitHub Actions runs to retrieve the job logs and artifacts.

Both concluded **failure**. Parser and audit jobs succeeded in both; every native
compilation job failed. No local build, configuration, or test was performed for
this audit. Source changes after those SHAs remain unverified until a new hosted run.

No product runtime was reached by the accessible native jobs. The compile failure
prevents an executable from being considered usable. There are therefore **no
observed product runtime failures or successes** in this evidence set; this does
not imply absence of runtime defects.

Future evidence must retain exit code, stderr/stdout, exact command, platform,
compiler, configuration, failing sandbox path, and source SHA. Reproduce only in
private disposable CVS repositories. Treat hangs, crashes, unexpected success,
and incorrect output as separate failure classes. Native Windows child-process,
Unicode, locking and rename behavior require native Windows runtime tests;
Linux compilation or a PE import listing cannot establish them.

## Follow-up on fd0ed04

Both workflows stopped during compilation. No product crash, sanitizer finding, timeout, or intermittent runtime failure can be determined from these runs.

Evidence: [native matrix 44760299305](https://github.com/daviduhden/opencvs-portable/actions/runs/44760299305) and [OpenBSD 44760299315](https://github.com/daviduhden/opencvs-portable/actions/runs/44760299315), both at `fd0ed043ee5521637c2c9c738f583b8b4a17f2da`. Local changes require a new published revision for validation.

## Follow-up on b79f865

RUN-401. LeakSanitizer reports 40 bytes across two allocations from rcsparse_token through rcsparse_commitid during GNU archive interchange. RUN-402. ASan aborts the deliberate reallocarray-overflow unit case at the allocator. The unit test environment requests NULL on allocator failure and retains sanitizer instrumentation. RUN-403. Import exits after reporting success because Entries cleanup attempts to create a temporary file outside an administrative directory.

Evidence at `b79f865ec6563a6f7943fda4fc607beed16cee76`: [native matrix 44918160599](https://github.com/daviduhden/opencvs-portable/actions/runs/44918160599), [OpenBSD 44918164184](https://github.com/daviduhden/opencvs-portable/actions/runs/44918164184). Local fixes await CI.

## Follow-up on 9d03e23

RUN-501. The Linux sanitizer job reports undefined behavior in diff context pointer arithmetic and signed line hashing. LeakSanitizer also reports 188–286 bytes in separate scenarios, including Entries parsed during file classification and a module record allocated while committing. Distinguish these new reports from the earlier RCS commit-ID leak. Sanitizer job: 136890316342.

Evidence at `9d03e23347299f4b652d26dbb828b7cced1ebea1`: [native matrix 44918836788](https://github.com/daviduhden/opencvs-portable/actions/runs/44918836788) (cancelled overall), [OpenBSD 44918836951](https://github.com/daviduhden/opencvs-portable/actions/runs/44918836951) (failed, jobs 136890189891 and 136890190132). Local changes await CI.

A follow-up static review found unchecked signed arithmetic in decimal date
tokens, relative unit products, and timestamp calculations. Local changes
check arithmetic against the supported signed time_t range before evaluating
it. Both yacc-generated variants still require verification. The normal
relative-day checkout was added to the sticky-date regression.

## Follow-up on ba0ad43

RUN-601. LeakSanitizer still reports 188 or 191 bytes in four allocations in individual cases, with Entries allocated in file classification and then consumed during update or commit. Earlier diff UBSan diagnostics were absent from the retrieved run. Local sanitizer configuration now makes UBSan abort on diagnostics.

Evidence at `ba0ad436b3c0cc0ec04e72f9c98982765cb7c486`: [native matrix 44920499963](https://github.com/daviduhden/opencvs-portable/actions/runs/44920499963) and [OpenBSD 44920500268](https://github.com/daviduhden/opencvs-portable/actions/runs/44920500268). The local changes await CI.

RUN-602. Both Windows Debug jobs reported per-command timeouts in server
cases, while Release completed the corresponding suite much sooner. This
is a reproducible configuration difference in this run, with the root
cause still open. The timeouts were left unchanged.

The Windows replacement diagnostics now report the failing native stage,
errno, and Win32 error. Tests vary whether the source and destination
handles remain open and whether the destination exists. This is diagnostic
coverage. The underlying permission error has yet to be resolved.

## Follow-up on 4646494

RUN-701. Clang LeakSanitizer (136905780394) reports 36 bytes in one revision from rcs_sym_getrev during sticky reset and 95 bytes in three allocations from module expansion. Local changes release the replaced revision and the transient module result. Earlier Entries classification leak signatures were absent from the retrieved logs.

Evidence at `4646494cf36970b37689580587497610321318db`: [native matrix 44923879403](https://github.com/daviduhden/opencvs-portable/actions/runs/44923879403) and [OpenBSD 44923879331](https://github.com/daviduhden/opencvs-portable/actions/runs/44923879331). Local changes await validation.
