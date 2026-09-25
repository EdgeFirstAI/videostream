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
   - Read [ARCHITECTURE.md](ARCHITECTURE.md) for design and implementation details
   - Read [HARDWARE.md](HARDWARE.md) for platform-specific capabilities
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

**Note:** VideoStream uses modern CMake workflow - all commands run from the project root directory. This ensures consistency and avoids directory confusion.

### Standard Build (Debug)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)
```

### Release Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
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
cmake -S . -B build \
      -DCMAKE_BUILD_TYPE=Debug \
      -DENABLE_COVER=ON \
      -DBUILD_TESTING=ON
cmake --build build -j$(nproc)
```

### Cross-Compiling for ARM/ARM64

#### Using NXP Yocto SDK

```bash
# Source the SDK environment
source /opt/fsl-imx-xwayland/5.4-zeus/environment-setup-aarch64-poky-linux

# Configure and build
cmake -S . -B build-arm64 -DCMAKE_BUILD_TYPE=Release
cmake --build build-arm64 -j$(nproc)
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
# Create virtual environment (first time only)
python3 -m venv venv
venv/bin/pip install -r requirements.txt

# Build library
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# Set library path for ctypes loading
export VIDEOSTREAM_LIBRARY=./build/libvideostream.so.1

# Run tests
venv/bin/pytest tests/

# Or use make target (handles venv, env.sh, and library path automatically)
make test
```

**Test Options:**

```bash
# Run specific test file
venv/bin/pytest tests/test_frame.py

# Run with verbose output
venv/bin/pytest -v tests/

# Run with coverage
venv/bin/pytest --cov=videostream tests/
```

### Environment Configuration (Optional)

Some integration tests may require external services (APIs, databases). Create `env.sh` for local development:

```bash
#!/bin/bash
# env.sh - LOCAL DEVELOPMENT ONLY (never commit!)
export API_ENDPOINT="https://dev.example.com/api"
export API_TOKEN="eyJ0eXAi...expires-in-24h"  # Ephemeral token, 24-48h lifespan
```

**CRITICAL:** `env.sh` is in `.gitignore` and must NEVER be committed. Use ephemeral tokens, not passwords.

**Usage:**

```bash
# Source env.sh if it exists
[ -f env.sh ] && source env.sh

# Run integration tests
venv/bin/pytest tests/
```

**CI/CD:** Production environments use secure vaults (GitHub Secrets), not env.sh files.

### Running C Unit Tests (Future)

C unit tests are planned for a future release.

```bash
# Once implemented:
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
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
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build

# Run with valgrind
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         build/src/vsl-monitor /tmp/test-vsl
```

---

## Contribution Process

### 1. Fork and Clone

1. Go to https://github.com/EdgeFirstAI/videostream and click **Fork**
2. Clone your fork and add the upstream remote:

```bash
git clone https://github.com/YOUR_USERNAME/videostream.git
git remote add upstream https://github.com/EdgeFirstAI/videostream.git
git fetch upstream
```

---

## Code Style

### C Code Style

VideoStream follows **C11 standard** with the following conventions:

#### Formatting

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
- [ ] PR targets `main`

## Related Issues
Fixes #123
Related to #456

## Additional Notes
Any other context, screenshots, or information reviewers should know.
```

### PR Review Process

1. **Automated Checks**: `ci-gate` must pass. The Quick tier runs on every push to a non-draft PR; a reviewer adds `ci:full` for the full platform, on-target and coverage run
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

- Mentioned in release notes for significant contributions
- Acknowledged in commit messages and PR comments

---

**Thank you for contributing to VideoStream and the EdgeFirst Perception ecosystem!**

Your contributions help make edge AI more accessible and efficient for developers worldwide.

---

## Release Process (Maintainers)

Releases go through a pull request, and the tag is created by CI rather than by hand. The process is the organisation-wide release chain described in the [canonical guide](https://github.com/EdgeFirstAI/.github/blob/main/.github/copilot-instructions.md#release-chain): a tag deploys artifacts that were already built on the release branch; it never builds.

| Workflow | Trigger | Action |
|----------|---------|--------|
| `release.yml` | push to `release/X.Y.Z` | Verifies versions and the CHANGELOG section, then builds every artifact: crates, wheel and sdist, ZIP archives, Debian packages, PDF manual, SBOM |
| `tag-release.yml` | release PR merged into `main` | Creates the annotated `vX.Y.Z` tag once `release.yml` is green for the merged head |
| `publish.yml` | push of the `vX.Y.Z` tag | Publishes the built artifacts to crates.io, PyPI and the GitHub Release |

### Steps

1. Branch `release/X.Y.Z` from an up-to-date `main`. Use `release/X.Y.Z-rcN` for a release candidate.
2. Update every version location (table below) and move the CHANGELOG `[Unreleased]` entries under `## [X.Y.Z] - YYYY-MM-DD`.
3. Run `make pre-release` locally. It formats, lints, tests, generates the SBOM and runs `make verify-version`.
4. Commit (`git commit -s`), push the branch, and open a PR to `main` labelled `ci:full`.
5. Wait for both the PR checks (`ci-gate`) and the `Release Build` run on the branch to go green. Fix problems with further commits on the branch; each push rebuilds.
6. Merge the PR. The tag and the publish follow automatically.

If the release build is red when the PR merges, no tag is created. Fix it on a new release branch rather than tagging by hand.

### Version File Reference

| File | Format | Notes |
|------|--------|-------|
| `include/videostream.h` | `#define VSL_VERSION "X.Y.Z"` | C library; CMake parses the version from here |
| `Cargo.toml` | `version = "X.Y.Z"` | `[workspace.package]` and the `videostream-sys` / `videostream` workspace dependencies |
| `pyproject.toml` | `version = "X.Y.Z"` | Python package |
| `doc/conf.py` | `version = 'X.Y.Z'` | Sphinx (single quotes) |
| `crates/videostream-sys/src/ffi.rs` | `VSL_VERSION` constant | Regenerate with `crates/videostream-sys/update.sh` |
| `NOTICE` | `videostream-sys X.Y.Z` | Third-party notice line |
| `CHANGELOG.md` | `## [X.Y.Z] - YYYY-MM-DD` | Release notes; the GitHub Release body is taken from this section |

`debian/changelog` is generated from `CHANGELOG.md` at build time by `debian/gen-changelog.py`.

### Rolling Back

A published crates.io or PyPI version is immutable. Yank it (`cargo yank --version X.Y.Z`, PyPI web UI) and release a patch version through the same process.

### References

- **Semantic Versioning**: https://semver.org/
- **Keep a Changelog**: https://keepachangelog.com/
- **CICD Pipelines design**: https://au-zone.atlassian.net/wiki/spaces/EAM/pages/2750906369/CICD+Pipelines

---

*For questions or clarifications about contributing, please email [support@au-zone.com](mailto:support@au-zone.com) or ask in [GitHub Discussions](https://github.com/EdgeFirstAI/videostream/discussions).*
