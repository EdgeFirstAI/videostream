# Changelog

All notable changes to the EdgeFirst VideoStream Library will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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

*Last Updated: 2025-11-19*
