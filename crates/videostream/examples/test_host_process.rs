use std::time::Instant;
use videostream::{camera, encoder, fourcc::FourCC, frame::Frame, host, timestamp};

fn bitrate_to_profile(bitrate_kbps: u32) -> u8 {
    match bitrate_kbps {
        0..=1000 => 0,     // Low
        1001..=3000 => 1,  // Medium
        3001..=7000 => 2,  // High
        _ => 3,            // Very high
    }
}

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let socket_path = std::env::args()
        .nth(1)
        .unwrap_or_else(|| "/tmp/vsl_separate_process_test.sock".to_string());
    let frame_count: usize = std::env::args()
        .nth(2)
        .and_then(|s| s.parse().ok())
        .unwrap_or(100);

    println!("[HOST] Starting host process");
    println!("[HOST] Socket: {}", socket_path);
    println!("[HOST] Frame count: {}", frame_count);

    // Create encoder
    println!("[HOST] Creating encoder...");
    let profile = bitrate_to_profile(5000);
    let codec_fourcc = u32::from_le_bytes(*b"H264");
    let enc = encoder::Encoder::create(profile as u32, codec_fourcc, 30)?;
    println!("[HOST] Encoder created");

    // Open camera
    println!("[HOST] Opening camera...");
    let camera_device = std::env::var("CAMERA_DEVICE").unwrap_or_else(|_| "/dev/video3".to_string());
    let cam = camera::create_camera()
        .with_device(&camera_device)
        .with_resolution(1280, 720)
        .with_format(FourCC(*b"YUYV"))
        .open()?;
    cam.start()?;
    println!("[HOST] Camera started");

    // Create host
    println!("[HOST] Creating host...");
    let mut host = host::Host::new(&socket_path)?;
    println!("[HOST] Host created at {}", socket_path);

    // Give client time to connect
    std::thread::sleep(std::time::Duration::from_millis(1000));

    // Capture, encode, and post frames
    let start = Instant::now();
    for i in 0..frame_count {
        let before_capture = Instant::now();
        let buffer = cam.read()?;
        let capture_time = before_capture.elapsed();

        let before_encode = Instant::now();
        let input_frame: Frame = (&buffer).try_into()?;
        let output_frame = enc.new_output_frame(1280, 720, -1, -1, -1)?;
        let crop = encoder::VSLRect::new(0, 0, 1280, 720);
        let mut keyframe: i32 = 0;
        unsafe {
            enc.frame(&input_frame, &output_frame, &crop, &mut keyframe)?;
        }
        let encode_time = before_encode.elapsed();

        let before_post = Instant::now();
        let now = timestamp()?;
        let expires = now + 5_000_000_000; // 5 second expiration
        host.post(output_frame, expires, -1, -1, -1)?;
        let post_time = before_post.elapsed();

        host.poll(100)?;
        host.process()?;

        if i < 5 || i % 20 == 0 {
            println!(
                "[HOST] Frame {}: capture={}ms, encode={}ms, post={}μs",
                i + 1,
                capture_time.as_millis(),
                encode_time.as_millis(),
                post_time.as_micros()
            );
        }
    }

    let duration = start.elapsed();
    println!(
        "[HOST] Completed {} frames in {:.2}s ({:.1} FPS)",
        frame_count,
        duration.as_secs_f64(),
        frame_count as f64 / duration.as_secs_f64()
    );

    // Continue processing for a bit to let client catch up
    println!("[HOST] Waiting for client to finish receiving...");
    for _ in 0..100 {
        host.poll(10)?;
        host.process()?;
        std::thread::sleep(std::time::Duration::from_millis(50));
    }

    println!("[HOST] Done");
    Ok(())
}
