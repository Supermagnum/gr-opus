# gr-opus

GNU Radio Out-of-Tree (OOT) module for Opus audio codec support.

## Description

gr-opus provides GNU Radio blocks for encoding and decoding Opus audio. The module uses Python blocks that interface with the Opus library through Python bindings.

## Why Opus Over Codec2?

Codec2 is a low-bitrate speech codec released in 2010, optimized for very low bandwidth (typically 1200-3200 bps). While effective for its era, codec2 has limitations:

- **Limited Audio Quality**: Designed for intelligibility over quality, resulting in robotic-sounding audio
- **Aging Technology**: Based on older speech coding techniques
- **Fixed Bitrates**: Limited flexibility in bitrate selection
- **Made for HF, not VHF bands**

Opus, standardized in 2012, represents a significant advancement:

- **Superior Quality**: Modern hybrid codec combining CELP and MDCT techniques
- **Adaptive Bitrate**: Supports bitrates from 6 kbps to 510 kbps with excellent quality at low rates
- **Low Latency**: Configurable frame sizes (2.5ms to 60ms) suitable for real-time communication
- **Robust Error Handling**: Better performance under adverse channel conditions
- **Wide Industry Adoption**: Used in modern VoIP, streaming, and communication systems

At comparable bitrates (e.g., 6-8 kbps), Opus provides noticeably better audio quality than codec2 while maintaining low latency suitable for ham radio applications.

## Features

- Opus audio encoder block
- Opus audio decoder block
- Support for multiple sample rates (8kHz, 12kHz, 16kHz, 24kHz, 48kHz)
- Mono and stereo support
- Configurable bitrate and application type

## Dependencies

### System Dependencies

- GNU Radio 3.8 or later
- Opus library development files
- CMake 3.8 or later

### Python Dependencies

- numpy
- opuslib (Python bindings for Opus)

Install Python dependencies:
```bash
pip install numpy opuslib
```

On Ubuntu/Debian, install system dependencies:
```bash
sudo apt-get install gnuradio-dev libopus-dev cmake
```

## Building and Installation

```bash
mkdir build
cd build
cmake ..
make
sudo make install
sudo ldconfig
```

## Usage

### Python

```python
from gnuradio import gr, gr_opus

# Create encoder
encoder = gr_opus.opus_encoder(
    sample_rate=48000,
    channels=1,
    bitrate=64000,
    application='audio'
)

# Create decoder
decoder = gr_opus.opus_decoder(
    sample_rate=48000,
    channels=1,
    packet_size=0  # 0 for auto-detect, or specify fixed packet size
)
```

### GNU Radio Companion (GRC)

The blocks are available in GRC under the `[gr-opus]` category:
- Opus Encoder
- Opus Decoder

## Block Parameters

### Opus Encoder

- Sample Rate: 8000, 12000, 16000, 24000, or 48000 Hz
- Channels: 1 (mono) or 2 (stereo)
- Bitrate: Target bitrate in bits per second
- Application Type: 'voip', 'audio', or 'lowdelay'

### Opus Decoder

- Sample Rate: 8000, 12000, 16000, 24000, or 48000 Hz
- Channels: 1 (mono) or 2 (stereo)
- Packet Size: Fixed packet size in bytes (0 for auto-detect/variable)

## Testing

The module includes comprehensive unit tests and integration tests. See the [tests/README.md](tests/README.md) for details.

To run tests:

```bash
cd build
ctest
```

Or run tests directly:

```bash
cd tests
python3 -m unittest discover -p 'qa_*.py'
```

## Notes

- The encoder processes audio in 20ms frames
- Input audio should be normalized to [-1.0, 1.0] range
- The decoder requires proper packet framing for optimal performance

## License

GPLv3

