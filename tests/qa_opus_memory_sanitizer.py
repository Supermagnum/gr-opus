#!/usr/bin/env python3
"""
Memory sanitizer tests for Opus encoder/decoder
Verifies memory safety and detects leaks
"""

import gc
import os
import sys
import tracemalloc
import unittest

import numpy as np

# Prefer gr_opus from gnuradio; fallback to local python
try:
    from gnuradio import gr_opus
    opus_encoder = gr_opus.opus_encoder
    opus_decoder = gr_opus.opus_decoder
except ImportError:
    sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "python"))
    from opus_decoder import opus_decoder
    from opus_encoder import opus_encoder


class qa_opus_memory_sanitizer(unittest.TestCase):
    """Memory sanitizer test suite"""

    def setUp(self):
        """Set up test fixtures"""
        self.sample_rate = 48000
        self.channels = 1
        self.frame_size = int(self.sample_rate * 0.020)

    def test_001_memory_leak_detection(self):
        """Test for memory leaks over many iterations"""
        tracemalloc.start()

        # Initial snapshot
        gc.collect()
        snapshot1 = tracemalloc.take_snapshot()

        # Create and use encoder many times
        for _ in range(1000):
            encoder = opus_encoder(sample_rate=self.sample_rate, channels=self.channels, bitrate=64000)

            test_signal = np.random.randn(self.frame_size).astype(np.float32) * 0.5
            output_data = np.zeros(4000, dtype=np.uint8)

            # Process many frames
            for __ in range(100):
                encoder.work([test_signal], [output_data])

            # Explicit cleanup
            del encoder
            gc.collect()

        # Final snapshot
        gc.collect()
        snapshot2 = tracemalloc.take_snapshot()

        # Compare snapshots
        top_stats = snapshot2.compare_to(snapshot1, "lineno")

        total_diff = sum(stat.size_diff for stat in top_stats)
        total_diff_mb = total_diff / (1024 * 1024)

        print("\nMemory Leak Detection Test:")
        print("  Iterations: 1000 encoders, 100 frames each")
        print(f"  Total memory difference: {total_diff_mb:.3f} MB")
        print("  Top 5 allocations:")
        for index, stat in enumerate(top_stats[:5], 1):
            print(f"    {index}. {stat}")

        # Memory growth should be minimal (< 50MB for Python overhead)
        self.assertLess(total_diff_mb, 50.0, f"Potential memory leak detected: {total_diff_mb:.3f}MB growth")

        tracemalloc.stop()

    def test_002_buffer_memory_bounds(self):
        """Test buffer memory stays within bounds (Python impl only)"""
        encoder = opus_encoder(sample_rate=self.sample_rate, channels=self.channels, bitrate=64000)
        if not hasattr(encoder, "sample_buffer"):
            self.skipTest("Buffer not exposed (C++ implementation)")

        # Send many partial frames to fill buffer
        partial_size = self.frame_size // 4
        test_signal = np.random.randn(partial_size).astype(np.float32) * 0.5
        output_data = np.zeros(4000, dtype=np.uint8)

        max_buffer_size = 0
        buffer_sizes = []

        for _ in range(10000):
            encoder.work([test_signal], [output_data])
            buffer_size = len(encoder.sample_buffer)
            buffer_sizes.append(buffer_size)
            max_buffer_size = max(max_buffer_size, buffer_size)

        mean_buffer = np.mean(buffer_sizes)

        print("\nBuffer Memory Bounds Test:")
        print(f"  Max buffer size: {max_buffer_size} samples")
        print(f"  Mean buffer size: {mean_buffer:.1f} samples")
        print(f"  Limit: {encoder.max_buffer_samples} samples")
        print(f"  Memory usage: ~{max_buffer_size * 4 / 1024:.2f} KB (float32)")

        # Buffer should never exceed limit
        self.assertLessEqual(
            max_buffer_size,
            encoder.max_buffer_samples,
            f"Buffer exceeded limit: {max_buffer_size} > {encoder.max_buffer_samples}",
        )

    def test_003_decoder_buffer_memory_bounds(self):
        """Test decoder buffer memory stays within bounds (Python impl only)"""
        decoder = opus_decoder(sample_rate=self.sample_rate, channels=self.channels, packet_size=0)
        if not hasattr(decoder, "packet_buffer"):
            self.skipTest("Buffer not exposed (C++ implementation)")

        # Generate encoded packet
        import opuslib

        test_encoder = opuslib.Encoder(self.sample_rate, self.channels, opuslib.APPLICATION_AUDIO)
        test_encoder.bitrate = 64000

        test_signal = np.random.randn(self.frame_size).astype(np.float32) * 0.5
        int16_samples = (test_signal * 32767.0).astype(np.int16)
        encoded_packet = test_encoder.encode(int16_samples.tobytes(), self.frame_size)

        # Send partial packets
        partial_size = len(encoded_packet) // 4
        input_data = np.frombuffer(encoded_packet[:partial_size], dtype=np.uint8)
        output_data = np.zeros(self.frame_size * 2, dtype=np.float32)

        max_buffer_size = 0
        buffer_sizes = []

        for _ in range(10000):
            decoder.work([input_data], [output_data])
            buffer_size = len(decoder.packet_buffer)
            buffer_sizes.append(buffer_size)
            max_buffer_size = max(max_buffer_size, buffer_size)

        mean_buffer = np.mean(buffer_sizes)

        print("\nDecoder Buffer Memory Bounds Test:")
        print(f"  Max buffer size: {max_buffer_size} bytes")
        print(f"  Mean buffer size: {mean_buffer:.1f} bytes")
        print(f"  Limit: {decoder.max_buffer_size} bytes")
        print(f"  Memory usage: ~{max_buffer_size / 1024:.2f} KB")

        self.assertLessEqual(
            max_buffer_size,
            decoder.max_buffer_size,
            f"Buffer exceeded limit: {max_buffer_size} > {decoder.max_buffer_size}",
        )

    def test_004_memory_cleanup_on_deletion(self):
        """Test memory is properly cleaned up when objects are deleted"""
        tracemalloc.start()

        # Create many encoders and decoders
        encoders = []
        decoders = []

        for _ in range(100):
            encoder = opus_encoder(sample_rate=self.sample_rate, channels=self.channels, bitrate=64000)
            decoder = opus_decoder(sample_rate=self.sample_rate, channels=self.channels)
            encoders.append(encoder)
            decoders.append(decoder)

        snapshot1 = tracemalloc.take_snapshot()

        # Delete all objects
        del encoders
        del decoders
        gc.collect()

        snapshot2 = tracemalloc.take_snapshot()

        # Memory should be freed
        top_stats = snapshot2.compare_to(snapshot1, "lineno")
        total_diff = sum(stat.size_diff for stat in top_stats)
        total_diff_mb = total_diff / (1024 * 1024)

        print("\nMemory Cleanup Test:")
        print("  Objects created: 100 encoders + 100 decoders")
        print(f"  Memory after deletion: {total_diff_mb:.3f} MB")

        # Memory should decrease (negative diff) or be stable
        # Allow some Python overhead
        self.assertLess(total_diff_mb, 10.0, f"Memory not properly freed: {total_diff_mb:.3f}MB")

        tracemalloc.stop()

    def test_005_stability_100_percent(self):
        """Test 100% stability - deterministic memory usage (Python impl only)"""
        encoder = opus_encoder(sample_rate=self.sample_rate, channels=self.channels, bitrate=64000)
        if not hasattr(encoder, "sample_buffer"):
            self.skipTest("Buffer not exposed (C++ implementation)")

        # Use fixed seed for deterministic input
        np.random.seed(42)
        test_signal = np.random.randn(self.frame_size).astype(np.float32) * 0.5
        output_data = np.zeros(4000, dtype=np.uint8)

        # Run multiple times and check buffer state is consistent
        buffer_states = []

        for _ in range(100):
            np.random.seed(42)
            test_signal = np.random.randn(self.frame_size).astype(np.float32) * 0.5
            encoder.work([test_signal], [output_data])
            buffer_states.append(len(encoder.sample_buffer))

        # All buffer states should be identical (deterministic)
        unique_states = set(buffer_states)

        print("\nStability Test (100% deterministic):")
        print("  Iterations: 100")
        print(f"  Unique buffer states: {len(unique_states)}")
        print(f"  Buffer states: {buffer_states[:10]}...")

        # Should have consistent buffer state
        self.assertEqual(
            len(unique_states), 1, f"Non-deterministic behavior: {len(unique_states)} different buffer states"
        )


if __name__ == "__main__":
    unittest.main(verbosity=2)
