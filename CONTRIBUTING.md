# Contributing to openc2e-nb

This is the [NORNBRAIN](https://github.com/thechimpmatrix/nornbrain) fork of
openc2e. The fork carries engine repairs and a pluggable Python brain layer
that NORNBRAIN uses to host its CfC comb-replacement brain inside the engine
tick loop.

## Where contributions land

- **Engine repairs that benefit any openc2e user** (CAOS parser fixes, agent
  destruction safety, build cleanups for modern toolchains): welcome here, and
  also worth opening upstream against
  [openc2e/openc2e](https://github.com/openc2e/openc2e) so the wider community
  benefits.
- **NORNBRAIN-specific integration** (the `--brain-module` flag, the
  `PythonBrain.cpp` boundary, TCP CAOS injection): land here only.
- **Brain-side Python work** (the comb-replacement CfC module, the bridge
  wrapper, training loops, monitor): belongs in the
  [NORNBRAIN repo](https://github.com/thechimpmatrix/nornbrain), not here.

## Branch and pull request workflow

The `main` branch is protected. All contributions must come through a pull
request from a fork or feature branch. Direct pushes to `main` are blocked
for non-maintainer contributors.

1. Fork the repository and create a feature branch from `main`.
2. Build and test your changes locally before submitting.
3. Open a pull request against `main` describing what you changed and why.

## Original openc2e contributor guidance

If you have any questions about openc2e development or would like to help, come find us on [the #openc2e channel on the Caos Coding Cave Discord](https://discord.gg/rWFC3b3).

### Code licensing

If you make any contributions to openc2e after January 9th, 2025, you are agreeing that any code you have contributed will be licensed under the GNU LGPL version 2.1 (or any later version).

### Code style and formatting

In most cases, `clang-format` can and should be used to automatically reformat code and solve most formatting issues.

- To run clang-format on all staged files:
  ```sh
  ./script/lint.sh
  ```

- To run clang-format on all files changed since a certain revision, or that are different from another branch, pass the ref name as the first argument to `lint.sh`. For instance, to check all files different from the current main branch:
  ```sh
  ./script/lint.sh main
  ```
