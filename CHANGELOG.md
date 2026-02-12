# Changelog

All notable changes to the EdgeFirst VideoStream Library will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

## [2.2.1] - 2026-02-12

### Fixed

- Fixed Rust CLI binary (`videostream`) missing from ZIP packages due to CMake
  configure-time `if(EXISTS)` check evaluating before cargo build completes
- Eliminated redundant build.yml trigger on tag push; release workflow now waits
  for CI artifacts instead of rebuilding
- Pinned Rust toolchain to 1.93.1 via `rust-toolchain.toml` to prevent profraw
  incompatibility between build and coverage processing jobs

## [2.2.0] - 2026-02-12

### Added

- **V4L2 Device Discovery API**: New API for automatic detection of cameras, encoders, and decoders
  by capability. Replaces hardcoded device paths with runtime detection.
  - `vsl_v4l2_enumerate()` - List all V4L2 devices with capabilities and formats
  - `vsl_v4l2_enumerate_type(mask)` - Filter devices by type bitmask
  - `vsl_v4l2_find_encoder(codec)` - Find encoder by output codec (H.264/HEVC)
  - `vsl_v4l2_find_decoder(codec)` - Find decoder by input codec
  - `vsl_v4l2_find_camera(format)` - Find camera by pixel format (NV12/YUYV/etc.)
  - `vsl_v4l2_find_camera_with_resolution()` - Find camera by format and resolution
  - `vsl_v4l2_device_enum_formats()` - Enumerate formats for a device
  - `vsl_v4l2_enum_resolutions()` - Get supported resolutions for a format
  - `vsl_v4l2_device_supports_format()` - Check format support
  - `vsl_v4l2_device_type_name()` - Get human-readable device type name
  - `vsl_v4l2_is_compressed_format()` - Check if fourcc is compressed
  - `vsl_v4l2_fourcc_to_string()` - Convert fourcc to printable string
  - Real format enumeration via VIDIOC_ENUM_FMT instead of hardcoded lists
  - Resolution enumeration via VIDIOC_ENUM_FRAMESIZES
  - Memory capability detection (MMAP/USERPTR/DMABUF)

- **DMA Heap USERPTR Allocation**: `vsl_v4l2_alloc_userptr()` allocates USERPTR buffers from
  DMA heap for zero-copy with devices that don't support DMABUF export.
  - `vsl_v4l2_free_userptr()` - Free USERPTR buffer and close FD

- **CLI `devices` Command**: New command to enumerate and filter V4L2 devices:
  - `videostream devices` - List all devices grouped by hardware unit
  - `--cameras` / `--encoders` / `--decoders` / `--converters` - Filter by type
  - `--all` - Show all device nodes (disable grouping)
  - `--verbose` - Show detailed format information
  - `--json` - Machine-readable JSON output
  - Smart grouping by `bus_info` to deduplicate multi-channel hardware

- **Rust V4L2 Module**: Safe Rust wrappers in `videostream::v4l2`:
  - `DeviceEnumerator` - Static methods for device discovery
  - `Device` - Device descriptor with capabilities and formats
  - `Format` / `Resolution` - Format and resolution types
  - `DeviceType` / `MemoryType` / `MemoryCapabilities` - Capability types
  - Comprehensive rustdoc with examples

- **vsl-v4l2-info Utility**: New test utility to demonstrate the discovery API.

### Changed

- **Encoder/Decoder Auto-Detection**: `vsl_encoder_create()` and `vsl_decoder_create()` now
  auto-detect the correct device path by codec capability. Environment variable overrides
  (`VSL_V4L2_ENCODER_DEV`, `VSL_V4L2_DECODER_DEV`) still work for explicit device selection.

- **CLI info Command**: `videostream info --v4l2` now uses the native V4L2 discovery API
  to show real device capabilities and formats instead of sysfs-based heuristics.

- **SOVERSION Bump**: Library SOVERSION changed from 1 to 2 (`libvideostream.so.2`).
  Applications linking against VideoStream must be rebuilt.

