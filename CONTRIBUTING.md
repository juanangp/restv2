# Contributing to restv2

## Workflow

`master` is protected: it only changes through pull requests whose validation passes.

1. Create a branch from `master` (`git switch -c <user>-<topic> master`).
2. Commit the changes (one-line commit messages) and push the branch.
3. Open a pull request to `master` (as draft while the work is in progress), filling the template.
4. The validation (`.github/workflows/validation.yml`) runs on the pull request, and again at each push to
   it: the checks **Build**, **Unit tests**, **restG4 examples** and **Format** must pass, and the branch must
   be up to date with `master`, before merging. The **macOS** check (build and unit tests with the ROOT of
   Homebrew, without Geant4 or Garfield++) also runs, but it is not required yet.
5. Merge the pull request; the validation runs again on `master` and the documentation is published.

## Before pushing

- Format: `pre-commit run --all-files` (clang-format with the line limit of 110, cmake-format, black and
  basic checks); `pre-commit install` runs it at each commit.
- Tests: build with `-DBUILD_TESTING=ON` and run `ctest` (all), `ctest -L <component>` (one component:
  tools, core, analysis, raw, detector, track, geant4, connectors, restG4) or `make check` (build and run
  all). New features need unit tests in the `test/` directory of their component (see below).
- Documentation: Doxygen in the `.cxx` of each class (keys, results, example), the README of the
  component, the user guide (doc/UserGuide.md) for the formats and keys, and MIGRATION_GUIDE.md /
  MIGRATION_STATUS.md for what comes from legacy.

## Tests

Each component has a `test/` directory with a GoogleTest executable (`<component>Tests`, sources in
`test/src/*.cxx`) added with `rest_add_tests(<component> <libraries>)` (cmake/RestTests.cmake): each test
has the label of its component. The unit tests build their inputs (events, metadata, datasets) in memory or
in temporary files, without external data. Tests that need input data (pipelines of the examples) will use
data published outside the repository.

## Continuous integration

The jobs run in the Docker image of the `rest-docker` repository (ROOT, Geant4, Garfield++, CRY,
prometheus-cpp), `ghcr.io/<owner>/rest-env:latest`, with `<owner>` the owner of this repository; the
repository variable `RESTV2_IMAGE` (Settings, Secrets and variables, Actions, Variables) sets another
image. The package of the image must be readable by this repository (public, or Package settings, Manage
Actions access).

| Workflow | When | What |
|---|---|---|
| `validation.yml` | Pull requests to master, pushes to master, releases, by hand | Build (all the packages, install, `restRoot`, Doxygen), unit tests (a step per component), restG4 examples, format (pre-commit). |
| `docs.yml` | Pushes to master, by hand | Doxygen documentation in GitHub Pages (Settings, Pages, Source: GitHub Actions). |

Other files of `.github`: the pull request template, CODEOWNERS (reviewers requested automatically),
dependabot (weekly updates of the actions) and pr-badge.yml (badges of author, size and validation of each
pull request, with the [PR Badge](https://pullrequestbadge.com) GitHub App installed in the repository).

## Protection of master (administrators)

The protection is a ruleset of the repository, not a file: import `.github/rulesets/master.json` in
Settings, Rules, Rulesets, New ruleset, Import a ruleset. It applies to the default branch (master):

- no deletion and no force push;
- changes only through pull requests (no approvals required: a single developer cannot approve their own
  pull requests; raise `required_approving_review_count` when there are reviewers);
- the checks Build, Unit tests, restG4 examples and Format must pass, with the branch up to date.

With the GitHub CLI: `gh api repos/<owner>/<repository>/rulesets --method POST --input .github/rulesets/master.json`.
The checks can only be selected once they have run at least once (open a first pull request).

## Build internals

- **Libraries and registration.** Events, metadata and processes register themselves (EventRegistry,
  MetadataClassRegistry) from static initializers, so the executables, macros and tests link the REST
  libraries with `--no-as-needed` (`rest_link_registered_libraries` in CMakeLists.txt): with `--as-needed`
  (the default of Ubuntu and others) a library whose symbols are not used would be dropped and its classes
  would not exist in `restManager`. The build-tree path of the libraries is written as RPATH (searched before
  `LD_LIBRARY_PATH`), so the programs of the build load its libraries and not those of another REST.
- **ROOT dictionaries.** Each library generates its dictionary with a LinkDef written at build time
  (`<library>_LinkDef.h`, cmake/scripts/RestGenerateLinkDef.cmake) selecting the class or namespace named as
  each header. Without it rootcling scans every declaration (also std and ROOT templates) and crashed with
  ROOT 6.36. The libraries find their headers and sources with `file(GLOB)`: a new file needs re-running
  `cmake`.
- **Macros.** Each macro (`source/macros`, `libraries/*/macros`, `packages/restGarfield/macros`) is compiled
  into `macros/lib<macro>_C.so`, without dictionary, and CMake writes `macros/REST_Macros.h` with the
  declarations of its `REST_*` functions and global pointers (`_run`, `_ev`, `_dataSet`), taken from the
  definitions starting at column 0. `restRoot` loads the libraries and declares that header, so the macros
  are available at start-up and run compiled. The `.C` files are installed as reference but are not in the
  macro path (`.x`/`.L` would redefine the declared functions); after changing a macro, rebuild and install.

## Publishing a package in its own repository

`packages/restG4`, `packages/restGarfield` and `packages/restDAQ` are developed in this repository. To publish
one of them alone (e.g. with its own tags), `git subtree` extracts the commits that touch it, with its files
at the root, and pushes them to another repository; the work is always done here (commits in the other
repository would need `git subtree pull` and conflict resolution).

```bash
# First and next publications (incremental)
git subtree push --prefix packages/restG4 git@github.com:<owner>/restG4.git master
# A tag in the other repository: on the branch generated by split
git subtree split --prefix packages/restG4 -b restg4-split
git push git@github.com:<owner>/restG4.git restg4-split:refs/tags/v3.0.0
git branch -D restg4-split
```

The history of the split starts with the first commit of restv2 (the legacy history of restG4 is in its
original repository), so pushing to a repository with that legacy history is rejected unless forced.
