# AGENTS.md - AI Assistant Development Guidelines

This document provides instructions for AI coding assistants (GitHub Copilot, Cursor, Claude Code, etc.) working on the VideoStream project. These guidelines ensure consistent code quality, proper workflow adherence, and maintainable contributions.

**Version:** 2.0
**Last Updated:** 2025-11-22
**Applies To:** EdgeFirst VideoStream Library

---

## Table of Contents

1. [Overview](#overview)
2. [Critical Rules](#critical-rules)
3. [Git Workflow](#git-workflow)
4. [Code Quality Standards](#code-quality-standards)
5. [Testing Requirements](#testing-requirements)
6. [License Policy](#license-policy)
7. [Security Practices](#security-practices)
8. [VideoStream-Specific Guidelines](#videostream-specific-guidelines)
9. [Release Process](#release-process)

---

## Overview

EdgeFirst VideoStream Library provides video I/O capabilities for embedded Linux applications, including camera capture, hardware encoding, and inter-process frame sharing.

When contributing to VideoStream, AI assistants should prioritize:

- **Resource efficiency**: Memory, CPU, and power consumption matter on embedded devices
- **Code quality**: Maintainability, readability, and adherence to established patterns
- **Testing**: Comprehensive coverage with unit, integration, and edge case tests
- **Documentation**: Clear explanations for complex logic and public APIs
- **License compliance**: Strict adherence to approved open source licenses
- **Stay in project root**: **NEVER** change directories - use paths instead

---

## Quick Reference

**Branch:** `feature/EDGEAI-###-description` or `bugfix/EDGEAI-###-description`
**Commit:** `EDGEAI-###: Brief description` (50-72 chars, what changed not how)
**PR:** main=2 approvals, develop=1. Link JIRA ticket, ensure CI passes.

**License Policy:** ✅ MIT/Apache/BSD | ⚠️ LGPL (dynamic only) | ❌ GPL/AGPL
**Coverage:** 70% minimum, 80%+ for core modules (lib/host.c, lib/client.c, lib/frame.c)
**Output:** ❌ NEVER filter build/test/sbom with head/tail/grep | ✅ Use tee for long output

---

## ⚠️ Critical Rules

### Rule #1: NEVER Use cd Commands

**BANNED: Changing directories during command execution**

**❌ NEVER DO THIS:**

```bash
cd build && cmake ..
cd tests && pytest
```

**✅ ALWAYS DO THIS:**

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
pytest tests/
```

**Why:** AI context doesn't reliably track current directory. Stay in project root, use paths.

### Rule #2: ALWAYS Use Python venv

**❌ NEVER DO THIS:**

```bash
pip install package
python script.py
pytest tests/
```

**✅ ALWAYS DO THIS:**

```bash
venv/bin/pip install package
venv/bin/python script.py
venv/bin/pytest tests/
```

**Setup:**

```bash
python3 -m venv venv
venv/bin/pip install -r requirements.txt
```

### Rule #3: Source env.sh Before Tests (If Exists)

```bash
# env.sh contains ephemeral tokens (<48h lifespan) - NEVER commit!
[ -f env.sh ] && source env.sh
venv/bin/pytest tests/

# Or use make test (handles venv + env.sh automatically)
make test
```

**Security:** ✅ Ephemeral tokens | ❌ NEVER commit env.sh | ❌ NO passwords

### Rule #4: NEVER Filter Output of Non-Trivial Commands

**BANNED: Using head, tail, grep to hide output that users need to monitor**

**❌ NEVER DO THIS:**

```bash
make test | head -20
cmake --build build 2>&1 | grep -i error
make sbom | tail -10
venv/bin/pytest tests/ | grep PASSED
```

**✅ ALWAYS DO THIS:**

```bash
# Show full output for commands users need to monitor
make test
cmake --build build
make sbom
venv/bin/pytest tests/

# For very long output, capture to file for review
make test 2>&1 | tee test-output.log
cmake --build build 2>&1 | tee build-output.log
```

**Exception: Fast parsing commands are OK:**

```bash
# Quick version checks - OK to filter
grep "VSL_VERSION" include/videostream.h
cat pyproject.toml | grep "^version"
```

**Why:**

- **Builds/Tests/SBOM**: Users need to see progress, warnings, errors in real-time
- **Debugging**: Filtering hides critical context (warnings before errors, timing info)
- **CI/CD transparency**: Full output shows what actually happened
- **tee preserves output**: User sees everything AND gets log file for detailed review

**What counts as "non-trivial":**

- ❌ Build commands (cmake, make, cargo build)
- ❌ Test suites (pytest, cargo test, make test)
- ❌ SBOM generation (make sbom)
- ❌ Linting/formatting with multiple files (make lint, make format)
- ❌ Any command taking >1 second
- ✅ Simple parsing (grep version from single file)
- ✅ File counting (wc -l, ls | wc)

---

## Git Workflow

### Branch Naming

**Format**: `<type>/EDGEAI-###[-description]`

**Types:**

- `feature/` - New features
- `bugfix/` - Bug fixes
- `hotfix/` - Critical fixes

**Examples:**

```bash
feature/EDGEAI-123-add-dmabuf-support
bugfix/EDGEAI-456-fix-memory-leak
```

### Commit Messages

**Format**: `EDGEAI-###: Brief description`

**Good commits:**

```bash
EDGEAI-123: Add DmaBuf support for zero-copy frame sharing
EDGEAI-456: Fix memory leak in frame pool cleanup
```

### Pull Requests

- **main**: 2 approvals required
- **develop**: 1 approval required
- All CI/CD checks must pass
- Link JIRA ticket in description

---

## Code Quality Standards

### Language Standards

**C11:**

- Compiler flags: `-Wall -Wextra -Werror`
- Formatter: clang-format (`make format`)
- Error handling: Return negative errno values

**Rust:**

- Toolchain: Stable (1.70+)
- Formatter: `cargo fmt`
- Linter: `cargo clippy -- -D warnings` (`make lint`)

**Python:**

- Version: 3.8+
- Formatter: autopep8 (in venv)
- Always use venv

### Performance Considerations

Edge AI constraints:

- **Memory**: 512MB-2GB RAM
- **CPU**: <2% for frame distribution
- **Latency**: <3ms for zero-copy DmaBuf
- **Hardware**: Leverage G2D, Hantro VPU when available

---

## Testing Requirements

### Coverage

- **Minimum**: 70% (project-wide)
- **Core modules**: 80%+ (lib/host.c, lib/client.c, lib/frame.c)
- **Edge cases**: Explicit tests for boundaries
- **Error paths**: Validate error handling

### Running Tests

```bash
# Using make (recommended - handles venv + env.sh + library path)
make test

# Manual (requires setup)
[ -f env.sh ] && source env.sh
source venv/bin/activate
export VIDEOSTREAM_LIBRARY=./build/libvideostream.so.1
pytest tests/
```

**Test files:**

- `tests/test_frame.py` - Frame lifecycle
- `tests/test_ipc.py` - IPC protocol
- `tests/test_host.py` - Host-side management
- `tests/test_client.py` - Client-side acquisition

---

## License Policy

### Allowed ✅

MIT, Apache-2.0, BSD-2/3, ISC, 0BSD, Unlicense, Public Domain

### Conditional ⚠️

LGPL (dynamic linking only - NEVER static)
MPL-2.0 (external dependencies only)

### Blocked ❌

GPL (any version), AGPL, SSPL, Commons Clause

### Verification

```bash
# REQUIRED before release
make sbom
```

CI/CD automatically blocks GPL/AGPL violations.

---

## Security Practices

### Vulnerability Reporting

Email: `support@au-zone.com` (Subject: "Security Vulnerability - VideoStream")

### Secure Coding

**Input Validation:**

- Validate all external inputs
- Use allowlists, enforce size limits

**Credentials:**

- NEVER hardcode credentials
- Use ephemeral tokens (<48h) in env.sh
- NEVER commit env.sh

**DmaBuf Security:**

- Understand FDs grant direct memory access
- Close FDs immediately after use
- Don't share with untrusted processes

---

## VideoStream-Specific Guidelines

### Technology Stack

- **Language**: C11 (ISO/IEC 9899:2011)
- **Build system**: CMake 3.10+
- **Dependencies**:
  - GStreamer 1.4+ (LGPL-2.1)
  - GLib 2.0+ (LGPL-2.1)
  - pthread
  - stb libraries (Public Domain)
- **Platforms**: NXP i.MX8, ARM64/ARMv7, x86_64
- **Acceleration**: G2D, Hantro VPU, V4L2 DmaBuf
- **Kernel**: Linux 4.14+ (5.6+ recommended)

### Architecture

**Pattern:** Host/Client IPC with event-driven frame sharing

**Layers:**

- Public API (vsl_host_*, vsl_client_*, vsl_frame_*)
- IPC Protocol (UNIX sockets, FD passing)
- Frame Management (reference counting)
- Memory Allocation (DmaBuf / POSIX shm)

**Threading:**

- Host: Main + GStreamer task thread
- Client: Main + POSIX timer thread

**Error handling:** Negative errno values (e.g., -EINVAL), 0 for success

See [ARCHITECTURE.md](ARCHITECTURE.md) for details.

### Build Commands

**IMPORTANT:** Use modern CMake workflow - stay in project root.

```bash
# Build (Debug)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# Build (Release)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run tests
make test

# Format code
make format

# Lint code
make lint

# Generate SBOM
make sbom

# Verify version consistency
make verify-version

# Generate PDF documentation
make doc

# All pre-release checks
make pre-release

# Cross-compile for ARM64
source /opt/fsl-imx-xwayland/5.4-zeus/environment-setup-aarch64-poky-linux
cmake -S . -B build-arm64 -DCMAKE_BUILD_TYPE=Release
cmake --build build-arm64 -j$(nproc)

# Install
cmake --install build

# Clean
make clean
```

**CMake Options:**

- `-DENABLE_GSTREAMER=ON/OFF` - Build GStreamer plugins (default: ON)
- `-DENABLE_DMABUF=ON/OFF` - Enable DmaBuf support (default: ON on Linux)
- `-DENABLE_G2D=ON/OFF` - Enable G2D acceleration (default: ON on Linux)
- `-DENABLE_VPU=ON/OFF` - Enable VPU encode/decode (default: ON on Linux)
- `-DBUILD_TESTING=ON/OFF` - Build test suite (default: OFF)
- `-DENABLE_COVER=ON/OFF` - Enable code coverage (default: OFF)

### Performance Targets

**NXP i.MX8M Plus:**

- Frame latency: <3ms (DmaBuf), <10ms (shm)
- Throughput: 1080p@60fps (3 clients), 4K@30fps (1 client)
- Memory overhead: <100KB per client
- CPU usage: <2% for distribution (1080p30)
- Startup: <100ms

**Critical paths:**

- Frame registration: <0.1ms
- Event broadcast: <0.5ms per client
- Lock/unlock round-trip: <2ms

### Hardware Platforms

**Supported:**

- **NXP i.MX8M Plus**: Full (G2D, VPU, DmaBuf)
- **NXP i.MX8M**: DmaBuf + basic acceleration
- **Generic ARM64/ARMv7**: POSIX shm fallback
- **x86_64**: Development/testing (shm mode)

**Acceleration:**

- **G2D**: Format conversion, scaling, rotation
- **Hantro VPU**: H.264/H.265 encode/decode
- **V4L2 DMABUF**: Zero-copy camera capture

**Platform quirks:**

- i.MX8: G2D requires contiguous physical memory (CMA)
- DmaBuf heap: `/dev/dma_heap/system` on Linux 5.6+
- Permissions: User must be in `video` group

---

## Release Process

**CRITICAL:** VideoStream uses manual release requiring version sync across 6 files.

### Version Files

1. `Cargo.toml` - `version = "X.Y.Z"`
2. `include/videostream.h` - `#define VSL_VERSION "X.Y.Z"`
3. `pyproject.toml` - `version = "X.Y.Z"`
4. `doc/conf.py` - `version = 'X.Y.Z'` (single quotes!)
5. `debian/changelog` - `videostream (X.Y.Z-1) stable`
6. `CHANGELOG.md` - `## [X.Y.Z] - YYYY-MM-DD`

**Note:** `CMakeLists.txt` parses from `include/videostream.h` automatically.

### Release Workflow

**Step 1: Pre-release checks**

```bash
make pre-release
# Runs: format, lint, verify-version, test, sbom
```

**Step 2: Determine version**

- **PATCH** (X.Y.Z): Bug fixes, non-breaking changes
- **MINOR** (X.Y.0): New features, breaking changes
- **MAJOR** (X.0.0): Only on explicit request

**Step 3: Update CHANGELOG.md**

- Move items from `## [Unreleased]` to new version
- Follow [Keep a Changelog](https://keepachangelog.com/) format

**Step 4: Update ALL 6 version files**

```bash
make verify-version
```

**Step 5: Commit**

```bash
git commit -a -m "Prepare Version X.Y.Z

- Updated all version files to X.Y.Z
- Finalized CHANGELOG.md
- See CHANGELOG.md for details"

git push origin main
```

**Step 6: Wait for CI/CD** (all green checkmarks)

**Step 7: Tag and push**

```bash
# v prefix REQUIRED to trigger release workflow
git tag -a -m "Version X.Y.Z" vX.Y.Z
git push origin vX.Y.Z
```

### Common Mistakes

❌ Forgetting to update all 6 version files
❌ Tagging before CI/CD passes
❌ Missing `v` prefix on tag
❌ Running tests without venv or library path
❌ Using `cd` commands
❌ Filtering output of build/test/sbom commands

✅ Run `make pre-release`, wait for CI, tag with `vX.Y.Z`

---

## AI Assistant Best Practices

**Verify:**

- APIs exist (no hallucinations)
- License compatibility before adding deps
- Test coverage meets 70% minimum
- Version files synchronized before release
- CI/CD passes before tagging

**Avoid:**

- Using `cd` commands (stay in root)
- System Python (always use venv)
- GPL/AGPL dependencies
- Hardcoded secrets
- Committing env.sh
- Filtering output with head/tail/grep on non-trivial commands

**Remember:** Review all AI-generated code thoroughly. YOU are the author, AI is a tool.

---

## Getting Help

**Documentation:**

- [README.md](README.md) - Overview and quick start
- [ARCHITECTURE.md](ARCHITECTURE.md) - Internal design
- [DESIGN.md](DESIGN.md) - High-level architecture
- [CONTRIBUTING.md](CONTRIBUTING.md) - Detailed contribution guide
- [EdgeFirst Docs](https://doc.edgefirst.ai/test/perception/)

**Support:** support@au-zone.com

---

**v2.0** | 2025-11-22 | EdgeFirst VideoStream Library

*This file helps AI assistants contribute effectively to VideoStream while maintaining quality, security, and consistency.*
