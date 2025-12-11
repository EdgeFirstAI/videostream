// SPDX-License-Identifier: Apache-2.0
// Copyright 2025 Au-Zone Technologies

use serde::Serialize;
use std::time::Instant;

/// Performance metrics collected during operation
#[derive(Debug, Clone, Serialize)]
pub struct FrameMetrics {
    /// Total number of frames processed
    pub frames_processed: u64,
    /// Total bytes processed
    pub bytes_processed: u64,
    /// Total duration in milliseconds
    pub duration_ms: u64,
    /// Average throughput in frames per second
    pub throughput_fps: f64,
    /// Average bandwidth in megabits per second
    pub bandwidth_mbps: f64,
    /// Minimum latency in microseconds
    pub latency_min_us: u64,
    /// Maximum latency in microseconds
    pub latency_max_us: u64,
    /// Average latency in microseconds
    pub latency_avg_us: u64,
    /// 50th percentile (median) latency in microseconds
    pub latency_p50_us: u64,
    /// 95th percentile latency in microseconds
    pub latency_p95_us: u64,
    /// 99th percentile latency in microseconds
    pub latency_p99_us: u64,
    /// Number of dropped frames detected
    pub dropped_frames: u64,
}

/// Metrics collector for tracking frame processing performance
pub struct MetricsCollector {
    start_time: Instant,
    latencies_us: Vec<u64>,
    bytes: u64,
    prev_serial: Option<i64>,
    dropped_frames: u64,
}

impl MetricsCollector {
    /// Create a new metrics collector
    pub fn new() -> Self {
        Self {
            start_time: Instant::now(),
            latencies_us: Vec::new(),
            bytes: 0,
            prev_serial: None,
            dropped_frames: 0,
        }
    }

    /// Record a frame's latency in nanoseconds
    pub fn record_latency_ns(&mut self, latency_ns: i64) {
        // Convert nanoseconds to microseconds
        let latency_us = (latency_ns.max(0) / 1000) as u64;
        self.latencies_us.push(latency_us);
    }

    /// Record a frame's latency in microseconds
    pub fn record_latency_us(&mut self, latency_us: u64) {
        self.latencies_us.push(latency_us);
    }

    /// Record bytes processed
    pub fn record_bytes(&mut self, bytes: u64) {
        self.bytes += bytes;
    }

    /// Track frame serial number to detect drops
    /// Returns the number of dropped frames detected (0 or N)
    pub fn track_serial(&mut self, serial: i64) -> u64 {
        let drops = if let Some(prev) = self.prev_serial {
            // Calculate expected next serial
            let expected = prev + 1;
            if serial > expected {
                // Frames were dropped
                (serial - expected) as u64
            } else if serial < expected {
                // Serial wrapped or went backwards - log but don't count as drops
                log::warn!("Frame serial number decreased: {} -> {}", prev, serial);
                0
            } else {
                // No drops
                0
            }
        } else {
            // First frame
            0
        };

        self.dropped_frames += drops;
        self.prev_serial = Some(serial);
        drops
    }

    /// Finalize and calculate all metrics
    pub fn finalize(&mut self) -> FrameMetrics {
        let duration = self.start_time.elapsed();
        let duration_ms = duration.as_millis() as u64;
        let duration_secs = duration.as_secs_f64();

        let frames_processed = self.latencies_us.len() as u64;

        // Calculate throughput
        let throughput_fps = if duration_secs > 0.0 {
            frames_processed as f64 / duration_secs
        } else {
            0.0
        };

        // Calculate bandwidth in Mbps
        let bandwidth_mbps = if duration_secs > 0.0 {
            (self.bytes as f64 * 8.0) / (duration_secs * 1_000_000.0)
        } else {
            0.0
        };

        // Calculate latency statistics
        let (min_us, max_us, avg_us, p50_us, p95_us, p99_us) = if !self.latencies_us.is_empty() {
            // Sort for percentile calculation
            self.latencies_us.sort_unstable();

            let min = *self.latencies_us.first().unwrap();
            let max = *self.latencies_us.last().unwrap();
            let sum: u64 = self.latencies_us.iter().sum();
            let avg = sum / self.latencies_us.len() as u64;

            let p50 = self.percentile(50.0);
            let p95 = self.percentile(95.0);
            let p99 = self.percentile(99.0);

            (min, max, avg, p50, p95, p99)
        } else {
            (0, 0, 0, 0, 0, 0)
        };

        FrameMetrics {
            frames_processed,
            bytes_processed: self.bytes,
            duration_ms,
            throughput_fps,
            bandwidth_mbps,
            latency_min_us: min_us,
            latency_max_us: max_us,
            latency_avg_us: avg_us,
            latency_p50_us: p50_us,
            latency_p95_us: p95_us,
            latency_p99_us: p99_us,
            dropped_frames: self.dropped_frames,
        }
    }

