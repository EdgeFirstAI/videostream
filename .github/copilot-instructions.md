# AI Assistant Guidelines for VideoStream

EdgeFirst VideoStream Library provides video I/O for embedded Linux: V4L2 camera capture, hardware codec integration (H.264/H.265), and inter-process frame sharing via DmaBuf.

## Quick Reference

| Item | Value |
|------|-------|
| **Branch** | `feature/EDGEAI-###-description` or `bugfix/EDGEAI-###-description` |
| **Commit** | `EDGEAI-###: Brief description` (50-72 chars) |
| **PR** | main=2 approvals, develop=1. Link JIRA ticket, ensure CI passes |
| **License** | ✅ MIT/Apache/BSD | ⚠️ LGPL (dynamic only) | ❌ GPL/AGPL |
| **Coverage** | 70% minimum, 80%+ for core modules |
| **Output** | ❌ NEVER filter build/test/sbom with head/tail/grep. ✅ Use `tee` |

## Critical Rules

### Rule 1: NEVER Use `cd` Commands

Stay in project root. Use paths instead.

```bash
# ❌ NEVER
cd build && cmake ..

# ✅ ALWAYS
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)
```

### Rule 2: ALWAYS Use Python venv

```bash
# ❌ NEVER
pip install package
pytest tests/

# ✅ ALWAYS
venv/bin/pip install package
venv/bin/pytest tests/
```

### Rule 3: Source env.sh Before Tests

```bash
[ -f env.sh ] && source env.sh
venv/bin/pytest tests/

# Or use make test (handles venv + env.sh automatically)
make test
```

**Security:** env.sh contains ephemeral tokens (<48h). NEVER commit it.

### Rule 4: NEVER Filter Non-Trivial Command Output

```bash
# ❌ NEVER
make test | head -20
cmake --build build 2>&1 | grep -i error

# ✅ ALWAYS (show full output, optionally tee to file)
make test
cmake --build build 2>&1 | tee build-output.log
```

**Exception:** Simple parsing like `grep "VSL_VERSION" include/videostream.h` is fine.

## Technology Stack

- **C11** with `-Wall -Wextra -Werror`, clang-format, negative errno error handling
- **Rust** stable (1.70+), `cargo fmt`, `cargo clippy -- -D warnings`
- **Python** 3.8+, autopep8, always in venv
- **Build:** CMake 3.10+
- **Dependencies:** GStreamer 1.4+ (LGPL-2.1), GLib 2.0+, pthread
- **Platforms:** NXP i.MX 8M Plus, NXP i.MX 95, ARM64/ARMv7, x86_64
- **Acceleration:** G2D, Hantro VPU (i.MX 8M Plus), Wave6 VPU (i.MX 95), V4L2 DmaBuf

See [HARDWARE.md](../HARDWARE.md) for platform details and tested configurations.

## Build Commands

```bash
# Debug build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j$(nproc)

# Release build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Makefile targets
make test              # Run tests with coverage
make format            # Format C, Rust, Python
make lint              # cargo clippy
make sbom              # SBOM + license check
make verify-version    # Check version consistency
make pre-release       # All pre-release checks
make doc               # Sphinx + Doxygen HTML
make clean             # Clean artifacts
```

**CMake Options:** `ENABLE_GSTREAMER`, `ENABLE_DMABUF`, `ENABLE_G2D`, `ENABLE_VPU`, `ENABLE_V4L2_CODEC`, `ENABLE_HANTRO_CODEC`, `BUILD_TESTING`, `ENABLE_COVER` (all ON/OFF).

## Cross-Compilation for aarch64

### Using Generic Toolchain

```bash
cmake -S . -B build-aarch64 \
    -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux-gnu.cmake \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DENABLE_GSTREAMER=OFF

cmake --build build-aarch64 -j$(nproc)
```

### Using Yocto SDK

```bash
# i.MX 8M Plus
source /opt/fsl-imx-xwayland/5.4-zeus/environment-setup-aarch64-poky-linux
cmake -S . -B build-imx8mp -DCMAKE_BUILD_TYPE=Release
cmake --build build-imx8mp -j$(nproc)

# i.MX 95
source /opt/yocto-sdk-imx95-frdm-6.12.49-2.2.0/environment-setup-armv8a-poky-linux
cmake -S . -B build-imx95 -DCMAKE_BUILD_TYPE=Release
cmake --build build-imx95 -j$(nproc)
```

### Rust Cross-Compilation

```bash
CARGO_TARGET_AARCH64_UNKNOWN_LINUX_GNU_LINKER=aarch64-linux-gnu-gcc \
    LD_LIBRARY_PATH=$PWD/build-aarch64:$LD_LIBRARY_PATH \
    cargo build --release --target aarch64-unknown-linux-gnu
```

### Deploy to Target

```bash
scp build-aarch64/libvideostream.so.* root@<target>:/usr/lib/
scp target/aarch64-unknown-linux-gnu/release/videostream root@<target>:/usr/local/bin/
```

## On-Target Debugging and Profiling

Use the following skills for on-target work. Invoke the skill rather than repeating its knowledge inline:

