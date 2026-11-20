# Updating the OpenBSD baseline

The baseline is a pinned full SHA, not a moving branch. Imported source paths and
SHA-256 values live in [the source manifest](../upstream/manifest.json) and
[compatibility manifest](../upstream/compat-manifest.json). Preserve the original
notices and `$OpenBSD$` identifiers, including Caldera's separate conditions.

1. Read [UPSTREAM](../UPSTREAM), the manifests, portability changes, and relevant
   tests. Check the working tree before proposing an update.
2. Choose and inspect an exact candidate SHA from official `openbsd/src` using
   `gh api`; a latest branch tip is a proposal, not permission to overwrite.
3. Run the non-destructive maintenance helper (no product build is involved):

   ```sh
   perl scripts/upstream-stage.pl --revision FULL_40_CHARACTER_SHA \
     --output /new/external/staging-directory
   ```

   Omitting `--revision` stages the existing pinned revision, useful for verifying
   provenance. The output directory must be new and outside this working tree.
4. Inspect `report.json`, `baseline/`, and `candidate/`. It records changed/added
   files, removals, local modifications, and overlapping changes needing manual
   three-way review. Unexpected upstream subdirectories or unavailable auxiliary
   files stop staging; investigate rather than broadening imports silently.
5. Prepare an isolated Git branch/worktree. Review upstream changes and licenses
   against the pristine baseline, then resolve the three-way differences while
   retaining intentional portability/security adaptations. The staging helper does
   not apply changes and does not resolve conflicts for you.
6. Review dependencies transitively. Regenerate `date.y` only in the hosted parser
   job and retrieve its artifact; do not install or run a local generator here.
7. Commit reviewable changes with the configured identity/signature. Dispatch the
   full native matrix for that exact revision. Review actual tests, sanitizers,
   interoperability and package results before claiming support.
8. Update UPSTREAM, both manifests, compatibility evidence and release notes only
   after the imported tree and adaptations have been reviewed. Record the import
   SHA/date and file-level changes without overwriting the previous provenance.

Never synchronize by copying the entire OpenBSD tree or resetting a working tree
to a moving upstream branch. Keep original authorship separate from portability
work. No automatic scheduler or destructive synchronization is installed.