    /// Calculate percentile from sorted latency data
    /// Assumes self.latencies_us is already sorted
    fn percentile(&self, p: f64) -> u64 {
        if self.latencies_us.is_empty() {
            return 0;
        }

        let len = self.latencies_us.len();
        let idx = ((p / 100.0) * (len - 1) as f64).round() as usize;
        self.latencies_us[idx.min(len - 1)]
    }

    /// Print metrics in human-readable format
    pub fn print_text(&mut self) {
        let metrics = self.finalize();
        println!("\n=== Performance Metrics ===");
        println!("Frames processed:  {}", metrics.frames_processed);
        println!(
            "Bytes processed:   {} ({:.2} MB)",
            metrics.bytes_processed,
            metrics.bytes_processed as f64 / 1_048_576.0
        );
        println!(
            "Duration:          {:.2} s",
            metrics.duration_ms as f64 / 1000.0
        );
        println!("Throughput:        {:.2} fps", metrics.throughput_fps);
        println!("Bandwidth:         {:.2} Mbps", metrics.bandwidth_mbps);

        if metrics.frames_processed > 0 {
            println!("\nLatency Statistics (µs):");
            println!("  Min:    {}", metrics.latency_min_us);
            println!("  Max:    {}", metrics.latency_max_us);
            println!("  Avg:    {}", metrics.latency_avg_us);
            println!("  P50:    {}", metrics.latency_p50_us);
            println!("  P95:    {}", metrics.latency_p95_us);
            println!("  P99:    {}", metrics.latency_p99_us);
        }

        if metrics.dropped_frames > 0 {
            println!(
                "\nDropped frames:    {} ({:.2}%)",
                metrics.dropped_frames,
                (metrics.dropped_frames as f64 / metrics.frames_processed as f64) * 100.0
            );
        }
    }

    /// Print metrics in JSON format
    pub fn print_json(&mut self) -> Result<(), serde_json::Error> {
        let metrics = self.finalize();
        let json = serde_json::to_string_pretty(&metrics)?;
        println!("{}", json);
        Ok(())
    }
}

impl Default for MetricsCollector {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_percentile_calculation() {
        let mut collector = MetricsCollector::new();

        // Add test data: 0, 10, 20, ..., 90, 100 (11 values)
        for i in 0..=10 {
            collector.record_latency_us(i * 10);
        }

        let metrics = collector.finalize();

        // P50 should be around 50
        assert_eq!(metrics.latency_p50_us, 50);

        // P95 should be around 95
        assert_eq!(metrics.latency_p95_us, 100);

        // Min and max
        assert_eq!(metrics.latency_min_us, 0);
        assert_eq!(metrics.latency_max_us, 100);

        // Average should be 50
        assert_eq!(metrics.latency_avg_us, 50);
    }

    #[test]
    fn test_dropped_frames_detection() {
        let mut collector = MetricsCollector::new();

        // Normal sequence
        collector.track_serial(100);
        collector.track_serial(101);
        collector.track_serial(102);

        // Drop 3 frames
        collector.track_serial(106);

        assert_eq!(collector.dropped_frames, 3);

        // Continue normal
        collector.track_serial(107);
        assert_eq!(collector.dropped_frames, 3);
    }

    #[test]
    fn test_throughput_calculation() {
        let mut collector = MetricsCollector::new();

        // Simulate some frames
        for _ in 0..30 {
            collector.record_latency_us(1000);
            collector.record_bytes(100_000);
        }

        std::thread::sleep(std::time::Duration::from_millis(100));

        let metrics = collector.finalize();

        assert_eq!(metrics.frames_processed, 30);
        assert_eq!(metrics.bytes_processed, 3_000_000);

        // Throughput should be roughly 300 fps (30 frames / 0.1 sec)
        assert!(metrics.throughput_fps > 200.0 && metrics.throughput_fps < 400.0);
    }

    #[test]
    fn test_empty_metrics() {
        let mut collector = MetricsCollector::new();
        let metrics = collector.finalize();

        assert_eq!(metrics.frames_processed, 0);
        assert_eq!(metrics.bytes_processed, 0);
        assert_eq!(metrics.latency_min_us, 0);
        assert_eq!(metrics.latency_max_us, 0);
    }
}