- **Rust Client API**: Removed unsound public `Client::release()` method that could trigger
  use-after-free in safe code. Resource cleanup is now handled exclusively by `Drop`. (Fixes #8)

- **Safe String Functions**: Replaced all `strncpy` calls with new `vsl_strcpy_s` and
  `vsl_strncpy_s` functions following C11 Annex K conventions, ensuring null-termination
  and SonarCloud compliance.

### Fixed

- **vslsink DMA Buffer Pool**: Fixed DMA heap memory exhaustion when vslsink copies system memory
  to dmabuf. Previously, a new DMA buffer was allocated for every frame, exhausting the DMA heap
  after ~275 frames (~850MB). Now uses a pre-allocated pool of buffers (default: 8) that are
  reused via round-robin allocation. Added `pool-size` property to configure pool size.

- **vslsink Zero-Copy for All Sources**: Implemented `VslDmaBufBufferPool`, a custom
  `GstVideoBufferPool` subclass that enables zero-copy IPC for ALL upstream sources including
  those that cannot allocate DmaBuf memory themselves (e.g., videotestsrc). The pool:
  - Allocates buffers from `/dev/dma_heap/linux,cma` or `/dev/dma_heap/system`
  - Proposes the pool in `propose_allocation()` so upstream writes directly to shareable memory
  - Uses `gst_dmabuf_allocator_alloc()` to wrap FDs for GStreamer integration
  - Tracks buffer lifecycle with round-robin allocation and automatic recycling

- **Host Socket Error Messages**: Suppressed spurious "Socket operation on non-socket" error
  messages when clients disconnect. Added `ENOTSOCK`, `EBADF`, and `EPIPE` to the list of
  silently handled disconnect errors.

- **vslsink File Descriptor Leak**: Fixed FD leak in cleanup callbacks. When `vsl_frame_attach()`
  duplicates the buffer FD, the dup'd FD was not being closed when a cleanup callback existed
  (since `vsl_frame_unalloc()` skips closing in that case). Both `frame_cleanup()` and
  `dmabuf_pool_cleanup()` now close the dup'd handle.

### Documentation

- Comprehensive Doxygen documentation for all V4L2 API with `@since 2.2` annotations
- `VSL_AVAILABLE_SINCE_2_2` macro decorators on all new functions
- Full Rust rustdoc with examples for docs.rs publication
- V4L2 types moved from internal `lib/v4l2_device.h` to public `include/videostream.h`
- Migrated AI assistant guidelines to `.github/copilot-instructions.md` with skill references
- Merged i.MX 95 hardware documentation into `HARDWARE.md`
- Hash-pinned all GitHub Actions for SPS v2.1 compliance
- Added i.MX 95 to issue template platform dropdowns
- Consolidated duplicate aarch64 toolchain files
- Fixed CONTRIBUTING.md release process to reflect auto-generated `debian/changelog`
- Updated SECURITY.md version support table for 2.x

---

## [2.1.4] - 2026-01-05

### Fixed

- **Release Workflow**: Complete release with trusted publisher configured for `videostream-cli`
  crate on crates.io (2.1.3 failed CLI publishing due to missing trusted publisher)

---

## [2.1.3] - 2026-01-04

### Fixed

- **Release Workflow**: First complete release with all artifacts after fixing crates.io
  workspace publishing and manually registering `videostream-cli` crate

Note: 2.1.1 and 2.1.2 had partial release failures. This is the first complete 2.1.x release.

---

## [2.1.2] - 2026-01-04

### Fixed

- **Cargo Workspace Publishing**: Added `videostream` crate to workspace dependencies with version
  for crates.io publishing (fixes `cargo publish --workspace` failure)

Note: 2.1.1 was partially released (PyPI only) due to crates.io publishing failure.

---

## [2.1.1] - 2026-01-04

### Fixed

- **NOTICE Version** (EDGEAI-993): Fixed NOTICE file version tracking and added verify-version check
- **CI Doctests**: Added Rust doctests to test.yml (nextest doesn't run doctests) and fixed
  non-exhaustive match pattern in decoder.rs doctest

---

## [2.1.0] - 2026-01-04

### Added

- **VideoStream CLI**: Modern Rust CLI application with 5 commands (DE-993)
  - `videostream stream`: Camera → VSL socket streaming (raw/encoded)
  - `videostream record`: Camera → H.264/H.265 Annex-B bitstream recording
  - `videostream receive`: VSL socket client with latency metrics (p50/p95/p99)
  - `videostream info`: Hardware capabilities enumeration (camera, VPU encoder/decoder)
  - `videostream convert`: H.264/H.265 Annex-B → MP4 conversion with SPS parser
  - CLI distributed in `videostream-tools` package alongside legacy `vsl-camhost`
  - Comprehensive integration tests (11 unit + 6 hardware tests)
  - JSON output support for programmatic metrics collection
- **V4L2 Codec Backend** (EDGEAI-982): Hardware encoder/decoder via V4L2 mem2mem API
  - Alternative to Hantro libcodec.so for H.264/HEVC encoding and decoding
  - Supports standard V4L2 stateful codec devices
- **VPU Decoder DMA Heap Support**: DMA buffer allocation for zero-copy decode pipelines
- **Rust API Improvements** (EDGEAI-1001):
  - Added `Rect` type for crop rectangles with `x`, `y`, `width`, `height` fields
  - Added `Debug` trait implementations for `Host`, `Client`, `Frame`, `Encoder`, `Decoder`
  - Added common traits (`Clone`, `Copy`, `PartialEq`, `Eq`, `Hash`) to enums
  - Added `#[non_exhaustive]` attribute to public enums for future compatibility
- **Rust Unit Tests** (EDGEAI-995): Comprehensive test coverage for all Rust modules
  - Tests for `SymbolNotFound` and `HardwareNotAvailable` error handling
  - Tests for library error handling edge cases

### Changed

- **Rust API** (EDGEAI-1001): Removed `get_` prefix from getters per Rust API Guidelines
  - `frame.get_width()` → `frame.width()`
  - `frame.get_height()` → `frame.height()`
  - Similar changes for all accessor methods
- **Rust API**: Replaced `bool` parameter with `Reconnect` enum for type safety
  - `Client::connect(path, true)` → `Client::connect(path, Reconnect::Yes)`

### Fixed

- **VPU Decode Latency**: Eliminated 200ms decode delay by using KICK mode polling
- **IPC Deadlock**: Fixed critical deadlock and memory corruption bugs in host/client IPC
- **Stack Smashing (aarch64)**: Multiple fixes for stack corruption on ARM64 platforms
  - Fixed uninitialized `pollfd.revents` in `vsl_frame_wait`
  - Initialized all VPU structures in decoder
  - Used heap allocation for KICK mode buffer
  - Replaced designated initializers with memset
- **Socket EAGAIN**: Handle EAGAIN in socket operations to prevent client disconnection
- **Memory Leaks**: Resolved memory leaks, hot path allocation, and logic bugs in VPU code
- **V4L2**: Fixed VIDIOC_DQBUF bug by removing incorrect `buf.index = 0` initialization
- **IPC Memory Allocation**: Corrected sizeof in memory allocation for sockets and frames
- **Camera**: Enhanced buffer release logging with trace/warn diagnostics
- **CLI**: Resolved argument short-option conflicts (-d, -f flags)
- **Rust Decoder**: Fixed `create_ex` to use fourcc codec value correctly
- **SonarCloud**: Addressed blocker bugs, security vulnerabilities, and critical code issues
- **Code Quality**: Reduced cognitive complexity across encoder, decoder, client, and host modules

---

## [2.0.0] - 2025-11-27

### Changed

- **BREAKING**: Python package namespace changed from `deepview.videostream` to `videostream`
  - Update imports: `from deepview.videostream import Frame` → `from videostream import Frame`
  - Update imports: `import deepview.videostream as vsl` → `import videostream as vsl`

---

## [1.5.6] - 2025-11-25

### Fixed

- GitHub Actions: Fixed release workflow to properly include PDF documentation in release assets
- GitHub Actions: Improved artifact detection and copying with better error messages

### Added

- Rust crates: Added README.md for `videostream` crate with Rust-specific quick start guide
- Rust crates: Added README.md for `videostream-sys` crate with FFI binding documentation
- Documentation: Added comprehensive examples and usage guides for Rust developers

---

## [1.5.5] - 2025-11-25

### Changed

- Makefile: Default target now displays help instead of building documentation
- Makefile: Added `make doc` target for building PDF documentation (README.pdf, DESIGN.pdf)
- Makefile: Added `make lint` and `make clean` targets for code quality and cleanup
- Documentation: Updated all build instructions to use modern CMake workflow (cmake -S . -B build)
- AGENTS.md: Aligned with SPS v2.0 standards, reduced from 1,280 to 424 lines
- AGENTS.md: Removed duplicated organization-wide rules, added reference to Au-Zone SPS

---

## [1.5.4] - 2025-11-21

### Changed

- Internal documentation improvements to AGENTS.md (AI assistant guidelines)
- No user-facing changes - release verifies updated release process

---

## [1.5.3] - 2025-11-20

### Added

- Makefile automation targets for release process
  - `make format` - Formats C, Rust, and Python code
  - `make test` - Runs tests with proper environment setup
  - `make sbom` - Generates SBOM and verifies license compliance
  - `make verify-version` - Checks all version files are synchronized
  - `make pre-release` - Runs complete pre-release checklist

### Changed

- Improved crates.io publishing using official Trusted Publishers via rust-lang/crates-io-auth-action
- Updated to Rust 1.90+ cargo publish --workspace for simplified multi-crate releases
- Enhanced release process documentation with common pitfalls and solutions
- Expanded AI assistant guidelines with concrete examples from 1.5.2 release learnings

### Fixed

- Release workflow now properly uses crates.io Trusted Publishers (OIDC)
- Removed manual dependency ordering and wait times in crate publishing

---

## [1.5.2] - 2025-11-20

### Fixed

- Documentation: Updated doc/conf.py version to match other version files

---

## [1.5.1] - 2025-11-20

### Fixed

- Release process: Ensured all version files are synchronized correctly

---

## [1.5.0] - 2025-11-19

### Added (Rust bindings only)

- **Dynamic Library Loading**: Rust crates now use runtime dynamic loading via `libloading` crate
  - Rust projects can build on systems without `libvideostream.so` installed (compile-time only)
  - Library loaded lazily at first API call, not at program startup
  - New error variant: `Error::LibraryNotLoaded` when library cannot be found

### Changed (Rust bindings only)

- **BREAKING: Error type renamed**: `VSLError` → `Error`
  - Follows Rust naming conventions (matches `std::io::Error`)
  - **Migration**: Replace all `VSLError` references with `Error` in your code

- **BREAKING: Error enum refactored with specific variants**:
  - Removed catch-all `Other(String)` variant
  - Added specific variants for each error type:
    - `Error::LibraryNotLoaded(libloading::Error)` - library loading failures
    - `Error::Io(io::Error)` - I/O and system call errors
    - `Error::Utf8(Utf8Error)` - UTF-8 conversion errors
    - `Error::CString(NulError)` - CString creation errors (null byte in string)
    - `Error::TryFromInt(TryFromIntError)` - integer conversion errors
    - `Error::NullPointer` - null pointer from C library
  - Implemented `From` trait for automatic error conversion (enables `?` operator)
  - **Migration**: Error matching code must use new variant names

- **BREAKING: All Rust APIs now return `Result` types**:

  ```rust
  // Before (v1.3.x)
  let frame = Frame::new(640, 480, 0, "RGB3");  // Could panic
  let width = frame.width();                     // Returns i32
  
  // After (v1.5.0)
  let frame = Frame::new(640, 480, 0, "RGB3")?;  // Returns Result<Frame, Error>
  let width = frame.width()?;                     // Returns Result<i32, Error>
  ```

- **Function return type changes** (all now return `Result<T, Error>`):
  - `Frame::new()`, `width()`, `height()`, `size()`, `stride()`, `fourcc()`, `serial()`, `timestamp()`, `duration()`, `pts()`, `dts()`, `expires()`, `handle()`, `paddr()`, `path()`, `munmap()`
  - `Client::release()`, `disconnect()`, `set_timeout()`, `path()`
  - `Encoder::create()`, `frame()`
  - `Decoder::create()`, `width()`, `height()`, `crop()`
  - `CameraBuffer::length()`, `timestamp()`
  - `videostream::version()`, `timestamp()`

### Fixed (Rust bindings only)

- Drop implementations now handle library initialization failures gracefully
  - `Frame`, `Client`, `Host`, `Encoder`, `Decoder`, `CameraReader`, `CameraBuffer`
  - Errors during cleanup are silently ignored (cannot propagate from `Drop` trait)

### Removed (Rust bindings only)

- **BREAKING**: Removed `panic!()` calls from public API
  - All error conditions now return `Result` types
  - Callers must handle errors via `?` operator or `.unwrap()`

---

## Rust Migration Guide: v1.3.x → v1.5.0

### Update Error Type References

```rust
// Before
use videostream::VSLError;
fn my_function() -> Result<(), VSLError> { ... }

// After
use videostream::Error;
fn my_function() -> Result<(), Error> { ... }
```

### Update Error Matching

```rust
// Before
match some_operation() {
    Err(VSLError::Other(msg)) => eprintln!("Error: {}", msg),
    ...
}

// After  
match some_operation() {
    Err(Error::LibraryNotLoaded(e)) => eprintln!("Library not found: {}", e),
    Err(Error::Io(e)) => eprintln!("I/O error: {}", e),
    Err(Error::NullPointer) => eprintln!("Null pointer from C library"),
    ...
}
```

### Handle Result Types

```rust
// Before (could panic)
let frame = Frame::new(640, 480, 0, "RGB3");
let width = frame.width();

// After - propagate errors with ?
let frame = Frame::new(640, 480, 0, "RGB3")?;
let width = frame.width()?;

// Or explicitly unwrap (reproduces old panic behavior)
let frame = Frame::new(640, 480, 0, "RGB3").unwrap();
let width = frame.width().unwrap();
```

### Building Without C Library

```bash
# Now works at compile-time (tests require library at runtime)
cargo build --workspace

# To run tests, install C library first:
cd build && cmake .. && make && sudo make install
cargo test --workspace
```

---

## [1.4.0] - 2024-09-25

### Added (C Library)

- **VPU Decoder API**: Hardware-accelerated H.264/HEVC decoding via Hantro VPU
  - `vsl_decoder_create()` - Create decoder instance with target output format and FPS
  - `vsl_decode_frame()` - Decode compressed frame to VSLFrame with return codes
  - `vsl_decoder_width()`, `vsl_decoder_height()` - Query decoded frame dimensions
  - `vsl_decoder_crop()` - Get crop rectangle for decoded frames
  - `vsl_decoder_release()` - Release decoder resources
  - `VSLDecoderCodec` enum for H.264 (AVC) and HEVC codec selection
  - `VSLDecoderRetCode` enum: `VSL_DEC_SUCCESS`, `VSL_DEC_ERR`, `VSL_DEC_INIT_INFO`, `VSL_DEC_FRAME_DEC`

### Changed (C Library)

- Enhanced build system with improved CMake configuration
- Improved CI/CD pipeline for multi-platform testing
- Performance optimizations for embedded platforms

---

## [1.3.0] - 2024

### Added (C Library)

- **VPU Encoder API**: Hardware-accelerated H.264/HEVC encoding via Hantro VC8000e
  - `vsl_encoder_create()` - Create encoder instance with profile, codec, and FPS
  - `vsl_encode_frame()` - Encode VSLFrame to compressed format with optional crop
  - `vsl_encoder_new_output_frame()` - Allocate output frame for encoded data
  - `vsl_encoder_release()` - Release encoder resources
  - `VSLEncoderProfile` enum for quality/bitrate presets (AUTO, 5000_KBPS, 25000_KBPS, 50000_KBPS, 100000_KBPS)

### Changed (C Library)

- Various performance improvements and bug fixes

---

## [1.3.14] - 2024

### C Library

See git commit history for C library changes in versions 1.3.1 through 1.3.14.
Rust bindings were introduced in version 1.4.0.

---

## Support

**For latest releases:**

- GitHub Releases: https://github.com/EdgeFirstAI/videostream/releases
- Documentation: https://doc.edgefirst.ai/

**For security updates:**

- See [SECURITY.md](SECURITY.md) for supported versions and reporting process

**For commercial support:**

- Contact: support@au-zone.com

---

*Last Updated: 2026-01-04*
