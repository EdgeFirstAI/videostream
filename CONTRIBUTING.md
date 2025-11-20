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

VideoStream uses a **manual release process** that ensures version consistency across multiple languages and package formats. The process requires careful attention to detail as versions must be synchronized across:

- **C Library** (`include/videostream.h` - `VSL_VERSION`)
- **Rust Crates** (`Cargo.toml` workspace and member crates)
- **Python Package** (`pyproject.toml`)
- **CMake** (`CMakeLists.txt` - parsed from videostream.h)
- **Debian Package** (`debian/changelog`)
- **Documentation** (`doc/conf.py`)
- **Release Notes** (`CHANGELOG.md`)

### Prerequisites

1. **Verify clean working tree**:
   ```bash
   git status  # Should show no uncommitted changes
   ```

2. **Ensure you're on the correct branch**:
   ```bash
   git checkout main  # Or develop, depending on workflow
   git pull origin main
   ```

3. **Ensure all pre-commit checks pass**:
   ```bash
   # Use the unified pre-release target (recommended)
   make pre-release
   
   # Or run checks individually:
   
   # Format code
   find src lib gst include -name "*.[ch]" -exec clang-format -i {} \;
   
   # Build (ALWAYS use modern CMake workflow - stay in project root)
   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
   cmake --build build -j$(nproc)
   
   # Run tests (requires venv activation and library path)
   source venv/bin/activate
   export VIDEOSTREAM_LIBRARY=./build/libvideostream.so.1
   pytest tests/
   # Or use: make test
   
   # Generate SBOM and verify license compliance
   make sbom
   
   # Verify all version files are synchronized
   make verify-version
   ```

### Common Pitfalls and Solutions

#### Pitfall 1: Forgot to Activate venv Before Tests
**Symptom**: `pytest tests/` fails with "Unable to load VideoStream library"

**Solution**:
```bash
source venv/bin/activate
export VIDEOSTREAM_LIBRARY=./build/libvideostream.so.1
pytest tests/
# Or simply: make test
```

#### Pitfall 2: Using Old CMake Workflow (Directory Confusion)
**Wrong**:
```bash
cd build
cmake ..
make
cd ..  # Easy to forget!
```

**Correct** (ALWAYS use this):
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
# You stay in project root - no directory confusion
```

**Why it matters**:
- Prevents getting lost in filesystem
- Works with all generators (Make, Ninja, Visual Studio)
- Required by modern IDEs and automation
- Cross-platform compatible

#### Pitfall 3: Missing debian/changelog Update
**Symptom**: `make verify-version` fails, CI/CD fails

**Solution**: debian/changelog requires NEW entry at TOP with specific format:
```
videostream (X.Y.Z-1) stable; urgency=medium

  * Brief description of changes

 -- Your Name <email@domain.com>  Day, DD Mon YYYY HH:MM:SS +0000

[existing entries below...]
```

#### Pitfall 4: Version Files Out of Sync
**Symptom**: CI/CD fails with version mismatch

**Solution**: All 6 files MUST have identical version:
1. `Cargo.toml` - line ~6: `version = "X.Y.Z"`
2. `include/videostream.h` - line ~16: `#define VSL_VERSION "X.Y.Z"`
3. `pyproject.toml` - line ~7: `version = "X.Y.Z"`
4. `doc/conf.py` - line ~28: `version = 'X.Y.Z'` (single quotes!)
5. `debian/changelog` - line 1: `videostream (X.Y.Z-1) stable;`
6. `CHANGELOG.md` - `## [X.Y.Z] - YYYY-MM-DD`

Verify with: `make verify-version`

#### Pitfall 5: SBOM License Violations Not Caught Locally
**Symptom**: PR fails CI/CD with license policy violations

**Solution**: ALWAYS run `make sbom` before committing release:
```bash
make sbom
# Reviews license compliance and catches GPL/AGPL violations
```

#### Pitfall 6: Wrong Git Tag Format
**Symptom**: Release workflow doesn't trigger

**CRITICAL**: Tags MUST use `vX.Y.Z` format to trigger release workflow:
```bash
# Correct (v prefix REQUIRED)
git tag -a -m "Version 1.5.2" v1.5.2
git push origin v1.5.2

# Wrong (missing v - release.yml will NOT trigger!)
git tag -a -m "Version 1.5.2" 1.5.2
```

The release.yml workflow is triggered by `tags: - 'v*'` pattern.

### Release Process Steps

#### Step 1: Determine Next Version

