// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies
//
// VideoStream CLI - Device Discovery Tests
//
// TESTING LAYERS:
//
// Layer 1 (Unit Tests - No hardware required):
//   - Help text and command structure
//   - Invalid argument handling
//
// Layer 3 (Hardware Integration - Requires V4L2 devices):
//   - Device listing
//   - Filter flags (--cameras, --encoders, --decoders, --converters)
//   - JSON output
//   - Verbose mode
//
// RUN LAYER 1:
//   cargo test --test devices_cli
//
// RUN LAYER 3 (on hardware):
//   cargo test --test devices_cli -- --ignored --nocapture

use assert_cmd::Command;
use predicates::prelude::*;
use std::env;

/// Helper to create a Command for the videostream binary
fn videostream_cmd() -> Command {
    let mut cmd = if let Ok(bin_path) = env::var("VIDEOSTREAM_BIN") {
        Command::new(bin_path)
    } else {
        // Default: use cargo run
        let mut c = Command::new("cargo");
        c.args(["run", "--bin", "videostream", "--"]);
        c
    };

    // Pass LD_LIBRARY_PATH for library loading
    if let Ok(ld_library_path) = env::var("LD_LIBRARY_PATH") {
        cmd.env("LD_LIBRARY_PATH", ld_library_path);
    }

    cmd
}

// =============================================================================
// Layer 1: Basic Command Tests (No Hardware Required)
// =============================================================================

#[test]
fn test_devices_help() {
    videostream_cmd()
        .args(["devices", "--help"])
        .assert()
        .success()
        .stdout(predicate::str::contains("V4L2"))
        .stdout(predicate::str::contains("--cameras"))
        .stdout(predicate::str::contains("--encoders"))
        .stdout(predicate::str::contains("--decoders"))
        .stdout(predicate::str::contains("--converters"))
        .stdout(predicate::str::contains("--json"))
        .stdout(predicate::str::contains("--verbose"));
}

#[test]
fn test_devices_help_short() {
    videostream_cmd()
        .args(["devices", "-h"])
        .assert()
        .success()
        .stdout(predicate::str::contains("V4L2"));
}

// =============================================================================
// Layer 3: Hardware Tests (Requires V4L2 Devices)
// =============================================================================

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_devices_list() {
    videostream_cmd()
        .arg("devices")
        .assert()
        .success()
        .stdout(predicate::str::contains("V4L2 Devices"));
}

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_devices_cameras_filter() {
    let output = videostream_cmd()
        .args(["devices", "--cameras"])
        .assert()
        .success();

    // If there are cameras, output should indicate "Camera" type
    // If no cameras, should still succeed with appropriate message
    output.stdout(
        predicate::str::contains("Camera").or(predicate::str::contains("No devices found")),
    );
}

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_devices_encoders_filter() {
    let output = videostream_cmd()
        .args(["devices", "--encoders"])
        .assert()
        .success();

    output.stdout(
        predicate::str::contains("Encoder").or(predicate::str::contains("No devices found")),
    );
}

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_devices_decoders_filter() {
    let output = videostream_cmd()
        .args(["devices", "--decoders"])
        .assert()
        .success();

    output.stdout(
        predicate::str::contains("Decoder").or(predicate::str::contains("No devices found")),
    );
}

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_devices_converters_filter() {
    let output = videostream_cmd()
        .args(["devices", "--converters"])
        .assert()
        .success();

    // Converters include ISP and M2M devices
    output.stdout(
        predicate::str::contains("ISP")
            .or(predicate::str::contains("M2M"))
            .or(predicate::str::contains("No devices found")),
    );
}

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_devices_json_output() {
    videostream_cmd()
        .args(["devices", "--json"])
        .assert()
        .success()
        // JSON output should start with array bracket or object
        .stdout(predicate::str::starts_with("[").or(predicate::str::starts_with("{")));
}

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_devices_json_with_filter() {
    videostream_cmd()
        .args(["devices", "--cameras", "--json"])
        .assert()
        .success()
        .stdout(predicate::str::starts_with("[").or(predicate::str::starts_with("{")));
}

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_devices_verbose() {
    videostream_cmd()
        .args(["devices", "--verbose"])
        .assert()
        .success()
        // Verbose should show additional details like memory capabilities
        .stdout(predicate::str::contains("Memory").or(predicate::str::contains("Format")));
}

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_devices_all_ungrouped() {
    videostream_cmd()
        .args(["devices", "--all"])
        .assert()
        .success()
        // --all should show individual devices instead of grouped
        .stdout(predicate::str::contains("/dev/video"));
}

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_devices_multiple_filters() {
    // Multiple filters should work together (OR logic)
    videostream_cmd()
        .args(["devices", "--cameras", "--encoders"])
        .assert()
        .success();
}

#[test]
#[ignore = "requires V4L2 devices (run with --ignored on hardware)"]
fn test_devices_json_structure() {
    let output = videostream_cmd()
        .args(["devices", "--json"])
        .output()
        .expect("Command should execute");

    assert!(output.status.success(), "Command should succeed");

    let stdout = String::from_utf8_lossy(&output.stdout);
    // Validate JSON parses correctly
    let parsed: Result<serde_json::Value, _> = serde_json::from_str(&stdout);
    assert!(parsed.is_ok(), "Output should be valid JSON: {}", stdout);
}
