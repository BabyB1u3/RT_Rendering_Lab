# Contributing

Thanks for contributing to this repository.

This project follows a focused Git workflow designed to keep `main` stable, keep pull requests easy to review, and keep repository history clean. This document is the day-to-day contribution guide. The full workflow specification lives in `Workflow.md`.

## Development model

- `main` is the only long-lived development branch.
- Start every new task from the latest `main`.
- Keep branches short-lived and single-purpose.
- Keep each pull request focused on one clear story.
- Update related documentation together with code when the change affects setup, configuration, APIs, architecture, or developer workflow.
- Avoid direct commits to `main` except for emergency maintenance or repository administration.

## Branch types

Use one of the following branch types:

- `feature/<scope>-<topic>` for new functionality or non-trivial implementation work
- `fix/<scope>-<topic>` for bug fixes
- `docs/<topic>` for documentation-only work
- `refactor/<topic>` for structural cleanup or technical rework
- `integration/<topic>` only when multiple in-progress branches must be tested together before merging to `main`

### Branch naming rules

Use lowercase letters and hyphens.

Preferred format:

```text
<type>/<scope>-<topic>
```

Examples:

```text
feature/gui-docking-layout
feature/input-config-bindings
fix/config-json-defaults
docs/architecture-overview
refactor/gui-panels
```

Avoid vague or overly broad names such as:

```text
test
misc
update
changes
feature/gui
```

## Starting new work

Always begin from the latest `main`.

### New feature

```bash
git checkout main
git pull
git checkout -b feature/<scope>-<topic>
```

### Bug fix

```bash
git checkout main
git pull
git checkout -b fix/<scope>-<topic>
```

### Documentation-only change

```bash
git checkout main
git pull
git checkout -b docs/<topic>
```

## Scope rules

Each branch and PR should be easy to summarize in one sentence.

Good examples:

* Add configurable hotkey bindings for the input system
* Refactor GUI panels into separate translation units
* Fix JSON default loading for cursor configuration
* Add architecture overview documentation

Avoid mixing unrelated work into one branch or PR. If the change grows beyond one clear story, split it.

## Commit guidelines

Write commit messages that are meaningful and readable in `git log --oneline`.

Preferred style:

* imperative mood
* clear scope
* short but descriptive

Examples:

* `Add DXGI frame source implementation`
* `Refactor GUI control panel into separate class`
* `Fix lock matcher unlock threshold handling`
* `Document JSON config structure`

Avoid messages like:

* `fix`
* `update`
* `wip`
* `more changes`
* `temp`

Commit often enough to save progress, but avoid flooding history with trivial commits.

Before opening a PR, clean up noisy local history if needed:

```bash
git rebase -i main
```

## Keeping your branch up to date

If `main` has moved forward while you are working, update your branch before merging.

Preferred approach for short-lived local branches:

```bash
git fetch origin
git rebase origin/main
```

Alternative only when preserving exact branch history is intentional:

```bash
git fetch origin
git merge origin/main
```

After rebasing a published branch, push safely with:

```bash
git push --force-with-lease
```

## Pull requests

All non-trivial changes should go through a pull request, even in a single-developer workflow.

A pull request should clearly explain:

* what changed
* why the change was needed
* what areas are affected
* whether any user-facing behavior changed
* whether documentation was updated
* how the change was tested

Use the repository PR template and fill it out completely.

Expected PR template structure:

```md
## Summary
-

## Why
-

## Affected areas
-

## Documentation
- [ ] No documentation needed
- [ ] Documentation updated

## Testing
-

## Notes
-
```

Prefer small to medium PRs. If a PR becomes difficult to review in one pass, split it.

## Documentation policy

Current documentation should live on `main`, not on a long-lived separate documentation branch.

Update documentation in the same branch and PR when a change affects:

* setup or build instructions
* configuration format
* command-line usage
* module structure
* APIs or public interfaces
* developer workflow
* architecture diagrams or implementation notes

Use a `docs/<topic>` branch only when the work is independent of an implementation change, such as:

* README improvements
* architecture write-ups
* onboarding guides
* release notes
* documentation restructuring

Recommended documentation structure:

```text
README.md
CONTRIBUTING.md
docs/
  architecture/
  development/
  guides/
  api/
```

## CI and checks

Pull requests to `main` are expected to pass repository checks before merge.

If CI is configured for the repository:

* review failures before merging
* fix broken checks in the same branch
* describe relevant local testing in the PR

If your change affects build, test, configuration, or developer workflow, update related documentation in the same PR.

## Merge policy

The default merge method for this project is **Squash and merge**.

Why:

* it keeps `main` history clean
* one merged PR usually becomes one clear commit
* it avoids noisy branch-local commit history on `main`
* it makes history easier to scan and rollback

Expected squash commit titles should read like polished final commits, for example:

* `Add configurable input bindings`
* `Refactor GUI layout and panel structure`
* `Fix target lock matching edge cases`
* `Document repository workflow`

After a branch is merged:

1. update local `main`
2. delete the merged branch
3. do not continue development on that old branch

```bash
git checkout main
git pull
git branch -D feature/<scope>-<topic>
```

If the remote branch still exists:

```bash
git push origin --delete feature/<scope>-<topic>
```

After a squash merge, an old branch may still appear "ahead" of `main` because Git compares commit history, not only final file content. This is expected. Do not keep using that old branch. Delete it and start fresh from `main`.

## Integration branches

Use an `integration/<topic>` branch only when multiple in-progress branches must be combined for testing before merging into `main`.

Rules:

* integration branches are temporary
* they do not become a second long-lived mainline
* once the combined work is validated, merge cleanly into `main`
* delete the integration branch afterward

## Anti-patterns to avoid

Do not:

* keep long-lived feature branches active for weeks or months without syncing
* continue committing to a branch after it has been squash-merged
* use vague commit messages such as `fix`, `update`, or `wip`
* mix unrelated changes into one PR
* maintain a separate long-lived documentation mainline
* merge feature branches into each other casually
* treat temporary integration branches as permanent development branches

## Daily command reference

### Start a new feature

```bash
git checkout main
git pull
git checkout -b feature/<scope>-<topic>
```

### Check current branch

```bash
git branch --show-current
```

### See concise history graph

```bash
git log --oneline --graph --decorate --all --max-count=30
```

### Rebase onto latest main

```bash
git fetch origin
git rebase origin/main
```

### Push a new branch

```bash
git push -u origin feature/<scope>-<topic>
```

### Safe force-push after rebase

```bash
git push --force-with-lease
```

### Delete local merged branch

```bash
git branch -D feature/<scope>-<topic>
```

### Delete remote merged branch

```bash
git push origin --delete feature/<scope>-<topic>
```

### Compare branch against main

```bash
git diff main...feature/<scope>-<topic>
```

### Clean up noisy local commits before PR

```bash
git rebase -i main
```

## Working agreement

The default operating model for this repository is:

1. branch from `main`
2. keep work focused
3. update related docs together with code
4. open a PR for non-trivial changes
5. squash merge into `main`
6. delete the merged branch
7. start the next task from a fresh `main`
