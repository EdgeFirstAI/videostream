// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use std::fmt;
use std::process::ExitCode;

/// CLI-specific error type with exit code mapping
#[derive(Debug)]
pub enum CliError {
    /// Invalid command-line arguments
    InvalidArgs(String),
    /// Camera device not found or inaccessible
    CameraNotFound(String),
    /// Encoder/decoder hardware not available
    EncoderUnavailable(String),
    /// Socket error (connection, binding, etc.)
    SocketError(String),
    /// Operation timed out
    Timeout(String),
    /// General error from VideoStream library
    General(String),
}

impl fmt::Display for CliError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            CliError::InvalidArgs(msg) => write!(f, "Invalid arguments: {}", msg),
            CliError::CameraNotFound(msg) => write!(f, "Camera not found: {}", msg),
            CliError::EncoderUnavailable(msg) => {
                write!(f, "Encoder/decoder unavailable: {}", msg)
            }
            CliError::SocketError(msg) => write!(f, "Socket error: {}", msg),
            CliError::Timeout(msg) => write!(f, "Timeout: {}", msg),
            CliError::General(msg) => write!(f, "Error: {}", msg),
        }
    }
}

impl std::error::Error for CliError {}

impl CliError {
    /// Get the exit code for this error
    pub fn exit_code(&self) -> ExitCode {
        match self {
            CliError::InvalidArgs(_) => ExitCode::from(2),
            CliError::CameraNotFound(_) => ExitCode::from(3),
            CliError::EncoderUnavailable(_) => ExitCode::from(4),
            CliError::SocketError(_) => ExitCode::from(5),
            CliError::Timeout(_) => ExitCode::from(6),
            CliError::General(_) => ExitCode::from(1),
        }
    }
}

/// Map videostream::Error to CliError with appropriate exit codes
impl From<videostream::Error> for CliError {
    fn from(err: videostream::Error) -> Self {
        use videostream::Error;

        match err {
            // Symbol not found or hardware not available -> EncoderUnavailable
            Error::SymbolNotFound(sym) => {
                CliError::EncoderUnavailable(format!("Symbol not found: {}", sym))
            }
            Error::HardwareNotAvailable(hw) => {
                CliError::EncoderUnavailable(format!("Hardware not available: {}", hw))
            }

            // IO errors - try to map to specific error types
            Error::Io(io_err) => match io_err.kind() {
                std::io::ErrorKind::NotFound => {
                    CliError::CameraNotFound(format!("Device not found: {}", io_err))
                }
                std::io::ErrorKind::TimedOut => {
                    CliError::Timeout(format!("Operation timed out: {}", io_err))
                }
                std::io::ErrorKind::ConnectionRefused
                | std::io::ErrorKind::ConnectionReset
                | std::io::ErrorKind::ConnectionAborted
                | std::io::ErrorKind::BrokenPipe => {
                    CliError::SocketError(format!("Socket error: {}", io_err))
                }
                std::io::ErrorKind::PermissionDenied => {
                    CliError::CameraNotFound(format!("Permission denied: {}", io_err))
                }
                _ => CliError::General(format!("I/O error: {}", io_err)),
            },

            // Library loading errors
            Error::LibraryNotLoaded(lib_err) => {
                CliError::General(format!("Failed to load library: {}", lib_err))
            }

            // String/encoding errors
            Error::Utf8(utf8_err) => CliError::General(format!("UTF-8 error: {}", utf8_err)),
            Error::CString(cstr_err) => CliError::General(format!("C string error: {}", cstr_err)),

            // Integer conversion errors
            Error::TryFromInt(int_err) => {
                CliError::General(format!("Integer conversion error: {}", int_err))
            }

            // Null pointer errors
            Error::NullPointer => CliError::General("Unexpected null pointer".to_string()),

            // Catch-all for any future error variants (non-exhaustive enum)
            _ => CliError::General(format!("Unexpected error: {:?}", err)),
        }
    }
}

/// Helper function to convert result to exit code
pub fn result_to_exit_code<T>(result: Result<T, CliError>) -> ExitCode {
    match result {
        Ok(_) => ExitCode::SUCCESS,
        Err(e) => {
            eprintln!("{}", e);
            e.exit_code()
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_exit_codes() {
        assert_eq!(
            CliError::InvalidArgs("test".into()).exit_code(),
            ExitCode::from(2)
        );
        assert_eq!(
            CliError::CameraNotFound("test".into()).exit_code(),
            ExitCode::from(3)
        );
        assert_eq!(
            CliError::EncoderUnavailable("test".into()).exit_code(),
            ExitCode::from(4)
        );
        assert_eq!(
            CliError::SocketError("test".into()).exit_code(),
            ExitCode::from(5)
        );
        assert_eq!(
            CliError::Timeout("test".into()).exit_code(),
            ExitCode::from(6)
        );
        assert_eq!(
            CliError::General("test".into()).exit_code(),
            ExitCode::from(1)
        );
    }

    #[test]
    fn test_error_display() {
        let err = CliError::CameraNotFound("/dev/video0".to_string());
        assert_eq!(format!("{}", err), "Camera not found: /dev/video0");
    }
}
