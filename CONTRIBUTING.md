# Contributing to EdgeFirst VideoStream Library

Thank you for your interest in contributing to VideoStream! This project is part of the EdgeFirst Perception stack, advancing edge AI and computer vision capabilities on embedded platforms.

We welcome contributions from the community - whether it's fixing bugs, improving documentation, adding features, or helping others in discussions.

---

## Table of Contents

1. [Code of Conduct](#code-of-conduct)
2. [Ways to Contribute](#ways-to-contribute)
3. [Before You Start](#before-you-start)
4. [Development Setup](#development-setup)
5. [Building](#building)
6. [Testing](#testing)
7. [Contribution Process](#contribution-process)
8. [Code Style](#code-style)
9. [Commit Messages](#commit-messages)
10. [Pull Request Guidelines](#pull-request-guidelines)
11. [License Agreement](#license-agreement)
12. [Release Process (Maintainers)](#release-process-maintainers)

---

## Code of Conduct

Please read and follow our [Code of Conduct](CODE_OF_CONDUCT.md) before contributing. We're committed to fostering a welcoming, inclusive, and respectful community.

---

## Ways to Contribute

### Code Contributions
- **Bug Fixes** - Fix issues reported in the issue tracker
- **New Features** - Implement requested features or propose new ones
- **Performance Improvements** - Optimize critical paths for embedded platforms
- **Platform Support** - Add support for new hardware platforms and accelerators

### Documentation Contributions
- **Guides and Tutorials** - Write usage examples and how-to guides
- **API Documentation** - Improve inline code documentation
- **Architecture Documentation** - Clarify internal design decisions
- **Translation** - Help translate documentation to other languages

### Testing Contributions
- **Bug Reports** - Report bugs with detailed reproduction steps
- **Hardware Validation** - Test on different platforms (i.MX8, Raspberry Pi, etc.)
- **Test Coverage** - Write unit and integration tests
- **Performance Benchmarks** - Measure and report performance on various platforms

### Community Contributions
- **Answer Questions** - Help others in GitHub Discussions
- **Code Reviews** - Review pull requests from other contributors
- **Blog Posts and Talks** - Share your VideoStream use cases and experiences
- **Integration Examples** - Show how you integrated VideoStream with other tools

---

## Before You Start

1. **Check existing issues and pull requests**
   - Browse [open issues](https://github.com/EdgeFirstAI/videostream/issues) to see if your idea is already being discussed
   - Check [pull requests](https://github.com/EdgeFirstAI/videostream/pulls) to avoid duplicate work

2. **Discuss significant changes first**
   - For major features or breaking changes, open an issue for discussion before coding
   - This helps ensure your contribution aligns with project direction

3. **Review the architecture**
   - Read [DESIGN.md](DESIGN.md) for high-level architecture
   - Read [ARCHITECTURE.md](ARCHITECTURE.md) for implementation details
   - Understand the EdgeFirst Perception ecosystem: https://doc.edgefirst.ai/test/perception/

4. **Consider EdgeFirst Studio integration**
   - If adding features, think about how they might integrate with EdgeFirst Studio
   - Ensure changes don't break existing Studio workflows

---

## Development Setup

### Prerequisites

**Required:**
- **CMake** 3.10 or later
- **C Compiler** with C11 support (GCC 7+, Clang 6+, or MSVC 2019+)
- **GStreamer** 1.4 or later (with development headers)
  - `gstreamer-1.0`
  - `gstreamer-video-1.0`
  - `gstreamer-app-1.0`
  - `gstreamer-allocators-1.0`
  - `glib-2.0`
- **Python** 3.8 or later (for running tests)
- **Git** for version control

**Recommended:**
- **clang-format** for code formatting
- **valgrind** for memory leak detection
- **GDB** or **LLDB** for debugging

**Optional (for cross-compilation):**
- **NXP Yocto SDK** for i.MX8 target builds
- **Docker** for containerized builds

### Installing Prerequisites

#### Ubuntu / Debian
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    pkg-config \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libglib2.0-dev \
    python3 \
    python3-pip \
    python3-pytest \
    clang-format \
    valgrind \
    gdb
```

#### Fedora / RHEL / CentOS
```bash
sudo dnf install -y \
    gcc gcc-c++ make cmake \
    pkgconf \
    gstreamer1-devel \
    gstreamer1-plugins-base-devel \
    glib2-devel \
    python3 \
    python3-pip \
    python3-pytest \
    clang-tools-extra \
    valgrind \
    gdb
```

#### macOS
```bash
brew install cmake gstreamer glib pkg-config python3
```

#### Windows
- Install [Visual Studio 2019+](https://visualstudio.microsoft.com/) with C++ workload
- Install [GStreamer](https://gstreamer.freedesktop.org/download/) for Windows (MSVC build)
- Install [CMake](https://cmake.org/download/)
- Install [Python 3](https://www.python.org/downloads/)

#### Clone the Repository

```bash
git clone https://github.com/EdgeFirstAI/videostream.git
cd videostream
```

---

## Building

### Standard Build (Debug)

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
```

### Release Build

```bash
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

### Build Options

VideoStream supports several CMake options:

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_SHARED_LIBS` | ON | Build shared libraries (.so) instead of static |
| `ENABLE_GSTREAMER` | ON | Build GStreamer plugin (vslsink, vslsrc) |
| `ENABLE_DMABUF` | ON (Linux) | Enable Linux DmaBuf zero-copy support |
| `ENABLE_G2D` | ON (Linux) | Enable G2D hardware acceleration (NXP) |
| `ENABLE_VPU` | ON (Linux) | Enable VPU encoding/decoding (Hantro) |
| `ENABLE_OPENMP` | ON | Enable OpenMP threading |
| `ENABLE_COVER` | OFF | Enable code coverage instrumentation |
| `BUILD_TESTING` | OFF | Build test suite |

**Example:**
```bash
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DENABLE_COVER=ON \
      -DBUILD_TESTING=ON \
      ..
```

### Cross-Compiling for ARM/ARM64

#### Using NXP Yocto SDK

```bash
# Source the SDK environment
source /opt/fsl-imx-xwayland/5.4-zeus/environment-setup-aarch64-poky-linux

mkdir build-arm64
cd build-arm64
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)
```

#### Using Docker

```bash
# Pull the Au-Zone DeepView Yocto SDK image
docker pull deepview/yocto-sdk-imx8mp:latest

# Configure
docker run -v $PWD:/src deepview/yocto-sdk-imx8mp \
    cmake -S/src -B/src/build -DCMAKE_BUILD_TYPE=Release

# Build
docker run -v $PWD:/src deepview/yocto-sdk-imx8mp \
    cmake --build /src/build -j16
```

### Build Artifacts

After building, you'll find:
- `build/libvideostream.so` - Core library
- `build/gst/libgstvideostream.so` - GStreamer plugin
- `build/src/vsl-camhost` - Camera host example
- `build/src/vsl-monitor` - Frame monitoring example
- `build/src/vsl-framelock` - Frame locking demo

---

## Testing

### Running Python Tests

VideoStream includes Python-based integration tests:

```bash
# Install test dependencies
pip3 install pytest

# Run all tests
cd build
pytest ../tests/

# Run specific test file
pytest ../tests/test_frame.py

# Run with verbose output
pytest -v ../tests/

# Run with coverage
pytest --cov=videostream ../tests/
```

### Running C Unit Tests (Future)

C unit tests are planned - see [TODO.md](TODO.md) for status.

```bash
# Once implemented:
cd build
cmake -DBUILD_TESTING=ON ..
make
ctest --output-on-failure
```

### Manual Testing

#### Test GStreamer Plugin

**Terminal 1 (Host):**
```bash
# Make sure the plugin is in GST_PLUGIN_PATH
export GST_PLUGIN_PATH=$PWD/build/gst:$GST_PLUGIN_PATH

# Verify plugin is detected
gst-inspect-1.0 vslsink
gst-inspect-1.0 vslsrc

# Start a test pattern host
gst-launch-1.0 videotestsrc ! \
    video/x-raw,width=640,height=480,framerate=30/1 ! \
    vslsink path=/tmp/test-vsl
```

**Terminal 2 (Client):**
```bash
export GST_PLUGIN_PATH=$PWD/build/gst:$GST_PLUGIN_PATH

# Display the test pattern
gst-launch-1.0 vslsrc path=/tmp/test-vsl ! autovideosink
```

#### Test C API

```bash
# Run the monitor example
build/src/vsl-monitor --help

# Save frames as JPEG (requires running host)
build/src/vsl-monitor --jpeg --rate 10 /tmp/test-vsl
```

### Memory Leak Testing

```bash
# Build with debug symbols
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Run with valgrind
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         ./build/src/vsl-monitor /tmp/test-vsl
```

---

### 1. Fork and Clone

#### 1. Fork the Repository

1. Go to https://github.com/EdgeFirstAI/videostream
2. Click the "Fork" button in the top-right corner
3. This creates a copy under your GitHub account

## Code Style

### C Code Style

VideoStream follows **C11 standard** with the following conventions:

#### Formatting
# Add upstream remote
git remote add upstream https://github.com/EdgeFirstAI/videostream.git
git fetch upstream
We use **clang-format** with the included [.clang-format](.clang-format) configuration.

**Format your code before committing:**
```bash
# Format all C/C++ files
find src lib gst include -name "*.[ch]" -exec clang-format -i {} \;

# Or format specific files
clang-format -i lib/client.c
```

**Key style rules:**
- **Indentation**: 4 spaces (no tabs)
- **Line length**: 100 characters maximum
- **Braces**: K&R style (opening brace on same line, except for functions)
- **Function names**: `snake_case` (e.g., `vstream_client_open`)
- **Type names**: `CamelCase` with prefix (e.g., `VStreamClient`)
- **Constants**: `UPPER_SNAKE_CASE` (e.g., `VSTREAM_MAX_CLIENTS`)
- **Macros**: `UPPER_SNAKE_CASE` with `VSL_` prefix

**Example:**
```c
// Good
int vstream_client_open(const char *path, VStreamClient **client)
{
    if (path == NULL || client == NULL) {
        return -EINVAL;
    }

    VStreamClient *new_client = calloc(1, sizeof(VStreamClient));
    if (new_client == NULL) {
        return -ENOMEM;
    }

    // ... implementation ...

    *client = new_client;
    return 0;
}

// Bad: Poor naming, inconsistent bracing, no error checking
int OpenClient(char* p, void** c) {
  void* x = malloc(sizeof(VStreamClient));
  *c = x;
  return 0;
}
```

#### Naming Conventions

- **Public API functions**: `vstream_*` prefix
- **Internal functions**: `vsl_*` prefix or static functions
- **Struct types**: `VStream*` prefix (e.g., `VStreamFrame`)
- **Enum types**: `VStream*` prefix with `_t` suffix (e.g., `VStreamFormat_t`)

#### Documentation

Use **Doxygen-style** comments for public APIs:

```c
/**
 * @brief Open a connection to a VideoStream frame pool as a client
 *
 * Opens a connection to an existing frame pool identified by the given path.
 * The path must match the path used by the host when creating the pool.
 *
 * @param[in]  path    Path to the VideoStream IPC socket
 * @param[out] client  Pointer to receive the allocated client handle
 *
 * @return 0 on success, negative error code on failure
 * @retval 0       Success
 * @retval -EINVAL Invalid arguments (NULL pointers)
 * @retval -ENOMEM Out of memory
 * @retval -ENOENT Pool not found at the specified path
 *
 * @note The caller is responsible for closing the client with vstream_client_close()
 * @see vstream_client_close()
 */
int vstream_client_open(const char *path, VStreamClient **client);
```

#### Error Handling

- Use **negative errno values** for errors (e.g., `-EINVAL`, `-ENOMEM`)
- Return `0` for success
- Always validate input parameters
- Clean up resources on error paths

```c
int vstream_frame_lock(VStreamClient *client, VStreamFrame *frame)
{
    if (client == NULL || frame == NULL) {
        return -EINVAL;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        return -errno;
    }

    int ret = send_lock_request(fd, frame->serial);
    if (ret < 0) {
        close(fd);  // Clean up on error
        return ret;
    }

    return 0;
}
```

### CMake Code Style

- Use **4-space indentation**
- Use **lowercase** for command names (e.g., `add_library`, not `ADD_LIBRARY`)
- Group related commands together
- Comment non-obvious configurations

---

## Commit Messages

### Format

```
Short summary (50 characters or less)

More detailed explanation if needed. Wrap at 72 characters.
Explain the problem this commit solves and why this approach
was chosen.

- Bullet points are okay for listing changes
- Use present tense ("Add feature" not "Added feature")
- Reference issues: Fixes #123
```

### Examples

**Good commit messages:**
```
Add DmaBuf support for NVIDIA Jetson platforms

Extends the existing DmaBuf implementation to support NVIDIA Tegra
memory allocators. This enables zero-copy video pipelines on Jetson
devices (Nano, Xavier, Orin).

- Detect Tegra NvBufSurface allocator
- Map Tegra buffers to standard DmaBuf FDs
- Add Jetson-specific memory alignment
- Update documentation with Jetson examples

Fixes #45
```

```
Fix memory leak in client disconnect path

The client cleanup code was not freeing the socket buffer allocated
during connection. This caused a small memory leak on each client
disconnect.

- Free socket buffer in vstream_client_close()
- Add valgrind test to catch future leaks
- Update tests to verify cleanup

Fixes #67
```

**Bad commit messages:**
```
fix bug          # Too vague, no context
updated stuff    # Meaningless description
WIP              # Never commit WIP to main branches
fixed #45        # Should explain what was done
```

---

## Pull Request Guidelines

### PR Title

Use the same format as commit messages:
- **Feature**: "Add RTSP support for remote video sources"
- **Bugfix**: "Fix memory leak in client disconnect"
- **Docs**: "Improve API documentation with usage examples"

### PR Description Template

```markdown
## Summary
Brief description of what this PR accomplishes and why.

## Changes
- Bullet list of significant changes
- Keep it concise but informative

## Testing
- [ ] Unit tests added/updated
- [ ] Integration tests pass
- [ ] Manual testing completed on [platform/hardware]
- [ ] No regressions in existing functionality

## Documentation
- [ ] API documentation updated
- [ ] User guide updated (if user-facing)
- [ ] CHANGELOG.md updated
- [ ] Examples added/updated (if applicable)

## Checklist
- [ ] Code follows project style guidelines
- [ ] Builds without warnings
- [ ] All tests pass
- [ ] No memory leaks (verified with valgrind)
- [ ] Commit messages are clear and descriptive
- [ ] PR is against correct branch (main or develop)

## Related Issues
Fixes #123
Related to #456

## Additional Notes
Any other context, screenshots, or information reviewers should know.
```

### PR Review Process

1. **Automated Checks**: CI must pass (build, tests, linting)
2. **Code Review**: At least one maintainer approval required
3. **Testing**: Verify functionality on target platforms
4. **Documentation**: Ensure docs are updated
5. **Merge**: Squash or rebase to keep history clean

**Timeline:**
- Initial review: Within 3 business days
- Follow-up on feedback: Be responsive to comments
- Merge after approval: Usually within 1-2 days

---

## License Agreement

By contributing to VideoStream, you agree that your contributions will be licensed under the **Apache License 2.0**, the same license as the project.

You represent that:
- You have the right to contribute the code
- Your contribution is your original work
- You understand the contribution will be publicly available

**No Contributor License Agreement (CLA) is required** - your PR submission is considered acceptance of these terms.

---

## Development Tips

### Debugging

**Enable debug logging:**
```bash
# Set environment variable for verbose logging
export VSL_DEBUG=1
export GST_DEBUG=3  # For GStreamer debugging
```

**Use GDB:**
```bash
gdb --args ./build/src/vsl-monitor /tmp/test-vsl
(gdb) break vstream_client_open
(gdb) run
```

**Use valgrind for memory issues:**
```bash
valgrind --leak-check=full --track-origins=yes \
         ./build/src/vsl-monitor /tmp/test-vsl
```

### Performance Profiling

**Use `perf` on Linux:**
```bash
# Record performance data
perf record -g ./build/src/vsl-monitor /tmp/test-vsl

# View results
perf report
```

**Use `tracy` profiler (if integrated):**
```bash
cmake -DENABLE_TRACY=ON ..
make
# Run with Tracy server connected
```

### Common Issues

**GStreamer plugin not found:**
```bash
export GST_PLUGIN_PATH=$PWD/build/gst:$GST_PLUGIN_PATH
gst-inspect-1.0 vslsink  # Should list the plugin
```

**DmaBuf not available:**
- Ensure kernel has CONFIG_DMA_SHARED_BUFFER=y
- Check `/dev/dma_heap` exists (Linux 5.6+)
- Verify permissions: `ls -l /dev/dma_heap`

**Cross-compilation issues:**
- Ensure Yocto SDK environment is sourced
- Check CMake toolchain file paths
- Verify pkg-config finds correct GStreamer libs

---

## Getting Help

**For development questions:**
- Check existing [documentation](https://doc.edgefirst.ai/test/perception/videostream/)
- Search [GitHub Issues](https://github.com/EdgeFirstAI/videostream/issues)
- Ask in [GitHub Discussions](https://github.com/EdgeFirstAI/videostream/discussions)
- Review [ARCHITECTURE.md](ARCHITECTURE.md) for internal details

**For contribution process questions:**
- Read this CONTRIBUTING.md thoroughly
- Check recent merged PRs for examples
- Ask in GitHub Discussions under "Contributing" category

**For bugs or security issues:**
- **Bugs**: [Open an issue](https://github.com/EdgeFirstAI/videostream/issues/new)
- **Security**: Email support@au-zone.com with subject "Security Vulnerability"

---

## Recognition

Contributors are recognized in several ways:
- Listed in [CONTRIBUTORS.md](CONTRIBUTORS.md) (coming soon)
- Mentioned in release notes for significant contributions
- Acknowledged in commit messages and PR comments

---

**Thank you for contributing to VideoStream and the EdgeFirst Perception ecosystem!**

Your contributions help make edge AI more accessible and efficient for developers worldwide.

---

## Release Process (Maintainers)

This section is for VideoStream maintainers who perform releases.

### Release Overview

VideoStream uses a **multi-language release process** that coordinates version numbers across:
- **C Library** (`include/videostream.h` - `VSL_VERSION`)
- **Rust Crates** (`Cargo.toml` workspace and member crates)
- **Python Package** (`pyproject.toml`)
- **Debian Package** (`debian/changelog`)
- **Documentation** (`CHANGELOG.md`)

We use **`cargo-release`** to automate this coordination through its `pre-release-replacements` feature.

### Prerequisites

1. **Install cargo-release** (0.25.20 or later):
   ```bash
   cargo install cargo-release
   ```

2. **Verify clean working tree**:
   ```bash
   git status  # Should show no uncommitted changes
   ```

3. **Ensure you're on the correct branch**:
   ```bash
   git checkout main  # Or develop, depending on workflow
   git pull origin main
   ```

4. **Review CHANGELOG.md**:
   - Move unreleased changes to new version section
   - Add release date
   - Verify formatting follows Keep a Changelog

### Release Process Steps

#### Step 1: Dry Run (REQUIRED)

**ALWAYS** run a dry-run first to preview changes:

```bash
cargo release <level>
```

Where `<level>` is one of: `major`, `minor`, `patch`, `release`, `rc`, `beta`, `alpha`

Example:
```bash
# Bump from 1.4.0 to 1.5.0
cargo release minor

# Bump from 1.5.0-rc0 to 1.5.0 (remove pre-release)
cargo release release
```

This will show you:
- What version will be set
- What files will be modified
- What git operations will occur
- **BUT WILL NOT MAKE ANY CHANGES**

#### Step 2: Execute Release

Once you've reviewed the dry-run and are satisfied:

```bash
cargo release <level> --execute
```

**CRITICAL**: This command performs **ALL** of the following steps automatically:

1. **`cargo release version`** - Bumps version in `Cargo.toml` workspace and crates
2. **`cargo release replace`** - Runs pre-release-replacements:
   - Updates `include/videostream.h` with new `VSL_VERSION`
   - Updates `pyproject.toml` with new `version`
   - Updates `debian/changelog` with new entry
   - Updates `CHANGELOG.md` with release date
3. **`cargo release hook`** - Runs pre-release hooks (if configured)
4. **`cargo release commit`** - Creates git commit with version bump
5. **`cargo release tag`** - Creates git tag (e.g., `v1.5.0`)
6. **`cargo release push`** - Pushes commit and tag to remote

#### Step 3: Verify Release

After `cargo release --execute` completes:

```bash
# Verify tag was created
git tag -l | grep v1.5.0

# Verify all files were updated
git show HEAD

# Verify pyproject.toml
grep "^version" pyproject.toml

# Verify include/videostream.h
grep "VSL_VERSION" include/videostream.h

# Verify debian/changelog
head -n 5 debian/changelog
```

#### Step 4: Push to Remote

If you used `--no-push` option:

```bash
git push origin main
git push origin v1.5.0
```

#### Step 5: Monitor CI/CD

Watch GitHub Actions workflows:
- **Build Workflow**: Verifies build across platforms
- **SBOM Workflow**: Generates Software Bill of Materials
- **Release Workflow**: Creates GitHub Release and publishes artifacts

### Understanding cargo-release

#### The Problem

VideoStream has a **single source of truth** version that must be synchronized across **5 different files** in **3 different languages**:

1. `Cargo.toml` (Rust workspace)
2. `crates/*/Cargo.toml` (Rust crate manifests)
3. `include/videostream.h` (C header: `#define VSL_VERSION "1.5.0"`)
4. `pyproject.toml` (Python: `version = "1.5.0"`)
5. `debian/changelog` (Debian: `videostream (1.5.0-1) unstable; urgency=medium`)

Manually updating these is error-prone and easy to forget.

#### The Solution

`cargo-release` provides **pre-release-replacements** - a powerful regex-based find-and-replace system that runs automatically during the release process.

In `release.toml`, we define:

```toml
[[pre-release-replacements]]
file = "include/videostream.h"
search = '#define VSL_VERSION "[^"]*"'
replace = '#define VSL_VERSION "{{version}}"'

[[pre-release-replacements]]
file = "pyproject.toml"
search = 'version = "[^"]*"'
replace = 'version = "{{version}}"'

# ... etc for other files
```

When you run `cargo release <level> --execute`:
1. Cargo-release determines the new version (e.g., `1.5.0`)
2. Substitutes `{{version}}` placeholder with `1.5.0`
3. Uses regex `search` pattern to find exact location in file
4. Replaces with `replace` string
5. Repeats for all configured files

#### Individual Steps

You can run steps individually for debugging:

```bash
# Step 1: Just bump Cargo.toml versions (no other files)
cargo release version minor --execute

# Step 2: Run ONLY the pre-release-replacements
cargo release replace --execute

# Step 3: Commit the changes
cargo release commit --execute

# Step 4: Create the git tag
cargo release tag --execute

# Step 5: Push to remote
cargo release push --execute
```

This is useful if:
- You need to manually review changes between steps
- Something failed mid-process
- You want to add additional changes to the release commit

#### Common Options

```bash
# Dry-run (default - shows what would happen)
cargo release minor

# Execute for real
cargo release minor --execute

# Don't create git tag
cargo release minor --execute --no-tag

# Don't push to remote (push manually later)
cargo release minor --execute --no-push

# Don't publish to crates.io (we don't publish)
cargo release minor --execute --no-publish

# Skip confirmation prompt
cargo release minor --execute --no-confirm
```

### Troubleshooting

#### Problem: pre-release-replacements didn't run

**Symptoms**: After `cargo release --execute`, `pyproject.toml` or `include/videostream.h` still show old version.

**Cause**: You likely ran `cargo release version --execute` instead of full `cargo release <level> --execute`.

**Solution**:
```bash
# Option 1: Run the replace step manually
cargo release replace --execute

# Option 2: Start over (if no tag created yet)
git reset --hard HEAD~1  # Undo the version bump commit
cargo release <level> --execute  # Run full process
```

#### Problem: Tag already exists

**Symptoms**: `cargo release` fails with "tag v1.5.0 already exists"

**Cause**: You previously ran the release and created the tag, but files weren't updated.

**Solution**:
```bash
# Delete the tag locally
git tag -d v1.5.0

# Delete the tag remotely (if pushed)
git push origin :refs/tags/v1.5.0

# Fix the files manually or run replace step
cargo release replace --execute

# Recreate tag on current commit
git tag v1.5.0
git push origin v1.5.0
```

#### Problem: Version mismatch in CI

**Symptoms**: GitHub Actions fails with "Tag version (1.5.0) does not match pyproject.toml version (1.4.0-rc0)"

**Cause**: The `cargo release replace` step didn't run or failed silently.

**Solution**:
```bash
# Verify current versions in all files
grep "^version" pyproject.toml
grep "VSL_VERSION" include/videostream.h
grep "^version" Cargo.toml

# If they don't match the tag, manually update or re-run replace
cargo release replace --execute

# Amend the release commit
git add .
git commit --amend --no-edit

# Force-push the tag
git tag -f v1.5.0
git push origin main --force-with-lease
git push origin v1.5.0 --force
```

#### Problem: pre-release-replacements regex doesn't match

**Symptoms**: `cargo release replace` says "0 replacements made" for a file

**Cause**: The `search` regex in `release.toml` doesn't match the actual file content.

**Solution**:
```bash
# Test regex manually
grep -E 'version = "[^"]*"' pyproject.toml
grep -E '#define VSL_VERSION "[^"]*"' include/videostream.h

# If no match, update the search pattern in release.toml
# Then re-run
cargo release replace --execute
```

### Emergency Fixes

#### Fix Version Mismatch After Tag Created

If you've already pushed a tag but files are wrong:

```bash
# 1. Fix the files manually
vim include/videostream.h  # Update VSL_VERSION
vim pyproject.toml         # Update version

# 2. Commit the fix
git add include/videostream.h pyproject.toml
git commit -m "fix: Update version to match v1.5.0 tag"

# 3. Move the tag to new commit
git tag -f v1.5.0

# 4. Force-push (CAUTION: coordinate with team)
git push origin main --force-with-lease
git push origin v1.5.0 --force
```

#### Rollback a Release

If you need to undo a release completely:

```bash
# 1. Delete remote tag
git push origin :refs/tags/v1.5.0

# 2. Delete local tag
git tag -d v1.5.0

# 3. Reset to before release commit
git reset --hard HEAD~1

# 4. Force-push (CAUTION: coordinate with team)
git push origin main --force-with-lease
```

### Release Checklist

- [ ] Update CHANGELOG.md with release notes
- [ ] Run `cargo release <level>` dry-run
- [ ] Review dry-run output carefully
- [ ] Run `cargo release <level> --execute`
- [ ] Verify all 5 files updated (`Cargo.toml`, `pyproject.toml`, `include/videostream.h`, `debian/changelog`, `CHANGELOG.md`)
- [ ] Verify git tag created
- [ ] Push to remote (or verify `cargo-release` pushed)
- [ ] Monitor CI/CD workflows
- [ ] Verify GitHub Release created
- [ ] Announce release (if public)

### References

- **cargo-release Documentation**: https://github.com/crate-ci/cargo-release/blob/master/docs/reference.md
- **cargo-release FAQ**: https://github.com/crate-ci/cargo-release/blob/master/docs/faq.md
- **Working Example**: [EdgeFirst Client release.toml](https://github.com/EdgeFirstAI/client/blob/main/release.toml)
- **VideoStream CHANGELOG.md**: Follow Keep a Changelog format
- **VideoStream release.toml**: See pre-release-replacements configuration

---

*For questions or clarifications about contributing, please email [support@au-zone.com](mailto:support@au-zone.com) or ask in [GitHub Discussions](https://github.com/EdgeFirstAI/videostream/discussions).*
