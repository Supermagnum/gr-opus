#!/usr/bin/env python3
"""
Performance and memory safety tests for Opus encoder/decoder blocks
Verifies:
- <10μs mean latency requirement
- <0.02ms latency and 40ms budget for real-time voice
- 100% stability
"""

import unittest
import numpy as np
import time
import sys
import os
import tracemalloc
import gc
from statistics import mean, stdev

# Add parent directory to path to import gr_opus
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'python'))
from opus_encoder import opus_encoder
from opus_decoder import opus_decoder


class qa_opus_performance(unittest.TestCase):
    """Performance and memory safety test suite"""

    def setUp(self):
        """Set up test fixtures"""
        self.sample_rate = 48000
        self.channels = 1
        self.frame_size = int(self.sample_rate * 0.020)  # 20ms frames
        self.iterations = 10000  # Number of iterations for statistical significance

    def test_001_encoder_latency_10us(self):
        """Test encoder mean latency < 10μs"""
        encoder = opus_encoder(
            sample_rate=self.sample_rate,
            channels=self.channels,
            bitrate=64000
        )
        
        # Generate test signal
        test_signal = np.random.randn(self.frame_size).astype(np.float32) * 0.5
        output_data = np.zeros(4000, dtype=np.uint8)
        
        # Warm-up
        for _ in range(100):
            encoder.work([test_signal], [output_data])
        
        # Measure latency
        latencies = []
        for _ in range(self.iterations):
            start = time.perf_counter()
            encoder.work([test_signal], [output_data])
            end = time.perf_counter()
            latency_us = (end - start) * 1e6  # Convert to microseconds
            latencies.append(latency_us)
        
        mean_latency = mean(latencies)
        std_latency = stdev(latencies) if len(latencies) > 1 else 0
        
        print(f"\nEncoder Latency Statistics:")
        print(f"  Mean: {mean_latency:.3f} μs")
        print(f"  Std Dev: {std_latency:.3f} μs")
        print(f"  Min: {min(latencies):.3f} μs")
        print(f"  Max: {max(latencies):.3f} μs")
        print(f"  P95: {np.percentile(latencies, 95):.3f} μs")
        print(f"  P99: {np.percentile(latencies, 99):.3f} μs")
        
        self.assertLess(mean_latency, 10.0, 
                       f"Mean latency {mean_latency:.3f}μs exceeds 10μs threshold")

    def test_002_decoder_latency_10us(self):
        """Test decoder mean latency < 10μs"""
        # Generate encoded packet first
        import opuslib
        test_encoder = opuslib.Encoder(self.sample_rate, self.channels, opuslib.APPLICATION_AUDIO)
        test_encoder.bitrate = 64000
        
        test_signal = np.random.randn(self.frame_size).astype(np.float32) * 0.5
        int16_samples = (test_signal * 32767.0).astype(np.int16)
        encoded_packet = test_encoder.encode(int16_samples.tobytes(), self.frame_size)
        packet_size = len(encoded_packet)
        
        decoder = opus_decoder(
            sample_rate=self.sample_rate,
            channels=self.channels,
            packet_size=packet_size
        )
        
        input_data = np.frombuffer(encoded_packet, dtype=np.uint8)
        output_data = np.zeros(self.frame_size * 2, dtype=np.float32)
        
        # Warm-up
        for _ in range(100):
            decoder.work([input_data], [output_data])
        
        # Measure latency
        latencies = []
        for _ in range(self.iterations):
            start = time.perf_counter()
            decoder.work([input_data], [output_data])
            end = time.perf_counter()
            latency_us = (end - start) * 1e6
            latencies.append(latency_us)
        
        mean_latency = mean(latencies)
        std_latency = stdev(latencies) if len(latencies) > 1 else 0
        
        print(f"\nDecoder Latency Statistics:")
        print(f"  Mean: {mean_latency:.3f} μs")
        print(f"  Std Dev: {std_latency:.3f} μs")
        print(f"  Min: {min(latencies):.3f} μs")
        print(f"  Max: {max(latencies):.3f} μs")
        print(f"  P95: {np.percentile(latencies, 95):.3f} μs")
        print(f"  P99: {np.percentile(latencies, 99):.3f} μs")
        
        self.assertLess(mean_latency, 10.0,
                       f"Mean latency {mean_latency:.3f}μs exceeds 10μs threshold")

    def test_003_realtime_voice_latency_0_02ms(self):
        """Test real-time voice requirements: <0.02ms latency, 40ms budget"""
        encoder = opus_encoder(
            sample_rate=self.sample_rate,
            channels=self.channels,
            bitrate=64000,
            application='lowdelay'
        )
        
        # Generate 20ms frame (real-time voice frame)
        test_signal = np.random.randn(self.frame_size).astype(np.float32) * 0.5
        output_data = np.zeros(4000, dtype=np.uint8)
        
        # Warm-up
        for _ in range(100):
            encoder.work([test_signal], [output_data])
        
        # Measure per-frame latency
        latencies = []
        for _ in range(1000):  # 1000 frames = 20 seconds of audio
            start = time.perf_counter()
            encoder.work([test_signal], [output_data])
            end = time.perf_counter()
            latency_ms = (end - start) * 1e3  # Convert to milliseconds
            latencies.append(latency_ms)
        
        mean_latency_ms = mean(latencies)
        max_latency_ms = max(latencies)
        p99_latency_ms = np.percentile(latencies, 99)
        
        print(f"\nReal-time Voice Latency Statistics:")
        print(f"  Mean: {mean_latency_ms:.6f} ms")
        print(f"  Max: {max_latency_ms:.6f} ms")
        print(f"  P99: {p99_latency_ms:.6f} ms")
        print(f"  40ms budget: {40.0 - mean_latency_ms:.6f} ms remaining")
        
        # Verify <0.02ms mean latency
        self.assertLess(mean_latency_ms, 0.02,
                       f"Mean latency {mean_latency_ms:.6f}ms exceeds 0.02ms threshold")
        
        # Verify 40ms budget is met (mean latency should be much less than 40ms)
        self.assertLess(mean_latency_ms, 40.0,
                       f"Mean latency {mean_latency_ms:.6f}ms exceeds 40ms budget")

    def test_004_roundtrip_latency_40ms_budget(self):
        """Test round-trip encoding+decoding within 40ms budget"""
        encoder = opus_encoder(
            sample_rate=self.sample_rate,
            channels=self.channels,
            bitrate=64000,
            application='lowdelay'
        )
        
        decoder = opus_decoder(
            sample_rate=self.sample_rate,
            channels=self.channels,
            packet_size=0  # Auto-detect
        )
        
        test_signal = np.random.randn(self.frame_size).astype(np.float32) * 0.5
        encoder_output = np.zeros(4000, dtype=np.uint8)
        decoder_output = np.zeros(self.frame_size * 2, dtype=np.float32)
        
        # Warm-up
        for _ in range(100):
            enc_produced = encoder.work([test_signal], [encoder_output])
            if enc_produced > 0:
                decoder.work([encoder_output[:enc_produced]], [decoder_output])
        
        # Measure round-trip latency
        latencies = []
        for _ in range(1000):
            start = time.perf_counter()
            enc_produced = encoder.work([test_signal], [encoder_output])
            if enc_produced > 0:
                decoder.work([encoder_output[:enc_produced]], [decoder_output])
            end = time.perf_counter()
            latency_ms = (end - start) * 1e3
            latencies.append(latency_ms)
        
        mean_latency_ms = mean(latencies)
        max_latency_ms = max(latencies)
        
        print(f"\nRound-trip Latency Statistics:")
        print(f"  Mean: {mean_latency_ms:.6f} ms")
        print(f"  Max: {max_latency_ms:.6f} ms")
        print(f"  40ms budget: {40.0 - mean_latency_ms:.6f} ms remaining")
        
        # Verify 40ms budget
        self.assertLess(mean_latency_ms, 40.0,
                       f"Round-trip latency {mean_latency_ms:.6f}ms exceeds 40ms budget")

    def test_005_memory_stability_100_percent(self):
        """Test 100% memory stability (no leaks, deterministic behavior)"""
        encoder = opus_encoder(
            sample_rate=self.sample_rate,
            channels=self.channels,
            bitrate=64000
        )
        
        test_signal = np.random.randn(self.frame_size).astype(np.float32) * 0.5
        output_data = np.zeros(4000, dtype=np.uint8)
        
        # Start memory tracking
        tracemalloc.start()
        
        # Get initial memory
        gc.collect()
        snapshot1 = tracemalloc.take_snapshot()
        
        # Run many iterations
        for _ in range(10000):
            encoder.work([test_signal], [output_data])
            if _ % 1000 == 0:
                gc.collect()
        
        # Get final memory
        gc.collect()
        snapshot2 = tracemalloc.take_snapshot()
        
        # Calculate memory difference
        top_stats = snapshot2.compare_to(snapshot1, 'lineno')
        
        total_diff = sum(stat.size_diff for stat in top_stats)
        total_diff_mb = total_diff / (1024 * 1024)
        
        print(f"\nMemory Stability Test:")
        print(f"  Memory difference: {total_diff_mb:.3f} MB")
        print(f"  Top 5 memory allocations:")
        for index, stat in enumerate(top_stats[:5], 1):
            print(f"    {index}. {stat}")
        
        # Memory should be stable (small difference is acceptable due to Python's GC)
        # For 100% stability, we expect minimal growth
        self.assertLess(total_diff_mb, 10.0,  # Allow up to 10MB for Python overhead
                       f"Memory growth {total_diff_mb:.3f}MB indicates potential leak")
        
        tracemalloc.stop()

    def test_006_deterministic_behavior(self):
        """Test deterministic behavior (100% stability)"""
        # Use fixed seed for deterministic input
        np.random.seed(42)
        test_signal = np.random.randn(self.frame_size).astype(np.float32) * 0.5
        output_data = np.zeros(4000, dtype=np.uint8)
        
        # Test: Same input with fresh encoder should produce same output
        results = []
        for _ in range(100):
            # Create fresh encoder each time to test determinism
            encoder = opus_encoder(
                sample_rate=self.sample_rate,
                channels=self.channels,
                bitrate=64000
            )
            
            np.random.seed(42)  # Reset seed
            test_signal = np.random.randn(self.frame_size).astype(np.float32) * 0.5
            produced = encoder.work([test_signal], [output_data])
            results.append((produced, output_data[:produced].tobytes()))
        
        # All results should be identical (deterministic)
        first_result = results[0]
        all_same = all(r == first_result for r in results)
        
        print(f"\nDeterministic Behavior Test:")
        print(f"  Iterations: 100 (fresh encoder each time)")
        print(f"  All results identical: {all_same}")
        print(f"  Output size: {first_result[0]} bytes")
        if not all_same:
            # Show first few differences
            unique_results = set(results)
            print(f"  Unique output patterns: {len(unique_results)}")
            for i, r in enumerate(results[:5]):
                if r != first_result:
                    print(f"  Difference at iteration {i}: size={r[0]}, first_result size={first_result[0]}")
        
        # For 100% stability, all fresh encoders with same input should produce same output
        self.assertTrue(all_same, "Non-deterministic behavior detected - same input produces different output")

    def test_007_buffer_size_stability(self):
        """Test buffer size remains stable (no unbounded growth)"""
        encoder = opus_encoder(
            sample_rate=self.sample_rate,
            channels=self.channels,
            bitrate=64000
        )
        
        # Send partial frames to test buffering
        partial_size = self.frame_size // 2
        test_signal = np.random.randn(partial_size).astype(np.float32) * 0.5
        output_data = np.zeros(4000, dtype=np.uint8)
        
        buffer_sizes = []
        
        # Run many iterations with partial frames
        for i in range(1000):
            encoder.work([test_signal], [output_data])
            buffer_sizes.append(len(encoder.sample_buffer))
            
            # Every 100 iterations, send a full frame to clear buffer
            if i % 100 == 0 and i > 0:
                full_frame = np.random.randn(self.frame_size).astype(np.float32) * 0.5
                encoder.work([full_frame], [output_data])
        
        max_buffer = max(buffer_sizes)
        mean_buffer = mean(buffer_sizes)
        
        print(f"\nBuffer Size Stability Test:")
        print(f"  Max buffer size: {max_buffer} samples")
        print(f"  Mean buffer size: {mean_buffer:.1f} samples")
        print(f"  Expected max (10s): {encoder.max_buffer_samples} samples")
        
        # Buffer should not exceed the maximum limit
        self.assertLessEqual(max_buffer, encoder.max_buffer_samples,
                           f"Buffer size {max_buffer} exceeds limit {encoder.max_buffer_samples}")

    def test_008_decoder_buffer_stability(self):
        """Test decoder buffer size remains stable"""
        decoder = opus_decoder(
            sample_rate=self.sample_rate,
            channels=self.channels,
            packet_size=0  # Auto-detect
        )
        
        # Generate encoded packet
        import opuslib
        test_encoder = opuslib.Encoder(self.sample_rate, self.channels, opuslib.APPLICATION_AUDIO)
        test_encoder.bitrate = 64000
        
        test_signal = np.random.randn(self.frame_size).astype(np.float32) * 0.5
        int16_samples = (test_signal * 32767.0).astype(np.int16)
        encoded_packet = test_encoder.encode(int16_samples.tobytes(), self.frame_size)
        
        # Send partial packets to test buffering
        partial_size = len(encoded_packet) // 2
        input_data = np.frombuffer(encoded_packet[:partial_size], dtype=np.uint8)
        output_data = np.zeros(self.frame_size * 2, dtype=np.float32)
        
        buffer_sizes = []
        
        for i in range(1000):
            decoder.work([input_data], [output_data])
            buffer_sizes.append(len(decoder.packet_buffer))
            
            # Every 100 iterations, send full packet
            if i % 100 == 0 and i > 0:
                full_input = np.frombuffer(encoded_packet, dtype=np.uint8)
                decoder.work([full_input], [output_data])
        
        max_buffer = max(buffer_sizes)
        mean_buffer = mean(buffer_sizes)
        
        print(f"\nDecoder Buffer Size Stability Test:")
        print(f"  Max buffer size: {max_buffer} bytes")
        print(f"  Mean buffer size: {mean_buffer:.1f} bytes")
        print(f"  Expected max: {decoder.max_buffer_size} bytes")
        
        self.assertLessEqual(max_buffer, decoder.max_buffer_size,
                           f"Buffer size {max_buffer} exceeds limit {decoder.max_buffer_size}")


if __name__ == '__main__':
    unittest.main(verbosity=2)

