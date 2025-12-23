#!/usr/bin/env python3
"""
Camera → H.264 Encoder → IPC → H.264 Decoder Pipeline Performance Analyzer
Parses test output and generates comprehensive timing histograms for full encode-decode pipeline
"""
import re
import sys
from typing import List, Tuple

# Data structures
get_frame_times = []
decode_times = []
intervals = []
encoded_sizes = []
decoded_sizes = []
frame_nums = []

# Parse input
for line in sys.stdin:
    # Parse client frame timing with decode:
    # [CLIENT] Frame N: get_frame=Xms, decode=Xms, metadata=Xμs, interval=Xms, encoded_size=X, decoded_size=X
    match = re.search(r'\[CLIENT\] Frame (\d+): get_frame=(\d+)ms, decode=(\d+)ms.*interval=(\d+)ms, encoded_size=(\d+), decoded_size=(\d+)', line)
    if match:
        frame_num = int(match.group(1))
        get_frame_ms = int(match.group(2))
        decode_ms = int(match.group(3))
        interval_ms = int(match.group(4))
        enc_size = int(match.group(5))
        dec_size = int(match.group(6))

        frame_nums.append(frame_num)
        get_frame_times.append(get_frame_ms)
        decode_times.append(decode_ms)
        if frame_num > 1:
            intervals.append(interval_ms)
        encoded_sizes.append(enc_size)
        decoded_sizes.append(dec_size)

if not frame_nums:
    print("No timing data found in input")
    sys.exit(1)

# Find first successful decode (non-zero decoded size)
first_decode_idx = next((i for i, size in enumerate(decoded_sizes) if size > 0), None)
if first_decode_idx is None:
    print("No successful decodes found")
    warmup = len(frame_nums)
else:
    warmup = first_decode_idx
    print(f"First successful decode at frame {frame_nums[first_decode_idx]} (index {first_decode_idx})")

# Post-warmup data
get_frame_post = get_frame_times[warmup:] if len(get_frame_times) > warmup else get_frame_times
decode_post = decode_times[warmup:] if len(decode_times) > warmup else decode_times
intervals_post = intervals[warmup:] if len(intervals) > warmup else intervals
enc_sizes_post = encoded_sizes[warmup:] if len(encoded_sizes) > warmup else encoded_sizes
dec_sizes_post = decoded_sizes[warmup:] if len(decoded_sizes) > warmup else decoded_sizes

# Calculate successful decode count
successful_decodes = sum(1 for size in dec_sizes_post if size > 0)
decode_rate = (successful_decodes / len(dec_sizes_post) * 100) if dec_sizes_post else 0

print("\n" + "="*80)
print(" Camera → H.264 Encoder → IPC → H.264 Decoder Pipeline - Performance Analysis")
print("="*80)

# Frame reception statistics
print(f"\nFrame Reception:")
print(f"  Total Frames Logged: {len(frame_nums)}")
print(f"  Warmup Period: {warmup} frames (until first successful decode)")
print(f"  Steady-State Frames: {len(get_frame_post)}")
print(f"  Successful Decodes: {successful_decodes} ({decode_rate:.1f}%)")

# get_frame() Timing
if get_frame_post:
    gf_min = min(get_frame_post)
    gf_max = max(get_frame_post)
    gf_avg = sum(get_frame_post) / len(get_frame_post)

    print(f"\nget_frame() Time (post-warmup):")
    print(f"  Min: {gf_min} ms")
    print(f"  Max: {gf_max} ms")
    print(f"  Avg: {gf_avg:.1f} ms")
    print(f"  Jitter: {gf_max - gf_min} ms")

# Decode timing (only for successful decodes)
successful_decode_times = [decode_post[i] for i in range(len(decode_post)) if dec_sizes_post[i] > 0]
if successful_decode_times:
    dec_min = min(successful_decode_times)
    dec_max = max(successful_decode_times)
    dec_avg = sum(successful_decode_times) / len(successful_decode_times)

    print(f"\nDecode Time (successful decodes only, {len(successful_decode_times)} frames):")
    print(f"  Min: {dec_min} ms")
    print(f"  Max: {dec_max} ms")
    print(f"  Avg: {dec_avg:.1f} ms")
    print(f"  Jitter: {dec_max - dec_min} ms")

# Total end-to-end latency (get_frame + decode)
total_latencies = [get_frame_post[i] + decode_post[i] for i in range(len(get_frame_post))]
if total_latencies:
    lat_min = min(total_latencies)
    lat_max = max(total_latencies)
    lat_avg = sum(total_latencies) / len(total_latencies)

    print(f"\nEnd-to-End Latency (get_frame + decode, post-warmup):")
    print(f"  Min: {lat_min} ms")
    print(f"  Max: {lat_max} ms")
    print(f"  Avg: {lat_avg:.1f} ms")
    print(f"  Jitter: {lat_max - lat_min} ms")