Follow **Semantic Versioning** (MAJOR.MINOR.PATCH):

- **MAJOR** (e.g., 1.x.x → 2.0.0): Breaking changes (API incompatibility)
  - Only on explicit user request
  - Requires migration guide in CHANGELOG or separate document
- **MINOR** (e.g., 1.4.x → 1.5.0): Breaking changes in embedded context
  - New features that change behavior
  - ABI/API changes requiring client recompilation
  - Performance characteristics changes
  - Include migration details in CHANGELOG
- **PATCH** (e.g., 1.5.0 → 1.5.1): Non-breaking changes
  - Bug fixes
  - Documentation updates
  - Performance improvements (no behavior change)
  - Security patches (non-breaking)

**Example decision tree**:
- Changed function signature? → MINOR (or MAJOR if requested)
- Added new optional API? → MINOR
- Fixed memory leak? → PATCH
- Updated documentation? → PATCH

#### Step 2: Update CHANGELOG.md

Follow [Keep a Changelog](https://keepachangelog.com/) format:

```bash
vim CHANGELOG.md
```

**Required changes**:
1. Move all items from `## [Unreleased]` section to new version section
2. Add version number and release date: `## [1.5.0] - 2025-11-20`
3. Ensure changes are categorized:
   - `### Added` - New features
   - `### Changed` - Changes in existing functionality
   - `### Deprecated` - Soon-to-be removed features
   - `### Removed` - Removed features
   - `### Fixed` - Bug fixes
   - `### Security` - Security fixes
4. For MINOR releases with breaking changes, add migration guide
5. Keep `## [Unreleased]` section at top for future changes

**Example**:
```markdown
## [Unreleased]

## [1.5.0] - 2025-11-20

### Added
- DmaBuf heap allocator support for Linux 5.6+
- Rust bindings with dynamic library loading

### Changed
- **BREAKING**: Client API now requires explicit timeout parameter
  - Migration: Replace `vsl_client_lock(client, &frame)` with `vsl_client_lock(client, &frame, 1000)`
  - Timeout is in milliseconds, use `-1` for infinite wait

### Fixed
- Memory leak in client disconnect path
- Race condition in frame recycling
```

#### Step 3: Update All Version Files

**CRITICAL**: All files must be updated to the **exact same version**. Create a checklist as you go.

**Files requiring version updates**:

1. **`Cargo.toml`** (Rust workspace):
   ```toml
   [workspace.package]
   version = "1.5.0"  # Update this line
   ```

2. **`include/videostream.h`** (C header - single source of truth for CMake):
   ```c
   #define VSL_VERSION "1.5.0"  // Update this line
   ```
   Note: CMake parses this file to extract PROJECT_VERSION automatically

3. **`pyproject.toml`** (Python package):
   ```toml
   [project]
   version = "1.5.0"  # Update this line
   ```

4. **`doc/conf.py`** (Sphinx documentation):
   ```python
   version = '1.5.0'  # Update this line (keep quotes)
   release = version  # This line stays unchanged
   ```

5. **`debian/changelog`** (Debian package):
   ```bash
   # Add NEW entry at the TOP of the file
   videostream (1.5.0-1) stable; urgency=medium

     * Release version 1.5.0
     * See CHANGELOG.md for detailed changes

    -- Au-Zone Technologies <support@au-zone.com>  Wed, 20 Nov 2025 14:30:00 -0400
   ```
   **Important**: Use `dch` tool or manually ensure proper Debian changelog format

**Update checklist**:
```bash
# Verify all files updated with same version
grep "version = \"1.5.0\"" Cargo.toml
grep "VSL_VERSION \"1.5.0\"" include/videostream.h  
grep "version = \"1.5.0\"" pyproject.toml
grep "version = '1.5.0'" doc/conf.py
grep "videostream (1.5.0-1)" debian/changelog
```

#### Step 4: Create Release Commit

**Commit message format**: `Prepare Version X.Y.Z`

```bash
# Stage all version changes
git add Cargo.toml \
        include/videostream.h \
        pyproject.toml \
        doc/conf.py \
        debian/changelog \
        CHANGELOG.md

# Create commit with structured message
git commit -m "Prepare Version 1.5.0

- Updated all version files to 1.5.0
- Finalized CHANGELOG.md with release notes
- See CHANGELOG.md for detailed changes"

# Push to remote
git push origin main
```

**Optional**: Add brief release highlights as bullet points if they add value beyond CHANGELOG

#### Step 5: Wait for CI/CD Success

**CRITICAL**: Do NOT tag until all checks pass.

```bash
# Monitor GitHub Actions
# URL: https://github.com/EdgeFirstAI/videostream/actions

# Watch for:
# ✅ Build Workflow - All platforms compile successfully
# ✅ Test Workflow - All tests pass
# ✅ SBOM Workflow - License compliance verified
# ✅ Code Quality - SonarQube checks pass (if configured)
```

If any checks fail:
1. Fix the issues
2. Create new commits to address failures
3. Push fixes
4. Wait for CI/CD to pass again
5. **DO NOT PROCEED TO TAGGING UNTIL ALL GREEN**

#### Step 6: Create and Push Git Tag

**Tag format**: `vX.Y.Z` (with `v` prefix)

```bash
# Create annotated tag
git tag -a -m "Version 1.5.0" 1.5.0

# Alternative: Multi-line tag message with highlights
git tag -a 1.5.0 -m "Version 1.5.0

Key improvements:
- DmaBuf heap allocator support
- Rust bindings with dynamic loading
- Performance optimizations for i.MX8M Plus

See CHANGELOG.md for complete details."

# Push tag to remote
git push --tags

# Or push specific tag
git push origin 1.5.0
```

**Tag naming**:
- Release: `1.5.0`
- Release candidate: `1.5.0-rc0`, `1.5.0-rc1`, etc.
- Beta: `1.5.0-beta0`
- Alpha: `1.5.0-alpha0`

#### Step 7: Monitor Release Workflow

After pushing the tag, GitHub Actions automatically triggers the release workflow:

```bash
# Watch: https://github.com/EdgeFirstAI/videostream/actions/workflows/release.yml
```

**The release workflow**:
1. **Builds release artifacts**:
   - Source tarball
   - Debian package
   - Python wheel
   - Documentation
2. **Creates GitHub Release**:
   - Extracts release notes from CHANGELOG.md
   - Attaches build artifacts
   - Marks as latest release
3. **Publishes packages**:
   - PyPI (Python Package Index)
   - crates.io (Rust packages)
   - GitHub Releases (archives)

**Verify success**:
```bash
# Check GitHub Release created:
# https://github.com/EdgeFirstAI/videostream/releases/tag/1.5.0

# Check PyPI published:
# https://pypi.org/project/videostream/1.5.0/

# Check crates.io published:
# https://crates.io/crates/videostream/1.5.0
```

If release workflow fails:
1. Check workflow logs for errors
2. Fix issues (may require code changes)
3. Delete the tag: `git tag -d 1.5.0 && git push origin :refs/tags/1.5.0`
4. Create new commits with fixes
5. Restart from Step 4 (create new release commit)

### Post-Release Tasks

1. **Verify published packages**:
   ```bash
   # Test PyPI installation
   pip install videostream==1.5.0
   
   # Test crates.io download
   cargo search videostream
   ```

2. **Update project documentation** (if needed):
   - Update README.md installation instructions
   - Update EdgeFirst Studio integration docs
   - Update examples if API changed

3. **Announce release**:
   - GitHub Discussions announcement
   - Project mailing list (if applicable)
   - Social media / blog post (for major releases)

4. **Create milestone for next version**:
   ```bash
   # On GitHub: Projects → Milestones → New Milestone
   # Version: 1.5.1 or 1.6.0
   ```

### Troubleshooting

#### Problem: Forgot to update a version file

**Symptoms**: CI fails with version mismatch errors, or GitHub Release fails

**Solution**:
```bash
# 1. Delete the tag
git tag -d 1.5.0
git push origin :refs/tags/1.5.0

# 2. Fix the missing version file
vim doc/conf.py  # or whichever file was missed

# 3. Amend the release commit
git add doc/conf.py
git commit --amend --no-edit

# 4. Force-push the commit
git push origin main --force-with-lease

# 5. Recreate and push the tag
git tag -a -m "Version 1.5.0" 1.5.0
git push origin 1.5.0
```

#### Problem: CI/CD failed after tagging

**Symptoms**: Tag exists but GitHub Release not created, or packages not published

**Solution**:
```bash
# 1. Fix the issue causing CI failure
# (may require new commits)

# 2. Delete the tag
git tag -d 1.5.0
git push origin :refs/tags/1.5.0

# 3. If fixes required new commits:
#    Update version in commit message
git commit --amend -m "Prepare Version 1.5.0

- Updated all version files to 1.5.0
- Fixed CI/CD configuration issue
- Finalized CHANGELOG.md with release notes"

# 4. Push fixes
git push origin main --force-with-lease

# 5. Recreate tag once CI passes
git tag -a -m "Version 1.5.0" 1.5.0
git push origin 1.5.0
```

#### Problem: Version mismatch between files

**Symptoms**: CMake complains about version, Python package version wrong, etc.

**Solution**:
```bash
# Verify all versions match
grep -r "1.5.0" Cargo.toml include/videostream.h pyproject.toml doc/conf.py debian/changelog

# If any don't match, fix them all:
vim <file>

# Amend the commit
git add <files>
git commit --amend --no-edit

# If already tagged and pushed:
git tag -d 1.5.0
git push origin :refs/tags/1.5.0
git push origin main --force-with-lease
git tag -a -m "Version 1.5.0" 1.5.0
git push origin 1.5.0
```

### Emergency: Rollback a Release

If a critical issue is discovered immediately after release:

```bash
# 1. Delete GitHub Release (via web UI)
# Go to: https://github.com/EdgeFirstAI/videostream/releases
# Click on release → Delete

# 2. Delete git tag
git tag -d 1.5.0
git push origin :refs/tags/1.5.0

# 3. Yank packages (mark as unavailable)
# PyPI: Use web UI to yank release
# crates.io: cargo yank --version 1.5.0

# 4. Create hotfix release (1.5.1) with fix
# Follow normal release process
```

### Release Checklist

Pre-release:
- [ ] All tests passing locally
- [ ] Code formatted (clang-format)
- [ ] SBOM generated and clean
- [ ] Determined correct version (MAJOR.MINOR.PATCH)
- [ ] CHANGELOG.md updated with release notes
- [ ] Migration guide added (if breaking changes)

Version updates:
- [ ] `Cargo.toml` updated
- [ ] `include/videostream.h` updated (VSL_VERSION)
- [ ] `pyproject.toml` updated
- [ ] `doc/conf.py` updated
- [ ] `debian/changelog` updated (new entry at top)
- [ ] All files show **same version** (verified with grep)

Commit and push:
- [ ] Release commit created (`Prepare Version X.Y.Z`)
- [ ] Pushed to main branch
- [ ] CI/CD all green (build, test, SBOM, quality checks)

Tagging:
- [ ] Git tag created (`git tag -a -m "Version X.Y.Z" X.Y.Z`)
- [ ] Tag pushed to remote
- [ ] Release workflow triggered and succeeded

Post-release:
- [ ] GitHub Release created automatically
- [ ] PyPI package published
- [ ] crates.io packages published
- [ ] Installation verified (`pip install videostream==X.Y.Z`)
- [ ] Documentation updated (if needed)
- [ ] Release announced (if public)

### Version File Reference

**Complete list of files requiring version updates**:

| File | Format | Location | Notes |
|------|--------|----------|-------|
| `Cargo.toml` | `version = "X.Y.Z"` | Line ~6, `[workspace.package]` section | Rust workspace |
| `include/videostream.h` | `#define VSL_VERSION "X.Y.Z"` | Line ~16 | C library, CMake source |
| `pyproject.toml` | `version = "X.Y.Z"` | Line ~7, `[project]` section | Python package |
| `doc/conf.py` | `version = 'X.Y.Z'` | Line ~28 | Sphinx docs (note single quotes) |
| `debian/changelog` | `videostream (X.Y.Z-1) stable; urgency=medium` | Top of file, new entry | Debian package |
| `CHANGELOG.md` | `## [X.Y.Z] - YYYY-MM-DD` | After `[Unreleased]` section | Release notes |

**Automated checks** (in CI/CD):
- `CMakeLists.txt` - Parses `VSL_VERSION` from `include/videostream.h` automatically (no manual update needed)
- `crates/*/Cargo.toml` - Use `version.workspace = true`, inherits from workspace `Cargo.toml` (no manual update needed)

### References

- **Semantic Versioning**: https://semver.org/
- **Keep a Changelog**: https://keepachangelog.com/
- **Debian Changelog Format**: https://www.debian.org/doc/debian-policy/ch-source.html#s-dpkgchangelog
- **VideoStream CHANGELOG.md**: Follow Keep a Changelog format
- **GitHub Release Workflow**: `.github/workflows/release.yml`

---

*For questions or clarifications about contributing, please email [support@au-zone.com](mailto:support@au-zone.com) or ask in [GitHub Discussions](https://github.com/EdgeFirstAI/videostream/discussions).*
