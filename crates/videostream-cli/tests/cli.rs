// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

//! Integration tests for the videostream CLI
//!
//! These tests verify CLI commands work correctly end-to-end using the
//! assert_cmd crate pattern. Tests are serial and some require hardware.

use assert_cmd::Command;
use predicates::prelude::*;
use serial_test::serial;
use std::{
    env, fs,
    path::PathBuf,
    process::{Command as StdCommand, Stdio},
    thread,
    time::Duration,
};

/// Helper to create a Command for the videostream binary
/// Uses VIDEOSTREAM_BIN environment variable if set, otherwise uses cargo run
fn videostream_cmd() -> Command {
    if let Ok(bin_path) = env::var("VIDEOSTREAM_BIN") {
        Command::new(bin_path)
    } else {
        // Default: use cargo run (works in dev, CI build runners)
        let mut cmd = Command::new("cargo");
        cmd.args(["run", "--bin", "videostream", "--"]);
        cmd
    }
}

/// Get the path to the videostream binary for std::process::Command
fn videostream_bin() -> PathBuf {
    if let Ok(bin_path) = env::var("VIDEOSTREAM_BIN") {
        PathBuf::from(bin_path)
    } else {
        // In development/CI, we'll use the cargo-built binary
        // This is a fallback that won't work for subprocess spawning via std::process::Command
        panic!("VIDEOSTREAM_BIN environment variable must be set for tests that spawn subprocesses")
    }
}

/// Get the test data directory (target/testdata/videostream-cli)
/// Creates it if it doesn't exist
fn get_test_data_dir() -> PathBuf {
    let test_dir = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .parent()
        .unwrap()
        .parent()
        .unwrap()
        .join("target")
        .join("testdata")
        .join("videostream-cli");

    fs::create_dir_all(&test_dir).expect("Failed to create test data directory");
    test_dir
}

// =============================================================================
// Basic CLI Tests (No Hardware Required)
// =============================================================================

#[test]
fn test_cli_help() {
    videostream_cmd()
        .arg("--help")
        .assert()
        .success()
        .stdout(predicate::str::contains("VideoStream CLI"))
        .stdout(predicate::str::contains("stream"))
        .stdout(predicate::str::contains("record"))
        .stdout(predicate::str::contains("receive"))
        .stdout(predicate::str::contains("info"))
        .stdout(predicate::str::contains("convert"));
}

#[test]
fn test_cli_version() {
    videostream_cmd()
        .arg("--version")
        .assert()
        .success()
        .stdout(predicate::str::contains("videostream"));
}

#[test]
fn test_stream_help() {
    videostream_cmd()
        .arg("stream")
        .arg("--help")
        .assert()
        .success()
        .stdout(predicate::str::contains("Stream camera frames"))
        .stdout(predicate::str::contains("--device"))
        .stdout(predicate::str::contains("--resolution"))
        .stdout(predicate::str::contains("--encode"));
}

#[test]
fn test_record_help() {
    videostream_cmd()
        .arg("record")
        .arg("--help")
        .assert()
        .success()
        .stdout(predicate::str::contains("Record camera"))
        .stdout(predicate::str::contains("--device"))
        .stdout(predicate::str::contains("--frames"))
        .stdout(predicate::str::contains("--codec"));
}

#[test]
fn test_receive_help() {
    videostream_cmd()
        .arg("receive")
        .arg("--help")
        .assert()
        .success()
        .stdout(predicate::str::contains("Receive frames"))
        .stdout(predicate::str::contains("--frames"))
        .stdout(predicate::str::contains("--json"));
}

#[test]
fn test_info_help() {
    videostream_cmd()
        .arg("info")
        .arg("--help")
        .assert()
        .success()
        .stdout(predicate::str::contains("Display"))
        .stdout(predicate::str::contains("capabilities"));
}

#[test]
fn test_convert_help() {
    videostream_cmd()
        .arg("convert")
        .arg("--help")
        .assert()
        .success()
        .stdout(predicate::str::contains("Convert"))
        .stdout(predicate::str::contains("h264"));
}

