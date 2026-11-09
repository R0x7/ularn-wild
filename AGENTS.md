# Rules for AI agents

## Never touch the `original-backup` branch

`original-backup` is a preserved copy of the game before any agent changes.
It must stay exactly as it is.

- Do not check it out to commit, and never commit, merge, rebase, cherry-pick
  or reset onto it.
- Never push to it, force-push it, rename it, or delete it, locally or on
  GitHub.
- Do not open pull requests that target it.
- Reading it (for example `git diff original-backup..main`) is fine.

If a task seems to require changing it, stop and ask the repository owner
instead.

## Quality-of-life changes go in android-ularn, then merge into ularn-wild

There are two apps in two repositories. `main` of
[R0x7/android-ularn](https://github.com/R0x7/android-ularn) builds the base
game, which is meant to stay the original Ularn plus quality-of-life (QoL)
changes only. `main` of [R0x7/ularn-wild](https://github.com/R0x7/ularn-wild)
builds the Wild Edition, which is android-ularn's `main` plus the Wild
features. Both apps should have every QoL change.

(Until November 2026 the Wild Edition was the `ularn-wild` branch of
android-ularn. That branch is no longer updated; work in R0x7/ularn-wild.)

- Make QoL changes (controls, layout, prompts, settings, accessibility,
  build and CI fixes) on android-ularn's `main`, not in ularn-wild.
- Wild-only features go in ularn-wild and never in android-ularn.
- ularn-wild brings android-ularn's `main` in with a merge, never by
  cherry-picking. A merge records what has come over, so the next merge only
  brings the new changes and conflicts are not resolved twice. By hand, in a
  ularn-wild clone:
  `git remote add upstream https://github.com/R0x7/android-ularn`
  (once), then `git fetch upstream && git merge upstream/main && git push`.
- ularn-wild's `Merge upstream main` workflow
  (`.github/workflows/upstream-sync.yml`, ularn-wild only) does this every
  hour. If the merge conflicts, or changes a workflow file (which Actions may
  not push), it opens a pull request in ularn-wild instead; resolve it there
  or merge by hand.
- Shared files (this one, `build.yml`, `README.md`'s base text) are edited in
  android-ularn so the merge stays clean; `build.yml` checks
  `github.event.repository.name` to decide what to publish in each repo.
- If a QoL fix was first made in ularn-wild, also make it in android-ularn,
  then merge android-ularn's `main` into ularn-wild again.
