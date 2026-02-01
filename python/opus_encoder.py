#!/usr/bin/env python3
"""
GNU Radio block for Opus audio encoding
"""

import numpy as np
import opuslib
from gnuradio import gr


class opus_encoder(gr.sync_block):
    """
    Opus audio encoder block

    Encodes PCM audio samples to Opus format.
    Input: Float32 audio samples (mono or stereo)
    Output: Bytes containing Opus-encoded packets
    """

    def __init__(self, sample_rate=48000, channels=1, bitrate=64000, application="audio",
                 enable_fargan_voice=False, dnn_blob_path=""):
        """
        Initialize Opus encoder

        Args:
            sample_rate: Input sample rate (8000, 12000, 16000, 24000, or 48000 Hz)
            channels: Number of channels (1 for mono, 2 for stereo)
            bitrate: Target bitrate in bits per second
            application: Opus application type ('voip', 'audio', or 'lowdelay')
            enable_fargan_voice: Ignored in Python fallback (C++ DRED only)
            dnn_blob_path: Ignored in Python fallback (C++ DRED only)
        """
        gr.sync_block.__init__(self, name="opus_encoder", in_sig=[np.float32], out_sig=[np.uint8])

        self.sample_rate = sample_rate
        self.channels = channels
        self.bitrate = bitrate

        # Map application string to opuslib constant
        app_map = {
            "voip": opuslib.APPLICATION_VOIP,
            "audio": opuslib.APPLICATION_AUDIO,
            "lowdelay": opuslib.APPLICATION_RESTRICTED_LOWDELAY,
        }
        self.application = app_map.get(application.lower(), opuslib.APPLICATION_AUDIO)

        # Create Opus encoder
        # Store encoder reference to prevent garbage collection
        try:
            self.encoder = opuslib.Encoder(sample_rate, channels, self.application)
            self.encoder.bitrate = bitrate
            # Ensure encoder is not None
            if self.encoder is None:
                raise RuntimeError("Failed to create Opus encoder")
        except Exception as e:
            raise RuntimeError(f"Failed to initialize Opus encoder: {e}")

        # Frame size in samples (20ms frames for good quality)
        self.frame_size = int(sample_rate * 0.020)  # 20ms frames

        # Buffer for accumulating samples (use list for efficient appending)
        self.sample_buffer = []

        # Maximum buffer size (10 seconds of audio) to prevent memory leaks
        self.max_buffer_samples = sample_rate * channels * 10

        # Convert float32 samples to int16 for Opus
        self.max_int16 = 32767.0

        # Store reference to self to prevent garbage collection issues
        # This helps prevent NoneType errors when GNU Radio gateway accesses the block
        self._self_ref = self

        # Store references to critical methods to ensure they're always accessible
        # This prevents the gateway from getting None when accessing these methods
        self._forecast_ref = self.forecast
        self._work_ref = self.work

        # Store instance in class-level list to prevent garbage collection
        # This ensures the Python object stays alive even if local references are cleared
        if not hasattr(type(self), "_instances"):
            type(self)._instances = []
        type(self)._instances.append(self)

        # Store all critical references in a dict to prevent GC
        self._refs = {
            "self": self,
            "forecast": self.forecast,
            "work": self.work,
            "gateway": self.gateway,
            "encoder": self.encoder,
        }

    # Do not override forecast - sync_blocks handle forecasting internally
    # The parent gr.sync_block.forecast method handles this automatically
    # Overriding it causes NoneType casting errors in GNU Radio's gateway code

    def work(self, input_items, output_items):
        """
        Process audio samples and encode to Opus
        """
        in0 = input_items[0]
        out = output_items[0]

        noutput = len(out)

        # Add new samples to buffer (efficient list append)
        self.sample_buffer.extend(in0.tolist())

        # Prevent unbounded buffer growth (memory leak protection)
        if len(self.sample_buffer) > self.max_buffer_samples:
            # Keep only the most recent samples (drop oldest)
            excess = len(self.sample_buffer) - self.max_buffer_samples
            self.sample_buffer = self.sample_buffer[excess:]

        output_idx = 0
        frame_size_samples = self.frame_size * self.channels
        frames_encoded = 0

        # Process complete frames
        while len(self.sample_buffer) >= frame_size_samples and output_idx < noutput:
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
                if output_idx + len(encoded_data) <= noutput:
                    out[output_idx : output_idx + len(encoded_data)] = np.frombuffer(encoded_data, dtype=np.uint8)
                    output_idx += len(encoded_data)
                    frames_encoded += 1
                else:
                    # Not enough space, put frame back in buffer (efficient list prepend)
                    self.sample_buffer = frame_samples.flatten().tolist() + self.sample_buffer
                    break
            except Exception as e:
                print(f"Opus encoding error: {e}")
                import traceback

                traceback.print_exc()
                break

        # Debug output disabled by default (can be enabled by setting _debug_enabled = True)
        # if frames_encoded > 0 and not hasattr(self, "_debug_count"):
        #     self._debug_count = 0
        # if frames_encoded > 0:
        #     self._debug_count = getattr(self, "_debug_count", 0) + 1
        #     if self._debug_count % 10 == 0:  # Print every 10th frame
        #         print(
        #             f"Opus encoder: Encoded {frames_encoded} frames, "
        #             f"produced {output_idx} bytes, buffer has {len(self.sample_buffer)} samples"
        #         )

        # For sync_block, we must consume all input
        # Return number of output items produced
        return output_idx

    def __del__(self):
        """Cleanup method to release Opus encoder resources"""
        # Remove from class instances list
        if hasattr(type(self), "_instances"):
            try:
                type(self)._instances.remove(self)
            except (ValueError, AttributeError):
                pass

        if hasattr(self, "encoder"):
            # opuslib objects should be automatically cleaned up by Python,
            # but we explicitly clear the reference
            self.encoder = None
