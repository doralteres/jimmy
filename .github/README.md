# GitHub Actions CI/CD

This project uses GitHub Actions for continuous integration and deployment.

## Workflows

### PR Build (`pr-build.yml`)
**Trigger:** When a pull request is opened or updated targeting the `main` branch

**Purpose:** Validates that the code builds successfully on macOS

**Steps:**
1. Checkout code with submodules
2. Install CMake
3. Configure and build the VST3 plugin
4. Verify the build artifact exists

### Build and Release (`release.yml`)
**Trigger:** When changes are pushed to the `main` branch

**Purpose:** Builds the plugin, bumps the version, and creates a GitHub release

**Skip CI:** To skip the release workflow, start your commit message with `[skip-ci]`:
```bash
git commit -m "[skip-ci] Update documentation"
```

**Steps:**
1. Checkout code with submodules
2. Install CMake
3. Bump minor version in `CMakeLists.txt` (e.g., 0.1.0 → 0.2.0)
4. Commit version change back to the repository
5. Build the VST3 plugin
6. Package the VST3 bundle as a zip file
7. Create a GitHub release with the packaged VST3 as an artifact

**Release Artifacts:**
- `Jimmy-{version}-macOS.zip` - Contains the VST3 bundle ready for installation

## Version Numbering

The project follows semantic versioning (MAJOR.MINOR.PATCH):
- **MAJOR:** Manual updates for breaking changes
- **MINOR:** Auto-bumped on every merge to main
- **PATCH:** Reset to 0 on minor version bumps

## Requirements

- macOS runner (for building macOS VST3)
- `contents: write` permission for creating releases
- JUCE submodule properly initialized

## Manual Version Update

To manually set a version, edit the `VERSION` in `CMakeLists.txt`:

```cmake
project(Jimmy VERSION 1.0.0 LANGUAGES C CXX)
```

The next push to main will then bump from that version (e.g., 1.0.0 → 1.1.0).
