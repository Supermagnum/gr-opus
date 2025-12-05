#!/usr/bin/env python3
"""
Dudect-style timing side-channel analysis for Opus encoder/decoder
Detects timing-dependent behavior that could leak information
"""

import unittest
import numpy as np
import time
import sys
import os
from scipy import stats
from collections import defaultdict

# Add parent directory to path to import gr_opus
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'python'))
from opus_encoder import opus_encoder
from opus_decoder import opus_decoder


class qa_opus_dudect(unittest.TestCase):
    """Dudect-style timing analysis tests"""

    def setUp(self):
        """Set up test fixtures"""
        self.sample_rate = 48000
        self.channels = 1
        self.frame_size = int(self.sample_rate * 0.020)
        self.iterations = 100000  # Large sample size for statistical significance

    def _measure_timing(self, func, *args, **kwargs):
        """Measure execution time with high precision"""
        start = time.perf_counter()
        result = func(*args, **kwargs)
        end = time.perf_counter()
        return (end - start) * 1e6, result  # Return microseconds

    def _welch_ttest(self, group1, group2):
        """Perform Welch's t-test for unequal variances"""
        if len(group1) < 2 or len(group2) < 2:
            return None, None
        
        t_stat, p_value = stats.ttest_ind(group1, group2, equal_var=False)
        return t_stat, p_value

    def _mann_whitney_test(self, group1, group2):
        """Perform Mann-Whitney U test (non-parametric)"""
        if len(group1) < 2 or len(group2) < 2:
            return None, None
        
        u_stat, p_value = stats.mannwhitneyu(group1, group2, alternative='two-sided')
        return u_stat, p_value

    def test_001_encoder_timing_independence(self):
        """Test encoder timing is independent of input data (no side-channels)"""
        encoder = opus_encoder(
            sample_rate=self.sample_rate,
            channels=self.channels,
            bitrate=64000
        )
        
        output_data = np.zeros(4000, dtype=np.uint8)
        
        # Warm-up
        for _ in range(1000):
            test_signal = np.random.randn(self.frame_size).astype(np.float32) * 0.5
            encoder.work([test_signal], [output_data])
        
        # Generate two groups of inputs with different characteristics
        # Group 1: Silence (zeros)
        # Group 2: Random noise
        group1_timings = []
        group2_timings = []
        
        for _ in range(self.iterations // 2):
            # Group 1: Silence
            silence = np.zeros(self.frame_size, dtype=np.float32)
            timing, _ = self._measure_timing(encoder.work, [silence], [output_data])
            group1_timings.append(timing)
            
            # Group 2: Random noise
            noise = np.random.randn(self.frame_size).astype(np.float32) * 0.5
            timing, _ = self._measure_timing(encoder.work, [noise], [output_data])
            group2_timings.append(timing)
        
        # Statistical tests
        welch_t, welch_p = self._welch_ttest(group1_timings, group2_timings)
        mw_u, mw_p = self._mann_whitney_test(group1_timings, group2_timings)
        
        mean1 = np.mean(group1_timings)
        mean2 = np.mean(group2_timings)
        std1 = np.std(group1_timings)
        std2 = np.std(group2_timings)
        
        print(f"\nEncoder Timing Independence Test:")
        print(f"  Group 1 (silence): mean={mean1:.3f}μs, std={std1:.3f}μs")
        print(f"  Group 2 (noise): mean={mean2:.3f}μs, std={std2:.3f}μs")
        print(f"  Difference: {abs(mean1 - mean2):.3f}μs")
        print(f"  Welch's t-test: t={welch_t:.3f}, p={welch_p:.6f}")
        print(f"  Mann-Whitney U: U={mw_u:.3f}, p={mw_p:.6f}")
        
        # For timing side-channel resistance, p-value should be high (>0.05)
        # indicating no significant difference between groups
        if welch_p is not None:
            self.assertGreater(welch_p, 0.01,  # Allow some tolerance
                             f"Timing difference detected (p={welch_p:.6f}), potential side-channel")

    def test_002_decoder_timing_independence(self):
        """Test decoder timing is independent of packet content"""
        import opuslib
        test_encoder = opuslib.Encoder(self.sample_rate, self.channels, opuslib.APPLICATION_AUDIO)
        test_encoder.bitrate = 64000
        
        decoder = opus_decoder(
            sample_rate=self.sample_rate,
            channels=self.channels,
            packet_size=0  # Auto-detect
        )
        
        output_data = np.zeros(self.frame_size * 2, dtype=np.float32)
        
        # Warm-up
        for _ in range(1000):
            test_signal = np.random.randn(self.frame_size).astype(np.float32) * 0.5
            int16_samples = (test_signal * 32767.0).astype(np.int16)
            encoded = test_encoder.encode(int16_samples.tobytes(), self.frame_size)
            input_data = np.frombuffer(encoded, dtype=np.uint8)
            decoder.work([input_data], [output_data])
        
        # Generate two groups: silence vs noise
        group1_timings = []
        group2_timings = []
        
        for _ in range(self.iterations // 2):
            # Group 1: Encoded silence
            silence = np.zeros(self.frame_size, dtype=np.float32)
            int16_silence = (silence * 32767.0).astype(np.int16)
            encoded_silence = test_encoder.encode(int16_silence.tobytes(), self.frame_size)
            input_silence = np.frombuffer(encoded_silence, dtype=np.uint8)
            
            timing, _ = self._measure_timing(decoder.work, [input_silence], [output_data])
            group1_timings.append(timing)
            
            # Group 2: Encoded noise
            noise = np.random.randn(self.frame_size).astype(np.float32) * 0.5
            int16_noise = (noise * 32767.0).astype(np.int16)
            encoded_noise = test_encoder.encode(int16_noise.tobytes(), self.frame_size)
            input_noise = np.frombuffer(encoded_noise, dtype=np.uint8)
            
            timing, _ = self._measure_timing(decoder.work, [input_noise], [output_data])
            group2_timings.append(timing)
        
        # Statistical tests
        welch_t, welch_p = self._welch_ttest(group1_timings, group2_timings)
        mw_u, mw_p = self._mann_whitney_test(group1_timings, group2_timings)
        
        mean1 = np.mean(group1_timings)
        mean2 = np.mean(group2_timings)
        
        print(f"\nDecoder Timing Independence Test:")
        print(f"  Group 1 (silence): mean={mean1:.3f}μs")
        print(f"  Group 2 (noise): mean={mean2:.3f}μs")
        print(f"  Difference: {abs(mean1 - mean2):.3f}μs")
        print(f"  Welch's t-test: t={welch_t:.3f}, p={welch_p:.6f}")
        print(f"  Mann-Whitney U: U={mw_u:.3f}, p={mw_p:.6f}")
        
        if welch_p is not None:
            self.assertGreater(welch_p, 0.01,
                             f"Timing difference detected (p={welch_p:.6f}), potential side-channel")

    def test_003_bitrate_timing_independence(self):
        """Test timing is independent of bitrate setting"""
        output_data = np.zeros(4000, dtype=np.uint8)
        test_signal = np.random.randn(self.frame_size).astype(np.float32) * 0.5
        
        # Test different bitrates
        bitrates = [32000, 64000, 128000]
        timing_by_bitrate = defaultdict(list)
        
        for bitrate in bitrates:
            encoder = opus_encoder(
                sample_rate=self.sample_rate,
                channels=self.channels,
                bitrate=bitrate
            )
            
            # Warm-up
            for _ in range(1000):
                encoder.work([test_signal], [output_data])
            
            # Measure
            for _ in range(10000):
                timing, _ = self._measure_timing(encoder.work, [test_signal], [output_data])
                timing_by_bitrate[bitrate].append(timing)
        
        # Compare bitrates pairwise
        bitrate_list = list(bitrates)
        all_p_values = []
        
        for i in range(len(bitrate_list)):
            for j in range(i + 1, len(bitrate_list)):
                b1, b2 = bitrate_list[i], bitrate_list[j]
                _, p_value = self._welch_ttest(timing_by_bitrate[b1], timing_by_bitrate[b2])
                if p_value is not None:
                    all_p_values.append(p_value)
                    print(f"  Bitrate {b1} vs {b2}: p={p_value:.6f}")
        
        print(f"\nBitrate Timing Independence Test:")
        print(f"  All pairwise comparisons: {len(all_p_values)}")
        print(f"  Min p-value: {min(all_p_values):.6f}")
        
        # All comparisons should show no significant difference
        min_p = min(all_p_values) if all_p_values else 1.0
        self.assertGreater(min_p, 0.01,
                          f"Timing difference detected between bitrates (min p={min_p:.6f})")


if __name__ == '__main__':
    unittest.main(verbosity=2)

