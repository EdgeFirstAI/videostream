// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

mod convert;
mod devices;
mod error;
mod info;
mod metrics;
mod receive;
mod record;
mod stream;
mod utils;

use clap::{Parser, Subcommand};
use error::result_to_exit_code;
use std::process::ExitCode;

/// VideoStream CLI - Camera streaming, encoding, and metrics tool
#[derive(Parser)]
#[command(name = "videostream")]
#[command(version)]
#[command(about = "VideoStream CLI - Camera streaming, encoding, and metrics tool")]
#[command(long_about = None)]
#[command(propagate_version = true)]
struct Cli {
    /// Enable verbose logging (use RUST_LOG=debug for more)
    #[arg(short, long, global = true)]
    verbose: bool,

    /// Suppress non-error output
    #[arg(short, long, global = true)]
    quiet: bool,

    /// Output metrics in JSON format
    #[arg(long, global = true)]
    json: bool,

    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Stream camera frames to a VSL socket
    Stream(stream::Args),

    /// Record camera to raw H.264/H.265 bitstream (power-loss resilient)
    Record(record::Args),

    /// Convert raw H.264/H.265 bitstream to MP4 container
    Convert(convert::Args),

    /// Receive frames from a VSL socket and measure performance
    Receive(receive::Args),

    /// Display camera and system hardware capabilities
    Info(info::Args),

    /// List V4L2 devices with filtering and grouping
    Devices(devices::Args),
}

fn main() -> ExitCode {
    let cli = Cli::parse();

    // Initialize logging based on verbosity
    init_logging(cli.verbose, cli.quiet);

    // Execute the subcommand and convert result to exit code
    let result = match cli.command {
        Commands::Stream(args) => stream::execute(args, cli.json),
        Commands::Record(args) => record::execute(args, cli.json),
        Commands::Convert(args) => convert::execute(args, cli.json),
        Commands::Receive(args) => receive::execute(args, cli.json),
        Commands::Info(args) => info::execute(args, cli.json),
        Commands::Devices(args) => devices::execute(args, cli.json),
    };

    result_to_exit_code(result)
}

/// Initialize env_logger based on verbosity flags
fn init_logging(verbose: bool, quiet: bool) {
    // Determine log level from flags or RUST_LOG environment variable
    let env = env_logger::Env::default();

    let env = if quiet {
        // Quiet mode: only show errors
        env.default_filter_or("error")
    } else if verbose {
        // Verbose mode: show debug messages
        env.default_filter_or("debug")
    } else {
        // Default: show info and above
        env.default_filter_or("info")
    };

    env_logger::Builder::from_env(env)
        .format_timestamp(None) // Disable timestamps for cleaner CLI output
        .format_target(false) // Disable target (module path) for cleaner output
        .init();

    log::debug!("Logging initialized");
}
