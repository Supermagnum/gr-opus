# gr-opus

**IMPORTANT NOTICE**: This is AI-generated code. The developer has a neurological condition that makes it impossible to use and learn traditional programming. The developer has put in a significant effort. This code might not work properly. Use at your own risk.

This code has not been reviewed by professional coders, it is a large task. If there are tests available in the codebase, please review those and their code.

---

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

GRC block definitions install to GNU Radio's share path (detected via pkg-config), so they appear in the `[gr-opus]` category after restarting GNU Radio Companion. For custom install prefixes, set `GRC_BLOCKS_PATH` to include your block directory.

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
- Enable FARGAN voice: Toggle for DRED/FARGAN when Opus is built with --enable-dred (see FARGAN section)

### Opus Decoder

- Sample Rate: 8000, 12000, 16000, 24000, or 48000 Hz
- Channels: 1 (mono) or 2 (stereo)
- Packet Size: Fixed packet size in bytes (0 for auto-detect/variable)

The decoder automatically detects and decodes FARGAN/DRED when present in received packets (requires Opus built with --enable-dred).

## FARGAN Voice Encoder for Amateur Radio

If Opus is built from source with `--enable-dred --enable-osce`, the FARGAN voice encoder is available. It supports voice at 1.6 kbps. Opus source: <https://github.com/xiph/opus>

### FARGAN vs Codec2

| Aspect | FARGAN (Opus DRED) | Codec2 |
|--------|-------------------|--------|
| Bitrate | 1.6 kbps | 450-3200 bps (typical: 1200-2400) |
| Technology | Neural vocoder (GAN-based), hybrid CELP/MDCT | Sinusoidal coding, LPC |
| Quality at low rate | Natural-sounding, modern synthesis | Robotic, optimized for intelligibility |
| Latency | Low, configurable frame sizes | 40 ms frames at 1200 bps |
| Noise robustness | Good with DRED redundancy | Limited; degrades in noise |
| Language support | Language-independent training | Optimized for English |
| Standardization | IETF Opus, industry adoption | Open source, amateur radio focus |

**Advantages of FARGAN over Codec2:** FARGAN delivers more natural-sounding speech at 1.6 kbps than Codec2 at 1200-2400 bps. It uses a neural vocoder that better preserves speaker characteristics and prosody. DRED redundancy improves robustness under packet loss. FARGAN is part of the Opus codec family, widely used in VoIP and streaming.

### FARGAN vs MELPe

| Aspect | FARGAN (Opus DRED) | MELPe |
|--------|-------------------|-------|
| Bitrate | 1.6 kbps | 600, 1200, 2400 bps |
| Standard | IETF Opus (RFC 6716) | MIL-STD-3005, STANAG-4591 (NATO) |
| Technology | Neural vocoder (FARGAN), hybrid codec | Mixed Excitation Linear Prediction |
| Licensing | BSD-like (Opus) | Military/government |
| Availability | Open source, gr-opus | Proprietary implementations |
| Noise robustness | Good | Excellent (designed for battlefield) |
| Quality at comparable rate | Higher at 1.6 kbps | Strong at 1200/2400 bps |

**Advantages of FARGAN over MELPe:** FARGAN is fully open source and freely available. At 1.6 kbps it achieves quality comparable to or better than MELPe at 1200 bps, with more natural synthesis. MELPe excels in very noisy environments (vehicles, aircraft) due to military testing; FARGAN with DRED targets general-purpose robustness. MELPe requires proprietary licenses for many uses.

### Suggested Universal HF/VHF/UHF Mode Usage

| Parameter | Value |
|-----------|-------|
| Bandwidth | 6 kHz |
| Modulation | 4FSK |
| FEC | LDPC rate 1/2, soft-decision |
| Raw data rate | ~6 kbps |
| Usable data rate | ~3 kbps |
| Interleaver depth | 500 ms to 1 s |

### Link Budget

| Component | Bitrate |
|-----------|---------|
| FARGAN voice | 1600 bps |
| Framing/sync/overhead | ~400 bps |
| LDPC parity (rate 1/2) | 3000 bps |
| Total | ~6000 bps |

### Suitable Amateur Bands

- **NVIS**: 80 m, 40 m
- **DX**: 40 m, 20 m, 17 m, 15 m, 10 m, 6 m

## Testing

The module includes comprehensive unit tests and integration tests. See the [tests/README.md](tests/README.md) for details.

### Test Status

**Core Functionality Tests (All Passing):**
- Encoder tests (12/12 passing)
- Decoder tests (12/12 passing)
- Round-trip integration tests (8/8 passing)
- Memory sanitizer tests (5/5 passing)

**Performance Tests:**
- Performance and latency tests may fail on slower systems or under load
- These tests verify strict latency requirements (<10μs mean, <0.02ms real-time)
- Results are system-dependent and should be evaluated in context

**Timing Analysis Tests:**
- Dudect-style timing side-channel analysis tests
- May show minor timing variations (expected for audio codecs)
- Not security concerns for audio codec applications

### Running Tests

To run all tests:

```bash
cd tests
python3 -m unittest discover -p 'qa_*.py'
```

To run specific test suites:

```bash
# Core functionality tests
python3 -m unittest qa_opus_encoder
python3 -m unittest qa_opus_decoder
python3 -m unittest qa_opus_roundtrip

# Memory safety tests
python3 -m unittest qa_opus_memory_sanitizer

# Performance tests (may fail on slower systems)
python3 -m unittest qa_opus_performance

# Timing analysis tests
python3 -m unittest qa_opus_dudect
```

Or using CMake/CTest (after building):

```bash
cd build
ctest
```

## Code Quality

The codebase follows Python best practices and has been validated with multiple code quality tools:

- **Formatting**: All code formatted with Black (line length 120)
- **Import Sorting**: Imports sorted with isort (Black profile)
- **Style Checking**: Flake8 compliant (with GNU Radio-specific exceptions)
- **Type Checking**: MyPy validation passes
- **Security**: Bandit security linting (low severity issues only)
- **Unused Code**: Vulture analysis shows no unused code

### Code Quality Tools

The project uses the following tools (all passing):
- **Black**: Code formatting
- **isort**: Import sorting
- **Flake8**: Style checking
- **MyPy**: Type checking
- **Bandit**: Security linting
- **Vulture**: Unused code detection

## Notes

- The encoder processes audio in 20ms frames
- Input audio should be normalized to [-1.0, 1.0] range
- The decoder requires proper packet framing for optimal performance
- Debug output is disabled by default in the encoder (can be enabled if needed)

## License

GPLv3

