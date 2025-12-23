use std::time::Instant;
use videostream::{client, timestamp};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let socket_path = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "/tmp/vsl_separate_process_test.sock".to_string());
    let frame_count: usize = std::env::args()
        .nth(2)
        .and_then(|s| s.parse().ok())
        .unwrap_or(100);

    println!("[CLIENT] Starting client process");
    println!("[CLIENT] Socket: {}", socket_path);
    println!("[CLIENT] Expected frames: {}", frame_count);

    // Wait a bit for host to start
    std::thread::sleep(std::time::Duration::from_millis(500));

    println!("[CLIENT] Connecting to host...");
    let client = client::Client::new(&socket_path, client::Reconnect::Yes)?;
    println!("[CLIENT] Connected");

    let mut received = 0;
    let mut total_bytes = 0u64;
    let mut keyframes = 0;
    let start = Instant::now();
    let mut last_frame_time = Instant::now();

    println!("[CLIENT] Starting receive loop...");
    while received < frame_count {
        let before_get_frame = Instant::now();

        match client.get_frame(timestamp()? - 10_000_000_000) {
            Ok(frame) => {
                let get_frame_duration = before_get_frame.elapsed();
                received += 1;

                let before_metadata = Instant::now();
                let size = frame.size().unwrap_or(0);
                let metadata_duration = before_metadata.elapsed();
                total_bytes += size as u64;

                // Check if keyframe
                if received > 0 {
                    let avg_size = total_bytes / received as u64;
                    if size as u64 > (avg_size * 3 / 2) {
                        keyframes += 1;
                    }
                }

                let frame_interval = last_frame_time.elapsed();
                last_frame_time = Instant::now();

                // Print timing for all frames to catch slow ones
                if get_frame_duration.as_millis() > 100 || received < 5 || received % 20 == 0 {
                    println!(
                        "[CLIENT] Frame {}: get_frame={}ms, metadata={}μs, interval={}ms, size={}",
                        received,
                        get_frame_duration.as_millis(),
                        metadata_duration.as_micros(),
                        frame_interval.as_millis(),
                        size
                    );
                }

                if get_frame_duration.as_millis() > 500 {
                    println!(
                        "[CLIENT] WARNING: Frame {} get_frame took {}ms!",
                        received,
                        get_frame_duration.as_millis()
                    );
                }
            }
            Err(e) => {
                println!("[CLIENT] Error receiving frame {}: {}", received + 1, e);
                std::thread::sleep(std::time::Duration::from_millis(10));
            }
        }
    }

    let duration = start.elapsed();
    println!("[CLIENT] Received {} frames in {:.2}s", received, duration.as_secs_f64());
    println!(
        "[CLIENT] Throughput: {:.1} FPS, {:.2} MB total, {} keyframes",
        received as f64 / duration.as_secs_f64(),
        total_bytes as f64 / 1_000_000.0,
        keyframes
    );

    Ok(())
}