| Task | Skill |
|------|-------|
| CPU profiling, PMU counters, IPC, cache misses | `linux-perf` |
| Kernel function tracing, scheduler analysis | `ftrace` |
| Dynamic kernel/userspace tracing | `ebpf-tracing` |
| Block I/O and storage latency | `blktrace` |
| Perfetto trace file generation | `perfetto` |
| GStreamer pipeline profiling | `gstreamer-profiling` |
| GStreamer plugin development | `gstreamer` |
| NNStreamer ML inference pipelines | `nnstreamer` |
| On-target test execution and coverage | `on-target-testing` |
| Embedded systems constraints | `embedded-systems` |

**Key environment for on-target debugging:**

```bash
# VideoStream debug logging
export VSL_DEBUG=1

# GStreamer debug logging
export GST_DEBUG=vslsink:5,vslsrc:5

# Rust logging
export RUST_LOG=debug

# Verify hardware
ls -la /dev/video* /dev/dma_heap/*
videostream info
```

**Performance targets (i.MX 8M Plus):**

| Metric | Target |
|--------|--------|
| Frame latency (DmaBuf) | <3ms |
| Frame latency (shm) | <10ms |
| Throughput (1080p, 3 clients) | 60fps |
| CPU usage (1080p30 distribution) | <2% |
| Memory per client | <100KB |

## Testing

```bash
# Recommended
make test

# Manual
[ -f env.sh ] && source env.sh
export VIDEOSTREAM_LIBRARY=./build/libvideostream.so.1
venv/bin/pytest tests/
```

See [TESTING.md](../TESTING.md) for the full testing strategy including on-target integration tests.

**Coverage:** 70% minimum project-wide, 80%+ for core modules (`lib/host.c`, `lib/client.c`, `lib/frame.c`).

## Release Process

**CRITICAL:** VideoStream requires version sync across 6 locations.

### Version Locations

| File | Format | Notes |
|------|--------|-------|
| `include/videostream.h` | `#define VSL_VERSION "X.Y.Z"` | Source of truth (CMakeLists.txt parses this) |
| `Cargo.toml` | `version = "X.Y.Z"` | Also update `videostream-sys` dependency version |
| `pyproject.toml` | `version = "X.Y.Z"` | |
| `doc/conf.py` | `version = 'X.Y.Z'` | Single quotes |
| `CHANGELOG.md` | `## [X.Y.Z] - YYYY-MM-DD` | Move items from `[Unreleased]` |
| `NOTICE` | `videostream-sys X.Y.Z` | videostream-sys version line |

`debian/changelog` is auto-generated from `CHANGELOG.md` by `debian/gen-changelog.py` — do **not** edit manually.

### Workflow

1. `make pre-release` — runs format, lint, verify-version, test, sbom
2. Update all 6 version locations
3. Update CHANGELOG.md (move Unreleased → new version)
4. `make verify-version` — confirm consistency
5. Commit: `git commit -a -m "Prepare Version X.Y.Z"`
6. Push and wait for CI (all green)
7. Tag: `git tag -a -m "Version X.Y.Z" vX.Y.Z && git push origin vX.Y.Z`

The `v` prefix on the tag is **required** to trigger the release workflow.

## Architecture Overview

**Pattern:** Host/Client IPC with event-driven frame sharing over UNIX sockets + DmaBuf FD passing.

**Key APIs:**
- `vsl_host_*`, `vsl_client_*`, `vsl_frame_*` — IPC frame sharing
- `vsl_camera_*` — V4L2 camera capture
- `vsl_encoder_*`, `vsl_decoder_*` — Hardware codec
- GStreamer plugins: `vslsink` (producer), `vslsrc` (consumer)
- CLI: `videostream stream|record|convert|devices|receive|info`

**Error handling:** Return negative errno values (e.g., -EINVAL). 0 for success.

See [ARCHITECTURE.md](../ARCHITECTURE.md) for diagrams and detailed design.

## License Policy

| Status | Licenses |
|--------|----------|
| ✅ Allowed | MIT, Apache-2.0, BSD-2/3, ISC, 0BSD, Unlicense, Public Domain |
| ⚠️ Conditional | LGPL (dynamic linking only), MPL-2.0 (external deps only) |
| ❌ Blocked | GPL (any version), AGPL, SSPL, Commons Clause |

Run `make sbom` before any release. CI/CD blocks GPL/AGPL violations automatically.

## Security

- NEVER hardcode credentials; use ephemeral tokens in env.sh
- NEVER commit env.sh
- DmaBuf FDs grant direct memory access — close immediately after use
- Report vulnerabilities: support@au-zone.com (Subject: "Security Vulnerability - VideoStream")

## Key Documentation

- [README.md](../README.md) — Overview and quick start
- [ARCHITECTURE.md](../ARCHITECTURE.md) — Internal design and diagrams
- [TESTING.md](../TESTING.md) — Testing strategy and on-target procedures
- [HARDWARE.md](../HARDWARE.md) — Platform compatibility and camera support
- [CONTRIBUTING.md](../CONTRIBUTING.md) — Development setup and contribution guide
- [CHANGELOG.md](../CHANGELOG.md) — Release history
