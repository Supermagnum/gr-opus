#!/usr/bin/env python3
"""
GNU Radio block for Opus audio encoding
"""

import numpy as np
from gnuradio import gr
import opuslib

class opus_encoder(gr.sync_block):
    """
    Opus audio encoder block

    Encodes PCM audio samples to Opus format.
    Input: Float32 audio samples (mono or stereo)
    Output: Bytes containing Opus-encoded packets
    """

    def __init__(self, sample_rate=48000, channels=1, bitrate=64000, application='audio'):
        """
        Initialize Opus encoder

        Args:
            sample_rate: Input sample rate (8000, 12000, 16000, 24000, or 48000 Hz)
            channels: Number of channels (1 for mono, 2 for stereo)
            bitrate: Target bitrate in bits per second
            application: Opus application type ('voip', 'audio', or 'lowdelay')
        """
        gr.sync_block.__init__(
            self,
            name="opus_encoder",
            in_sig=[np.float32],
            out_sig=[np.uint8]
        )
        
        self.sample_rate = sample_rate
        self.channels = channels
        self.bitrate = bitrate
        
        # Map application string to opuslib constant
        app_map = {
            'voip': opuslib.APPLICATION_VOIP,
            'audio': opuslib.APPLICATION_AUDIO,
            'lowdelay': opuslib.APPLICATION_RESTRICTED_LOWDELAY
        }
        self.application = app_map.get(application.lower(), opuslib.APPLICATION_AUDIO)
        
        # Create Opus encoder
        self.encoder = opuslib.Encoder(sample_rate, channels, self.application)
        self.encoder.bitrate = bitrate
        
        # Frame size in samples (20ms frames for good quality)
        self.frame_size = int(sample_rate * 0.020)  # 20ms frames
        
        # Buffer for accumulating samples (use list for efficient appending)
        self.sample_buffer = []
        
        # Maximum buffer size (10 seconds of audio) to prevent memory leaks
        self.max_buffer_samples = sample_rate * channels * 10
        
        # Convert float32 samples to int16 for Opus
        self.max_int16 = 32767.0
    
    def forecast(self, noutput_items, ninput_items_required):
        """
        Forecast how many input items are needed for noutput_items.
        
        For Opus encoding, output size is variable but typically:
        - 20ms frame at 8kHz = 160 samples input -> ~40 bytes output (at 6 kbps)
        - Average ratio: ~4:1 (input samples to output bytes)
        - But we need complete frames, so we request enough for at least one frame
        """
        # Ensure we have enough input for at least one complete frame
        frame_size_samples = self.frame_size * self.channels
        # Request enough input for complete frames
        # Add some buffer to account for variable output size
        required = max(frame_size_samples, noutput_items * 4)
        
        # Handle both list and int cases (GNU Radio may pass either)
        if isinstance(ninput_items_required, list):
            ninput_items_required[0] = required
        else:
            # If it's not a list, GNU Radio might be using a different mechanism
            # For sync_blocks, forecast might not be strictly necessary
            pass
    
    def work(self, input_items, output_items):
        """
        Process audio samples and encode to Opus
        """
        in0 = input_items[0]
        out = output_items[0]
        
        # Add new samples to buffer (efficient list append)
        self.sample_buffer.extend(in0.tolist())
        
        # Prevent unbounded buffer growth (memory leak protection)
        if len(self.sample_buffer) > self.max_buffer_samples:
            # Keep only the most recent samples (drop oldest)
            excess = len(self.sample_buffer) - self.max_buffer_samples
            self.sample_buffer = self.sample_buffer[excess:]
        
        output_idx = 0
        frame_size_samples = self.frame_size * self.channels
        
        # Process complete frames
        while len(self.sample_buffer) >= frame_size_samples and output_idx < len(out):
            # Extract frame (convert list slice to numpy array)
            frame_samples = np.array(self.sample_buffer[:frame_size_samples], dtype=np.float32)
            # Remove processed samples (efficient list slicing)
            self.sample_buffer = self.sample_buffer[frame_size_samples:]
            
            # Reshape for multi-channel
            if self.channels > 1:
                frame_samples = frame_samples.reshape(-1, self.channels)
            
            # Convert float32 [-1.0, 1.0] to int16
            int16_samples = (frame_samples * self.max_int16).astype(np.int16)
            
            # Encode frame
            try:
                encoded_data = self.encoder.encode(int16_samples.tobytes(), self.frame_size)
                
                # Write encoded packet to output
                if output_idx + len(encoded_data) <= len(out):
                    out[output_idx:output_idx + len(encoded_data)] = np.frombuffer(encoded_data, dtype=np.uint8)
                    output_idx += len(encoded_data)
                else:
                    # Not enough space, put frame back in buffer (efficient list prepend)
                    self.sample_buffer = frame_samples.flatten().tolist() + self.sample_buffer
                    break
            except Exception as e:
                print(f"Opus encoding error: {e}")
                break
        
        # For sync_block, we must consume all input
        # Return number of output items produced
        return output_idx
    
    def __del__(self):
        """Cleanup method to release Opus encoder resources"""
        if hasattr(self, 'encoder'):
            # opuslib objects should be automatically cleaned up by Python,
            # but we explicitly clear the reference
            self.encoder = None

