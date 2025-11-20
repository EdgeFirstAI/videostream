# AGENTS.md - AI Assistant Development Guidelines

This document provides instructions for AI coding assistants (GitHub Copilot, Cursor, Claude Code, etc.) working on Au-Zone Technologies projects. These guidelines ensure consistent code quality, proper workflow adherence, and maintainable contributions.

**Version:** 1.0
**Last Updated:** November 2025
**Applies To:** All Au-Zone Technologies software repositories

---

## Table of Contents

1. [Overview](#overview)
2. [Git Workflow](#git-workflow)
3. [Code Quality Standards](#code-quality-standards)
4. [Testing Requirements](#testing-requirements)
5. [Documentation Expectations](#documentation-expectations)
6. [License Policy](#license-policy)
7. [Security Practices](#security-practices)
8. [Project-Specific Guidelines](#project-specific-guidelines)
9. [Release Process](#release-process)

---

## Overview

Au-Zone Technologies develops edge AI and computer vision solutions for resource-constrained embedded devices. Our software spans:
- Edge AI inference engines and model optimization tools
- Computer vision processing pipelines
- Embedded Linux device drivers and system software
- MLOps platform (EdgeFirst Studio) for model deployment and management
- Open source libraries and tools (Apache-2.0 licensed)

When contributing to Au-Zone projects, AI assistants should prioritize:
- **Resource efficiency**: Memory, CPU, and power consumption matter on embedded devices
- **Code quality**: Maintainability, readability, and adherence to established patterns
- **Testing**: Comprehensive coverage with unit, integration, and edge case tests
- **Documentation**: Clear explanations for complex logic and public APIs
- **License compliance**: Strict adherence to approved open source licenses

---

## Git Workflow

### Branch Naming Convention

**REQUIRED FORMAT**: `<type>/<PROJECTKEY-###>[-optional-description]`

**Branch Types:**
- `feature/` - New features and enhancements
- `bugfix/` - Non-critical bug fixes
- `hotfix/` - Critical production issues requiring immediate fix

**Examples:**
```bash
feature/EDGEAI-123-add-authentication
bugfix/STUDIO-456-fix-memory-leak
hotfix/MAIVIN-789-security-patch

# Minimal format (JIRA key only)
feature/EDGEAI-123
bugfix/STUDIO-456
```

**Rules:**
- JIRA key is REQUIRED (format: `PROJECTKEY-###`)
- Description is OPTIONAL but recommended for clarity
- Use kebab-case for descriptions (lowercase with hyphens)
- Branch from `develop` for features/bugfixes, from `main` for hotfixes

### Commit Message Format

**REQUIRED FORMAT**: `PROJECTKEY-###: Brief description of what was done`

**Rules:**
- Subject line: 50-72 characters ideal
- Focus on WHAT changed, not HOW (implementation details belong in code)
- No type prefixes (`feat:`, `fix:`, etc.) - JIRA provides context
- Optional body: Use bullet points for additional detail

**Examples of Good Commits:**
```bash
EDGEAI-123: Add JWT authentication to user API

STUDIO-456: Fix memory leak in CUDA kernel allocation

MAIVIN-789: Optimize tensor operations for inference
- Implemented tiled memory access pattern
- Reduced memory bandwidth by 40%
- Added benchmarks to verify improvements
```

**Examples of Bad Commits:**
```bash
fix bug                           # Missing JIRA key, too vague
feat(auth): add OAuth2           # Has type prefix (not our convention)
EDGEAI-123                       # Missing description
edgeai-123: update code          # Lowercase key, vague description
```

### Pull Request Process

**Requirements:**
- **2 approvals required** for merging to `main`
- **1 approval required** for merging to `develop`
- All CI/CD checks must pass
- PR title: `PROJECTKEY-### Brief description of changes`
- PR description must link to JIRA ticket

**PR Description Template:**
```markdown
## JIRA Ticket
Link: [PROJECTKEY-###](https://au-zone.atlassian.net/browse/PROJECTKEY-###)

## Changes
Brief summary of what changed and why

## Testing
- [ ] Unit tests added/updated
- [ ] Integration tests pass
- [ ] Manual testing completed

## Checklist
- [ ] Code follows project conventions
- [ ] Documentation updated
- [ ] No secrets or credentials committed
- [ ] License policy compliance verified
```

**Process:**
1. Create PR via GitHub/Bitbucket web interface
2. Link to JIRA ticket in description
3. Wait for CI/CD to complete successfully
4. Address reviewer feedback through additional commits
5. Obtain required approvals
6. Merge using squash or rebase to keep history clean

### JIRA Integration

While full JIRA details are internal, contributors should know:
- **Branch naming triggers automation**: Creating a branch with format `<type>/PROJECTKEY-###` automatically updates the linked JIRA ticket
- **PR creation triggers status updates**: Opening a PR moves tickets to review status
- **Merge triggers closure**: Merging a PR to main/develop closes the associated ticket
- **Commit messages link to JIRA**: Format `PROJECTKEY-###: Description` creates automatic linkage

**Note**: External contributors without JIRA access can use branch naming like `feature/issue-123-description` referencing GitHub issue numbers instead.

---

## Code Quality Standards

### General Principles

- **Consistency**: Follow existing codebase patterns and conventions
- **Readability**: Code is read more often than written - optimize for comprehension
- **Simplicity**: Prefer simple, straightforward solutions over clever ones
- **Error Handling**: Validate inputs, sanitize outputs, provide actionable error messages
- **Performance**: Consider time/space complexity, especially for edge deployment

### Language-Specific Standards

Follow established conventions for each language:
- **Rust**: Use `cargo fmt` and `cargo clippy`; follow Rust API guidelines
- **Python**: Follow PEP 8; use autopep8 formatter (or project-specified tool); type hints preferred
- **C/C++**: Follow project's .clang-format; use RAII patterns
- **Go**: Use `go fmt`; follow Effective Go guidelines
- **JavaScript/TypeScript**: Use ESLint; Prettier formatter; prefer TypeScript

### Code Quality Tools

**SonarQube Integration:**
- Projects with `sonar-project.properties` must follow SonarQube guidelines
- Verify code quality using:
  - MCP integration for automated checks
  - VSCode SonarLint plugin for real-time feedback
  - SonarCloud reports in CI/CD pipeline
- Address critical and high-severity issues before submitting PR
- Maintain or improve project quality gate scores

### Code Review Checklist

Before submitting code, verify:
- [ ] Code follows project style guidelines (check `.editorconfig`, `CONTRIBUTING.md`)
- [ ] No commented-out code or debug statements
- [ ] Error handling is comprehensive and provides useful messages
- [ ] Complex logic has explanatory comments
- [ ] Public APIs have documentation
- [ ] No hardcoded values that should be configuration
- [ ] Resource cleanup (memory, file handles, connections) is proper
- [ ] No obvious security vulnerabilities (SQL injection, XSS, etc.)
- [ ] SonarQube quality checks pass (if applicable)

### Performance Considerations

For edge AI applications, always consider:
- **Memory footprint**: Minimize allocations; reuse buffers where possible
- **CPU efficiency**: Profile critical paths; optimize hot loops
- **Power consumption**: Reduce wake-ups; batch operations
- **Latency**: Consider real-time requirements for vision processing
- **Hardware acceleration**: Leverage NPU/GPU/DSP when available

---

## Testing Requirements

### Coverage Standards

- **Minimum coverage**: 70% (project-specific thresholds may vary)
- **Critical paths**: 90%+ coverage for core functionality
- **Edge cases**: Explicit tests for boundary conditions
- **Error paths**: Validate error handling and recovery

### Test Types

**Unit Tests:**
- Test individual functions/methods in isolation
- Mock external dependencies
- Fast execution (< 1 second per test suite)
- Use property-based testing where applicable

**Integration Tests:**
- Test component interactions
- Use real dependencies when feasible
- Validate API contracts and data flows
- Test configuration and initialization

**Edge Case Tests:**
- Null/empty inputs
- Boundary values (min, max, overflow)
- Concurrent access and race conditions
- Resource exhaustion scenarios
- Platform-specific behaviors

### Test Organization

**Test layout follows language/framework conventions. Each project should define specific practices.**

**Rust (common pattern):**
```rust
// Unit tests at end of implementation file
// src/module/component.rs
pub fn process_data(input: &[u8]) -> Result<Vec<u8>, Error> {
    // implementation
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_process_data_valid_input() {
        // test implementation
    }
}
```

```
# Integration tests in separate directory
tests/
├── integration_test.rs
└── common/
    └── mod.rs
```

**Python (depends on pytest vs unittest):**
```
# Common patterns - follow project conventions
project/
├── src/
│   └── mypackage/
│       └── module.py
└── tests/
    ├── unit/
    │   └── test_module.py
    └── integration/
        └── test_api_workflow.py
```

**General guidance:**
- Follow common patterns for your language and testing framework
- Consult project's `CONTRIBUTING.md` for specific conventions
- Keep test organization consistent within the project
- Co-locate unit tests or separate - project decides

### Running Tests

```bash
# Run all tests
make test

# Run with coverage
make coverage

# Language-specific examples
cargo test --workspace              # Rust
pytest tests/                       # Python with pytest
python -m unittest discover tests/  # Python with unittest
go test ./...                       # Go
```

---

## Documentation Expectations

### Code Documentation

**When to document:**
- Public APIs, functions, and classes (ALWAYS)
- Complex algorithms or non-obvious logic
- Performance considerations or optimization rationale
- Edge cases and error conditions
- Thread safety and concurrency requirements
- Hardware-specific code or platform dependencies

**Documentation style:**
```python
def preprocess_image(image: np.ndarray, target_size: tuple[int, int]) -> np.ndarray:
    """
    Resize and normalize image for model inference.

    Args:
        image: Input image as HWC numpy array (uint8)
        target_size: Target dimensions as (width, height)

    Returns:
        Preprocessed image as CHW float32 array normalized to [0, 1]

    Raises:
        ValueError: If image dimensions are invalid or target_size is negative

    Performance:
        Uses bilinear interpolation. For better quality with 2x cost,
        use bicubic interpolation via config.interpolation = 'bicubic'
    """
```

### Project Documentation

**Essential files for public repositories:**
- `README.md` - Project overview, quick start, documentation links
- `CONTRIBUTING.md` - Development setup, contribution process, coding standards
- `CODE_OF_CONDUCT.md` - Community standards (Contributor Covenant)
- `SECURITY.md` - Vulnerability reporting process
- `LICENSE` - Complete license text (Apache-2.0 for open source)

**Additional documentation:**
- User guides for features and workflows
- API reference documentation
- Migration guides for breaking changes

### Documentation Updates

When modifying code, update corresponding documentation:
- README if user-facing behavior changes
- API docs if function signatures or semantics change
- CHANGELOG for all user-visible changes
- Configuration guides if new options added

---

## License Policy

**CRITICAL**: Au-Zone has strict license policy for all dependencies.

### Allowed Licenses

✅ **Permissive licenses (APPROVED)**:
- MIT
- Apache-2.0
- BSD-2-Clause, BSD-3-Clause
- ISC
- 0BSD
- Unlicense

### Review Required

⚠️ **Weak copyleft (REQUIRES LEGAL REVIEW)**:
- MPL-2.0 (Mozilla Public License)
- LGPL-2.1-or-later, LGPL-3.0-or-later (if dynamically linked)

### Strictly Disallowed

❌ **NEVER USE THESE LICENSES**:
- GPL (any version)
- AGPL (any version)
- Creative Commons with NC (Non-Commercial) or ND (No Derivatives)
- SSPL (Server Side Public License)
- BSL (Business Source License, before conversion)
- OSL-3.0 (Open Software License)

### Verification Process

**Before adding dependencies:**
1. Check license compatibility with project license (typically Apache-2.0)
2. Verify no GPL/AGPL in dependency tree
3. Review project's SBOM (Software Bill of Materials) if available
4. Document third-party licenses in NOTICE file

**CI/CD will automatically:**
- Generate SBOM using scancode-toolkit
- Validate CycloneDX SBOM schema
- Check for disallowed licenses
- Block PR merges if violations detected

**If you need a library with incompatible license:**
- Search for alternatives with permissive licenses
- Consider implementing functionality yourself
- Escalate to technical leadership for approval (rare exceptions)

---

## Security Practices

### Vulnerability Reporting

**For security issues**, use project's SECURITY.md process:
- Email: `support@au-zone.com` with subject "Security Vulnerability"
- Expected acknowledgment: 48 hours
- Expected assessment: 7 days
- Fix timeline based on severity

### Secure Coding Guidelines

**Input Validation:**
- Validate all external inputs (API requests, file uploads, user input)
- Use allowlists rather than blocklists
- Enforce size/length limits
- Sanitize for appropriate context (HTML, SQL, shell)

**Authentication & Authorization:**
- Never hardcode credentials or API keys
- Use environment variables or secure vaults for secrets
- Implement proper session management
- Follow principle of least privilege

**Data Protection:**
- Encrypt sensitive data at rest and in transit
- Use secure protocols (HTTPS, TLS 1.2+)
- Implement proper key management
- Sanitize logs (no passwords, tokens, PII)

**Common Vulnerabilities to Avoid:**
- SQL Injection: Use parameterized queries
- XSS (Cross-Site Scripting): Escape output, use CSP headers
- CSRF (Cross-Site Request Forgery): Use tokens
- Path Traversal: Validate and sanitize file paths
- Command Injection: Avoid shell execution; use safe APIs
- Buffer Overflows: Use safe string functions; bounds checking

### Dependencies

- Keep dependencies up to date
- Monitor for security advisories
- Use dependency scanning tools (Dependabot, Snyk)
- Audit new dependencies before adding

---

## Project-Specific Guidelines

This section should be customized per repository. Common customizations:

### Technology Stack

**VideoStream Library Stack:**
- **Language**: C11 (ISO/IEC 9899:2011)
- **Build system**: CMake 3.10+ with modular configuration
- **Key dependencies**:
  - GStreamer 1.4+ (multimedia framework, LGPL-2.1)
  - GLib 2.0+ (core utilities, LGPL-2.1)
  - pthread (POSIX threads)
  - stb libraries (image encoding, Public Domain / MIT-0)
- **Target platforms**: Embedded Linux (NXP i.MX8, generic ARM64/ARMv7)
- **Development platforms**: Linux (primary), macOS (limited), Windows (minimal support)
- **Hardware acceleration**: G2D, Hantro VPU (NXP-specific), V4L2 DmaBuf
- **Kernel requirements**: Linux 4.14+ (5.6+ recommended for DmaBuf heap)

### Architecture

**VideoStream Architecture:**
- **Pattern**: Host/Client IPC architecture with event-driven frame sharing
- **Layers**:
  - Public API (vsl_host_*, vsl_client_*, vsl_frame_*)
  - IPC Protocol Layer (UNIX domain sockets, FD passing)
  - Frame Management (reference counting, lifecycle)
  - Memory Allocation (DmaBuf / POSIX shared memory)
- **Threading**:
  - Host: Main thread + GStreamer task thread for client servicing
  - Client: Main thread + POSIX timer thread for timeouts
- **Data flow**: Frame Registration → Event Broadcast → Lock Request → FD Passing → Processing → Unlock → Recycling
- **Error handling**: Negative errno values (e.g., -EINVAL, -ENOMEM), 0 for success
- **GStreamer Integration**: vslsink (host) and vslsrc (client) plugins

See [ARCHITECTURE.md](ARCHITECTURE.md) for detailed threading diagrams and internal design.

### Build and Deployment

**VideoStream Build Commands:**

**IMPORTANT: Use Modern CMake Workflow** - Always use `cmake -S <source> -B <build>` and `cmake --build <build>` instead of changing directories. This prevents getting lost in the filesystem and works consistently across all environments.

```bash
# Standard build (Debug) - PREFERRED METHOD
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# Alternative (legacy, avoid if possible)
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)
cd ..  # Don't forget to return!

# Release build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Release with coverage enabled (for CI/testing)
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_COVER=ON
cmake --build build -j$(nproc)

# Run Python tests (from project root)
pytest tests/

# Cross-compile for ARM64 (NXP Yocto SDK)
source /opt/fsl-imx-xwayland/5.4-zeus/environment-setup-aarch64-poky-linux
cmake -S . -B build-arm64 -DCMAKE_BUILD_TYPE=Release
cmake --build build-arm64 -j$(nproc)

# Docker build (note: uses modern CMake workflow)
docker pull deepview/yocto-sdk-imx8mp
docker run -v $PWD:/src deepview/yocto-sdk-imx8mp \
    cmake -S/src -B/src/build -DCMAKE_BUILD_TYPE=Release
docker run -v $PWD:/src deepview/yocto-sdk-imx8mp \
    cmake --build /src/build -j16

# Install (from project root)
cmake --install build

# Clean build directory
rm -rf build

# Reconfigure existing build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DENABLE_COVER=ON

# Format code
find src lib gst include -name "*.[ch]" -exec clang-format -i {} \;
```

**CMake Workflow Best Practices:**
- **Always use `-S <source> -B <build>`**: Explicitly specify source and build directories
- **Never `cd` into build directories**: Use `cmake --build <path>` instead of `make`
- **Stay in project root**: All commands should be run from the workspace root
- **Use `cmake --install`**: Instead of `cd build && make install`
- **Parallel builds**: Use `-j$(nproc)` or `-j<N>` with `cmake --build`
- **Multiple configurations**: Use different build directories (e.g., `build-debug`, `build-release`, `build-arm64`)

**Why Modern Workflow Matters:**
1. **No directory confusion**: Your shell always stays in the project root
2. **IDE compatibility**: Modern IDEs expect this pattern
3. **CI/CD friendly**: Works in containers and automation without state tracking
4. **Cross-platform**: Same commands work on Linux, macOS, Windows
5. **Generator agnostic**: Works with Make, Ninja, Visual Studio, Xcode, etc.

**CMake Options:**
- `-DENABLE_GSTREAMER=ON/OFF` - Build GStreamer plugins (default: ON)
- `-DENABLE_DMABUF=ON/OFF` - Enable DmaBuf support (default: ON on Linux)
- `-DENABLE_G2D=ON/OFF` - Enable G2D acceleration (default: ON on Linux)
- `-DENABLE_VPU=ON/OFF` - Enable VPU encode/decode (default: ON on Linux)
- `-DBUILD_TESTING=ON/OFF` - Build test suite (default: OFF)
- `-DENABLE_COVER=ON/OFF` - Enable code coverage (default: OFF)

### Performance Targets

**VideoStream Performance Goals (NXP i.MX8M Plus):**
- Frame latency: < 3ms (zero-copy DmaBuf path)
- Frame latency: < 10ms (shared memory fallback)
- Throughput: 1080p @ 60 fps with 3 concurrent clients
- Throughput: 4K @ 30 fps with single client
- Memory overhead: < 100 KB per client connection
- CPU usage: < 2% for frame distribution (1080p30)
- Startup time: < 100ms (pool initialization + IPC setup)

**Critical Paths to Optimize:**
- Frame registration (target: < 0.1ms)
- Event broadcast (target: < 0.5ms per client)
- Lock/unlock round-trip (target: < 2ms)
- mmap() overhead (kernel-dependent, monitor closely)

### Hardware Specifics

**Supported Platforms:**
- **NXP i.MX8M Plus**: Full support (G2D, Hantro VPU, V4L2 DmaBuf)
- **NXP i.MX8M**: DmaBuf and basic acceleration
- **Generic ARM64/ARMv7**: POSIX shared memory fallback
- **x86_64**: Development/testing (shared memory mode)

**Hardware Acceleration:**
- **G2D** (NXP): Format conversion, scaling, rotation
  - Requires physical address for DMA operations
  - Accessed via `libg2d.so` (proprietary NXP library)
- **Hantro VPU** (NXP): H.264/H.265 encode/decode
  - Integrated via `vpu_wrapper` library
  - DmaBuf input/output for zero-copy
- **V4L2 DMABUF**: Camera buffer export
  - Use `VIDIOC_EXPBUF` ioctl for zero-copy capture

**Platform Quirks:**
- **i.MX8**: G2D requires contiguous physical memory (CMA)
- **DmaBuf heap**: `/dev/dma_heap/system` available on Linux 5.6+
- **Older kernels**: Fall back to `ion` allocator if available
- **Permissions**: Ensure user in `video` group for `/dev/video*` access

### Testing Conventions

**VideoStream Testing Structure:**

**Python Tests** (current implementation):
- Framework: **pytest**
- Location: `tests/` directory at project root
- Test files:
  - `tests/test_frame.py` - Frame lifecycle and reference counting
  - `tests/test_ipc.py` - IPC protocol and socket communication
  - `tests/test_host.py` - Host-side frame management
  - `tests/test_client.py` - Client-side frame acquisition
  - `tests/test_library.py` - End-to-end library tests
- Run with: `pytest tests/` or `pytest tests/test_<module>.py`
- Configuration: `pytest.ini` at project root

**C Unit Tests** (planned - see TODO.md):
- Framework: To be determined (Criterion, Unity, or Cmocka)
- Location: `tests/unit/` and `tests/integration/`
- Naming: `test_<module>_<scenario>.c`
- Run with: `ctest` or `make test`

**Manual Testing:**
- GStreamer pipelines: See README.md examples
- Hardware validation: Performed on Maivin/Raivin platforms by QA team
- Performance benchmarks: `v4l2src ! vslsink` → `vslsrc` clients

**Test Coverage Goals:**
- Minimum: 70% line coverage
- Target: 80%+ for core modules (lib/host.c, lib/client.c, lib/frame.c)

---

## Release Process

**CRITICAL FOR AI ASSISTANTS**: VideoStream uses a **manual release process** that requires updating version numbers across multiple files. This process MUST be followed exactly to ensure consistency.

### The Multi-Language Version Problem

VideoStream must keep versions synchronized across **6 different files**:
1. `Cargo.toml` (Rust workspace: `version = "X.Y.Z"`)
2. `include/videostream.h` (C: `#define VSL_VERSION "X.Y.Z"`)
3. `pyproject.toml` (Python: `version = "X.Y.Z"`)
4. `doc/conf.py` (Sphinx: `version = 'X.Y.Z'` - note single quotes)
5. `debian/changelog` (Debian: `videostream (X.Y.Z-1) stable; urgency=medium`)
6. `CHANGELOG.md` (Release notes: `## [X.Y.Z] - YYYY-MM-DD`)

**Note**: `CMakeLists.txt` automatically parses version from `include/videostream.h`, so it does NOT need manual updates.

### Manual Release Process

**Step 1: Pre-commit Checks**

Before starting release process, ensure all checks pass:

```bash
# Format code
find src lib gst include -name "*.[ch]" -exec clang-format -i {} \;

# Build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Test
pytest tests/

# SBOM
make sbom
```

**Step 2: Determine Next Version**

Follow **Semantic Versioning**:
- **MINOR** (X.Y.0): Breaking changes, new features requiring migration
  - Include migration details in CHANGELOG.md
  - Or create separate migration guide if complex
- **PATCH** (X.Y.Z): Non-breaking changes, bug fixes, docs
- **MAJOR** (X.0.0): Only on explicit user request

**Step 3: Update CHANGELOG.md**

Follow [Keep a Changelog](https://keepachangelog.com/) rules:
1. Move items from `## [Unreleased]` to new version section
2. Add: `## [X.Y.Z] - YYYY-MM-DD`
3. Categorize changes: Added, Changed, Deprecated, Removed, Fixed, Security
4. For breaking changes, include migration guide

**Step 4: Update ALL Version Files**

**CRITICAL**: Update these 6 files to the EXACT SAME VERSION:

| File | Format | Location |
|------|--------|----------|
| `Cargo.toml` | `version = "X.Y.Z"` | Line ~6, `[workspace.package]` |
| `include/videostream.h` | `#define VSL_VERSION "X.Y.Z"` | Line ~16 |
| `pyproject.toml` | `version = "X.Y.Z"` | Line ~7, `[project]` |
| `doc/conf.py` | `version = 'X.Y.Z'` | Line ~28 (single quotes!) |
| `debian/changelog` | `videostream (X.Y.Z-1) stable; urgency=medium` | New entry at TOP |
| `CHANGELOG.md` | `## [X.Y.Z] - YYYY-MM-DD` | After `[Unreleased]` |

**Verification checklist**:
```bash
# After editing, verify all match:
grep "version = \"1.5.0\"" Cargo.toml
grep "VSL_VERSION \"1.5.0\"" include/videostream.h
grep "version = \"1.5.0\"" pyproject.toml
grep "version = '1.5.0'" doc/conf.py
grep "videostream (1.5.0-1)" debian/changelog
grep "## \[1.5.0\]" CHANGELOG.md
```

**Step 5: Create Release Commit**

Commit message format: `Prepare Version X.Y.Z`

```bash
git add Cargo.toml include/videostream.h pyproject.toml \
        doc/conf.py debian/changelog CHANGELOG.md

git commit -m "Prepare Version 1.5.0

- Updated all version files to 1.5.0
- Finalized CHANGELOG.md with release notes
- See CHANGELOG.md for detailed changes"

git push origin main
```

**Step 6: Wait for CI/CD to Pass**

**DO NOT TAG until all checks green**:
- ✅ Build workflow
- ✅ Test workflow
- ✅ SBOM workflow
- ✅ Code quality checks

**Step 7: Create and Push Git Tag**

```bash
# Create annotated tag (NO 'v' prefix)
git tag -a -m "Version 1.5.0" 1.5.0

# Push tag
git push --tags
```

**Step 8: Monitor Release Workflow**

GitHub Actions automatically:
1. Builds release artifacts
2. Creates GitHub Release
3. Publishes to PyPI
4. Publishes to crates.io

Watch: `https://github.com/EdgeFirstAI/videostream/actions/workflows/release.yml`

### AI Assistant Guidelines

When asked to prepare a release:

1. **NEVER skip pre-commit checks** - format, build, test, SBOM must all pass
2. **ALWAYS update ALL 6 version files** - missing even one will break CI/CD
3. **VERIFY versions match** - use grep commands to confirm all files have same version
4. **FOLLOW semantic versioning** - breaking changes = MINOR (or MAJOR on request), non-breaking = PATCH
5. **UPDATE CHANGELOG FIRST** - move Unreleased items to new version section
6. **DO NOT TAG until CI passes** - wait for green checkmarks after release commit
7. **Use correct tag format** - `1.5.0` NOT `v1.5.0` (no v prefix)
8. **Read CONTRIBUTING.md** - has complete detailed release process

### Common Mistakes to Avoid

❌ **WRONG**: Skipping version file updates
```bash
# Only updating some files - CI WILL FAIL
vim Cargo.toml
git commit -m "Update version"  # Missing 5 other files!
```

✅ **CORRECT**: Update all 6 files
```bash
# Edit all 6 version files, then verify:
grep "1.5.0" Cargo.toml include/videostream.h pyproject.toml \
             doc/conf.py debian/changelog CHANGELOG.md
```

❌ **WRONG**: Tagging before CI passes
```bash
git push origin main
git tag -a -m "Version 1.5.0" 1.5.0  # TOO SOON!
git push --tags
# Now CI fails and you have to delete the tag
```

✅ **CORRECT**: Wait for CI, then tag
```bash
git push origin main
# Wait for GitHub Actions to show all green
# Then create tag
git tag -a -m "Version 1.5.0" 1.5.0
git push --tags
```

❌ **WRONG**: Forgetting doc/conf.py
```bash
# This file is easy to forget!
# CI will fail with version mismatch
```

✅ **CORRECT**: Use checklist from CONTRIBUTING.md
```bash
# Follow the 6-file checklist every time
```

### Troubleshooting

**Problem**: Forgot to update a version file

**Solution**:
```bash
# 1. Delete the tag
git tag -d 1.5.0
git push origin :refs/tags/1.5.0

# 2. Fix missing file
vim doc/conf.py

# 3. Amend commit
git add doc/conf.py
git commit --amend --no-edit

# 4. Force-push
git push origin main --force-with-lease

# 5. Recreate tag
git tag -a -m "Version 1.5.0" 1.5.0
git push origin 1.5.0
```

**Problem**: Version mismatch between files

**Solution**:
```bash
# Verify all versions
grep -r "1.5.0" Cargo.toml include/videostream.h pyproject.toml \
               doc/conf.py debian/changelog

# Fix any that don't match, then amend commit
```

---

## Working with AI Assistants

### For GitHub Copilot / Cursor

These tools provide inline suggestions. Ensure:
- Suggestions match project conventions (run linters after accepting)
- Complex logic has explanatory comments
- Generated tests have meaningful assertions
- Security best practices are followed

### For Claude Code / Chat-Based Assistants

When working with conversational AI:
1. **Provide context**: Share relevant files, error messages, and requirements
2. **Verify outputs**: Review generated code critically before committing
3. **Iterate**: Refine solutions through follow-up questions
4. **Document decisions**: Capture architectural choices and tradeoffs
5. **Test thoroughly**: AI-generated code needs human verification

### Common AI Assistant Pitfalls

- **Hallucinated APIs**: Verify library functions exist before using
- **Outdated patterns**: Check if suggestions match current best practices
- **Over-engineering**: Prefer simple solutions over complex ones
- **Missing edge cases**: Explicitly test boundary conditions
- **License violations**: AI may suggest code with incompatible licenses
- **Directory confusion with CMake**: NEVER use `cd build && cmake .. && make`. Always use modern workflow: `cmake -S . -B build && cmake --build build`
- **Forgetting to return from directories**: If you must `cd` somewhere, immediately return to project root after
- **Using `make` directly**: Use `cmake --build <dir>` instead - it works with all generators (Make, Ninja, etc.)

---

## Workflow Example

**Implementing a new feature:**

```bash
# 1. Create branch from develop
git checkout develop
git pull origin develop
git checkout -b feature/EDGEAI-123-add-image-preprocessing

# 2. Implement feature with tests
# - Write unit tests first (TDD)
# - Implement functionality
# - Add integration tests
# - Update documentation

# 3. Verify quality
make format    # Auto-format code
make lint      # Run linters
make test      # Run all tests
make coverage  # Check coverage meets threshold

# 4. Commit with proper message
git add .
git commit -m "EDGEAI-123: Add image preprocessing pipeline

- Implemented resize, normalize, and augment functions
- Added comprehensive unit and integration tests
- Documented API with usage examples
- Achieved 85% test coverage"

# 5. Push and create PR
git push -u origin feature/EDGEAI-123-add-image-preprocessing
# Create PR via GitHub/Bitbucket UI with template

# 6. Address review feedback
# - Make requested changes
# - Push additional commits
# - Respond to comments

# 7. Merge after approvals
# Maintainer merges via PR interface (squash or rebase)
```

---

## Getting Help

**For development questions:**
- Check project's `CONTRIBUTING.md` for setup instructions
- Review existing code for patterns and conventions
- Search GitHub Issues for similar problems
- Ask in GitHub Discussions (for public repos)

**For security concerns:**
- Email `support@au-zone.com` with subject "Security Vulnerability"
- Do not disclose vulnerabilities publicly

**For license questions:**
- Review license policy section above
- Check project's `LICENSE` file
- Contact technical leadership if unclear

**For contribution guidelines:**
- Read project's `CONTRIBUTING.md`
- Review recent merged PRs for examples
- Follow PR template and checklist

**For release process:**
- Read "Release Process (Maintainers)" section in `CONTRIBUTING.md`
- Understand cargo-release workflow and pre-release-replacements
- Never manually edit version numbers across multiple files

---

## Document Maintenance

**Project maintainers should:**
- Update [Project-Specific Guidelines](#project-specific-guidelines) with repository details
- Add technology stack, architecture patterns, and performance targets
- Document build/test/deployment procedures specific to the project
- Specify testing conventions (unit test location, framework choice, etc.)
- Keep examples and code snippets current
- Review and update annually or when major changes occur

**This template version**: 1.0 (November 2025)
**Organization**: Au-Zone Technologies
**License**: Apache-2.0 (for open source projects)

---

*This document helps AI assistants contribute effectively to Au-Zone projects while maintaining quality, security, and consistency. For questions or suggestions, contact `support@au-zone.com`.*
