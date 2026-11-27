# Contributing to the ST plugins

Thanks for your interest! These notes apply to **ST Shell** and every plugin built on it
([ST Sat](https://github.com/davidstokarski/st-sat), [ST EQ](https://github.com/davidstokarski/st-eq),
[ST DC](https://github.com/davidstokarski/st-dc), [ST Comp](https://github.com/davidstokarski/st-comp),
[ST Split](https://github.com/davidstokarski/st-split), [ST Sample](https://github.com/davidstokarski/st-sample)).

## How changes land

- `main` is protected. Nobody can push to it directly except the maintainer.
- Every change goes through a pull request from a fork (or a branch, for collaborators).
- A pull request can only merge after it is approved by the code owner
  (@davidstokarski or @david-stokarski) and all review conversations are resolved.
  New commits after an approval dismiss it, so the PR needs a fresh review.
- Force-pushes to `main` and deleting `main` are blocked.
- GitHub Actions on PRs from first-time/outside contributors wait for maintainer approval.

## Before you open a PR

1. **Open an issue first** for anything bigger than a small fix, so we can agree on the approach.
2. Read [CLAUDE.md](CLAUDE.md) and the docs in [docs/](docs/): especially
   [DESIGN.md](docs/DESIGN.md) (the visual language every plugin shares),
   [ARCHITECTURE.md](docs/ARCHITECTURE.md) and [QUALITY.md](docs/QUALITY.md) (the verification bar).
3. Keep shell changes generic. Anything plugin-specific belongs in that plugin's repo, not in `modules/st_shell`.
4. Build and run the tests (and UI snapshots / `auval` where relevant) — see each repo's README.
5. Keep PRs focused: one change per PR, with a clear description of what and why, and screenshots for UI changes.

## Style

- C++20, JUCE 9. Match the surrounding code's formatting and naming (namespace `st` in the shell).
- No allocations, locks or logging on the audio thread.
- Don't commit build output (`build*/`, `dist/`) or secrets.

## Reporting security issues

Please **don't** open a public issue. Use GitHub's
[private vulnerability reporting](https://github.com/davidstokarski/st-shell/security/advisories/new)
on the affected repo instead.

## Licensing

Unless a repo has a LICENSE file saying otherwise, all rights are reserved by the author. By submitting a
pull request you agree your contribution may be used, modified and distributed as part of the project.