# Frame intervals
if intervals_post:
    int_min = min(intervals_post)
    int_max = max(intervals_post)
    int_avg = sum(intervals_post) / len(intervals_post)

    print(f"\nFrame Interval (post-warmup):")
    print(f"  Min: {int_min} ms")
    print(f"  Max: {int_max} ms")
    print(f"  Avg: {int_avg:.1f} ms")
    print(f"  Jitter: {int_max - int_min} ms")

# Encoded frame sizes
if enc_sizes_post:
    enc_min = min(enc_sizes_post)
    enc_max = max(enc_sizes_post)
    enc_avg = sum(enc_sizes_post) / len(enc_sizes_post)

    print(f"\nEncoded Frame Size (post-warmup):")
    print(f"  Min: {enc_min:,} bytes ({enc_min/1024:.1f} KB)")
    print(f"  Max: {enc_max:,} bytes ({enc_max/1024:.1f} KB)")
    print(f"  Avg: {enc_avg:,.0f} bytes ({enc_avg/1024:.1f} KB)")

# Decoded frame sizes
decoded_nonzero = [size for size in dec_sizes_post if size > 0]
if decoded_nonzero:
    dec_min = min(decoded_nonzero)
    dec_max = max(decoded_nonzero)
    dec_avg = sum(decoded_nonzero) / len(decoded_nonzero)

    print(f"\nDecoded Frame Size (successful decodes, NV12 format):")
    print(f"  Min: {dec_min:,} bytes ({dec_min/1024:.0f} KB)")
    print(f"  Max: {dec_max:,} bytes ({dec_max/1024:.0f} KB)")
    print(f"  Avg: {dec_avg:,.0f} bytes ({dec_avg/1024:.0f} KB)")
    print(f"  Expected for 1280x720 NV12: 1,382,400 bytes (1,350 KB)")

# Warmup analysis
print("\n" + "="*80)
print("Warmup Period Analysis:")
print("-" * 80)
for i in range(min(warmup + 5, len(frame_nums))):
    decoded_str = f"{decoded_sizes[i]:,}" if decoded_sizes[i] > 0 else "NO DECODE"
    marker = " ✓ FIRST DECODE" if i == first_decode_idx else ""
    print(f"  Frame {frame_nums[i]:3d}: get_frame={get_frame_times[i]:3d}ms, "
          f"decode={decode_times[i]:3d}ms, "
          f"encoded={encoded_sizes[i]:,} bytes, "
          f"decoded={decoded_str:>10s}{marker}")

# Decode success pattern
print("\n" + "="*80)
print("Decode Pattern Analysis (post-warmup):")
print("-" * 80)
if dec_sizes_post:
    success_count = sum(1 for size in dec_sizes_post if size > 0)
    fail_count = len(dec_sizes_post) - success_count
    print(f"  Successful decodes: {success_count} ({success_count/len(dec_sizes_post)*100:.1f}%)")
    print(f"  Failed decodes: {fail_count} ({fail_count/len(dec_sizes_post)*100:.1f}%)")

    # Check for alternating pattern
    pattern = ['S' if size > 0 else 'F' for size in dec_sizes_post[:20]]
    print(f"  First 20 frames pattern: {''.join(pattern)}")
    if pattern == list('SF' * 10):
        print(f"  → Consistent alternating pattern detected (VPU internal buffering)")

# Latency distribution histogram
if total_latencies:
    print("\n" + "="*80)
    print("End-to-End Latency Distribution (post-warmup):")
    print("-" * 80)

    buckets = [0, 10, 50, 100, 150, 200, 250, 300, float('inf')]
    bucket_labels = ["0-10ms", "10-50ms", "50-100ms", "100-150ms", "150-200ms",
                     "200-250ms", "250-300ms", ">300ms"]
    bucket_counts = [0] * len(bucket_labels)

    for lat in total_latencies:
        for i, upper in enumerate(buckets[1:]):
            if lat <= upper:
                bucket_counts[i] += 1
                break

    max_count = max(bucket_counts) if bucket_counts else 1
    for label, count in zip(bucket_labels, bucket_counts):
        bar_width = int(count * 60 / max_count) if max_count > 0 else 0
        bar = "█" * bar_width
        pct = (count / len(total_latencies) * 100) if total_latencies else 0
        print(f"  {label:>12s}: {bar:60s} {count:3d} ({pct:5.1f}%)")

# Summary
print("\n" + "="*80)
print("Performance Summary:")
print("-" * 80)
print(f"  Total Frames: {len(frame_nums)}")
print(f"  Warmup Frames: {warmup}")
print(f"  Successful Decodes: {successful_decodes}/{len(dec_sizes_post)} ({decode_rate:.1f}%)")
print(f"  Avg get_frame(): {gf_avg:.1f} ms")
if successful_decode_times:
    print(f"  Avg Decode: {dec_avg:.1f} ms")
print(f"  Avg End-to-End: {lat_avg:.1f} ms")
if intervals_post:
    print(f"  Avg Interval: {int_avg:.1f} ms")
print(f"  Avg Encoded Size: {enc_avg/1024:.1f} KB")
if decoded_nonzero:
    print(f"  Avg Decoded Size: {dec_avg/1024:.0f} KB")
print("="*80)