// =============================================================================
// Info Command Tests (Runs on all platforms, gracefully handles missing hardware)
// =============================================================================

#[test]
fn test_info_basic() {
    videostream_cmd()
        .arg("info")
        .assert()
        .success()
        .stdout(predicate::str::contains("VideoStream"))
        .stdout(predicate::str::contains("System Information"));
}

#[test]
fn test_info_json_output() {
    videostream_cmd()
        .arg("info")
        .arg("--json")
        .assert()
        .success()
        .stdout(predicate::str::is_match(r#"\{"#).unwrap());
}

// =============================================================================
// Convert Command Tests (No Hardware Required)
// =============================================================================

#[test]
fn test_convert_missing_input() {
    videostream_cmd()
        .arg("convert")
        .arg("/nonexistent/input.h264")
        .arg("output.mp4")
        .assert()
        .failure()
        .code(1); // GeneralError
}

#[test]
fn test_convert_invalid_extension() {
    let test_dir = get_test_data_dir();
    let input = test_dir.join("test.txt");

    // Create a dummy file
    fs::write(&input, b"dummy").unwrap();

    videostream_cmd()
        .arg("convert")
        .arg(&input)
        .arg("output.mp4")
        .assert()
        .failure();

    fs::remove_file(&input).ok();
}

// =============================================================================
// Hardware Tests (Camera Required)
// =============================================================================

#[test]
#[ignore = "requires camera hardware (run with --include-ignored on hardware)"]
#[serial]
fn test_record_basic() {
    let test_dir = get_test_data_dir();
    let output_file = test_dir.join("test_record.h264");

    // Clean up previous test run
    fs::remove_file(&output_file).ok();

    videostream_cmd()
        .arg("record")
        .arg(&output_file)
        .arg("--frames")
        .arg("30") // Record just 30 frames
        .arg("--device")
        .arg("/dev/video3")
        .timeout(Duration::from_secs(10))
        .assert()
        .success()
        .stdout(predicate::str::contains("Recording complete"));

    // Verify file was created
    assert!(output_file.exists(), "Output file should exist");
    assert!(
        output_file.metadata().unwrap().len() > 0,
        "Output file should not be empty"
    );

    // Clean up
    fs::remove_file(&output_file).ok();
}

#[test]
#[ignore = "requires camera hardware (run with --include-ignored on hardware)"]
#[serial]
fn test_record_with_duration() {
    let test_dir = get_test_data_dir();
    let output_file = test_dir.join("test_record_duration.h264");

    fs::remove_file(&output_file).ok();

    videostream_cmd()
        .arg("record")
        .arg(&output_file)
        .arg("--duration")
        .arg("2") // Record for 2 seconds
        .arg("--device")
        .arg("/dev/video3")
        .timeout(Duration::from_secs(5))
        .assert()
        .success();

    assert!(output_file.exists());

    fs::remove_file(&output_file).ok();
}

#[test]
#[ignore = "requires camera hardware (run with --include-ignored on hardware)"]
#[serial]
fn test_record_and_convert_to_mp4() {
    let test_dir = get_test_data_dir();
    let h264_file = test_dir.join("test_convert.h264");
    let mp4_file = test_dir.join("test_convert.mp4");

    fs::remove_file(&h264_file).ok();
    fs::remove_file(&mp4_file).ok();

    // Record H.264
    videostream_cmd()
        .arg("record")
        .arg(&h264_file)
        .arg("--frames")
        .arg("60")
        .arg("--device")
        .arg("/dev/video3")
        .timeout(Duration::from_secs(10))
        .assert()
        .success();

    assert!(h264_file.exists(), "H.264 file should exist");

    // Convert to MP4
    videostream_cmd()
        .arg("convert")
        .arg(&h264_file)
        .arg(&mp4_file)
        .timeout(Duration::from_secs(5))
        .assert()
        .success()
        .stdout(predicate::str::contains("Conversion complete"));

    assert!(mp4_file.exists(), "MP4 file should exist");
    assert!(
        mp4_file.metadata().unwrap().len() > 0,
        "MP4 file should not be empty"
    );

    // Clean up
    fs::remove_file(&h264_file).ok();
    fs::remove_file(&mp4_file).ok();
}

// =============================================================================
// Stream/Receive Tests (Camera Required)
// =============================================================================

#[test]
#[ignore = "requires camera hardware (run with --include-ignored on hardware)"]
#[serial]
fn test_stream_and_receive() {
    let socket_path = "/tmp/videostream_test_stream_receive";

    // Clean up previous test run
    fs::remove_file(socket_path).ok();

    // Start stream in background (use std::process::Command for background process)
    let mut stream_process = StdCommand::new(videostream_bin())
        .arg("stream")
        .arg(socket_path)
        .arg("--device")
        .arg("/dev/video3")
        .arg("--frames")
        .arg("100")
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn()
        .expect("Failed to start stream command");

    // Give stream time to start
    thread::sleep(Duration::from_secs(2));

    // Receive frames
    let receive_result = videostream_cmd()
        .arg("receive")
        .arg(socket_path)
        .arg("--frames")
        .arg("30")
        .arg("--json")
        .timeout(Duration::from_secs(10))
        .assert()
        .success()
        .stdout(predicate::str::contains("frames_processed"));

    // Kill stream process
    stream_process.kill().ok();
    stream_process.wait().ok();

    // Verify JSON output contains expected metrics
    receive_result
        .stdout(predicate::str::contains("throughput_fps"))
        .stdout(predicate::str::contains("latency_p95_us"));

    // Clean up
    fs::remove_file(socket_path).ok();
}

#[test]
#[ignore = "requires camera and VPU hardware (run with --include-ignored on hardware)"]
#[serial]
fn test_stream_encoded_and_receive_decoded() {
    let socket_path = "/tmp/videostream_test_encoded";

    fs::remove_file(socket_path).ok();

    // Start encoded stream in background (use std::process::Command for background process)
    let mut stream_process = StdCommand::new(videostream_bin())
        .arg("stream")
        .arg(socket_path)
        .arg("--device")
        .arg("/dev/video3")
        .arg("--encode")
        .arg("--codec")
        .arg("h264")
        .arg("--frames")
        .arg("100")
        .stdout(Stdio::null())
        .stderr(Stdio::null())
        .spawn()
        .expect("Failed to start encoded stream");

    thread::sleep(Duration::from_secs(2));

    // Receive and decode frames
    let receive_result = videostream_cmd()
        .arg("receive")
        .arg(socket_path)
        .arg("--frames")
        .arg("30")
        .arg("--decode")
        .timeout(Duration::from_secs(10))
        .assert()
        .success();

    stream_process.kill().ok();
    stream_process.wait().ok();

    receive_result.stdout(predicate::str::contains("Received 30 frames"));

    fs::remove_file(socket_path).ok();
}

// =============================================================================
// Error Handling Tests
// =============================================================================

// Note: Testing null bytes in paths doesn't work with assert_cmd
// (it fails in the test harness itself, not in the CLI)
// This is a known limitation - actual CLI usage will properly reject null bytes

// Note: test_receive_nonexistent_socket removed because the receive command
// will wait indefinitely trying to connect to a nonexistent socket.
// This is expected behavior - the CLI waits for a host to become available.
// Proper testing of this would require signal handling or a very short timeout,
// which is better suited for hardware integration tests.

#[test]
#[ignore = "requires camera hardware to test error handling"]
fn test_record_invalid_device() {
    let test_dir = get_test_data_dir();
    let output_file = test_dir.join("test_invalid.h264");

    videostream_cmd()
        .arg("record")
        .arg(&output_file)
        .arg("--device")
        .arg("/dev/videoNONEXISTENT")
        .arg("--frames")
        .arg("10")
        .timeout(Duration::from_secs(5))
        .assert()
        .failure()
        .code(3); // CameraNotFound
}
