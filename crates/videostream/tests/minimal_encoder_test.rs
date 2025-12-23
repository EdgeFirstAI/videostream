// Minimal encoder test - mimic C test behavior
// DOES NOT use camera, only creates encoder

use videostream::encoder::{Encoder, VSLEncoderProfileEnum};

#[test]
#[ignore]
fn test_minimal_encoder_create() {
    let _ = env_logger::builder().is_test(true).try_init();

    println!("Checking encoder availability...");
    match videostream::encoder::is_available() {
        Ok(true) => println!("✓ Encoder symbols available"),
        Ok(false) => {
            println!("✗ Encoder not available");
            return;
        }
        Err(e) => {
            println!("✗ Error checking encoder: {:?}", e);
            return;
        }
    }

    println!("Creating encoder...");
    let encoder = Encoder::create(
        VSLEncoderProfileEnum::Kbps5000 as u32,
        u32::from_le_bytes(*b"HEVC"),
        30,
    );

    match encoder {
        Ok(_) => println!("✓ Encoder created successfully!"),
        Err(e) => {
            println!("✗ Encoder creation failed: {:?}", e);
            panic!("Encoder creation failed");
        }
    }

    println!("✓ Test complete");
}
