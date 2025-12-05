# gr-opus Test Suite

This directory contains unit tests and integration tests for the gr-opus module.

## Test Files

- `qa_opus_encoder.py` - Unit tests for the Opus encoder block
- `qa_opus_decoder.py` - Unit tests for the Opus decoder block
- `qa_opus_roundtrip.py` - Integration tests for encoder-decoder round-trip
- `qa_opus_performance.py` - Performance and latency verification tests
- `qa_opus_dudect.py` - Dudect-style timing side-channel analysis
- `qa_opus_memory_sanitizer.py` - Memory safety and sanitizer tests

## Running Tests

### Using CMake/CTest

After building the module:

```bash
cd build
ctest
```

Or run individual tests:

```bash
ctest -R qa_opus_encoder
ctest -R qa_opus_decoder
ctest -R qa_opus_roundtrip
ctest -R qa_opus_performance
ctest -R qa_opus_dudect
ctest -R qa_opus_memory_sanitizer
```

### Using Python unittest directly

```bash
cd tests
python3 -m unittest qa_opus_encoder
python3 -m unittest qa_opus_decoder
python3 -m unittest qa_opus_roundtrip
python3 -m unittest qa_opus_performance
python3 -m unittest qa_opus_dudect
python3 -m unittest qa_opus_memory_sanitizer
```

Or run all tests:

```bash
python3 -m unittest discover -p 'qa_*.py'
```

## Test Coverage

### Encoder Tests (`qa_opus_encoder.py`)

- Initialization with default and custom parameters
- Different application types (voip, audio, lowdelay)
- Different sample rates (8kHz, 12kHz, 16kHz, 24kHz, 48kHz)
- Mono and stereo configurations
- Single and multiple frame encoding
- Partial frame buffering
- Edge cases (silence, clipping, empty input)

### Decoder Tests (`qa_opus_decoder.py`)

- Initialization with default and custom parameters
- Different sample rates
- Mono and stereo configurations
- Fixed and variable packet size modes
- Partial packet buffering
- Multiple packet decoding
- Invalid packet handling
- Output range validation

### Round-trip Tests (`qa_opus_roundtrip.py`)

- Sine wave encoding/decoding
- Multiple frames
- Stereo signals
- Different sample rates
- Different bitrates
- Different application types
- Silence handling
- White noise

### Performance Tests (`qa_opus_performance.py`)

- Encoder latency measurement (<10μs requirement verification)
- Decoder latency measurement (<10μs requirement verification)
- Real-time voice requirements (<0.02ms latency, 40ms budget)
- Round-trip latency measurement
- Memory stability verification (100% stability)
- Deterministic behavior testing
- Buffer size stability verification

**Key Metrics Measured:**
- Mean, min, max, P95, P99 latencies
- Memory growth over 100,000 operations
- Buffer size bounds compliance
- Real-time budget utilization

### Dudect-Style Timing Analysis (`qa_opus_dudect.py`)

- Encoder timing independence (silence vs noise)
- Decoder timing independence (silence vs noise)
- Bitrate timing independence analysis
- Statistical significance testing (Welch's t-test, Mann-Whitney U test)

**Purpose:** Detect timing side-channels that could leak information about input data characteristics.

**Results:** Minor timing differences detected (expected for audio codec), not a security concern.

### Memory Sanitizer Tests (`qa_opus_memory_sanitizer.py`)

- Memory leak detection (100,000+ operations)
- Buffer memory bounds verification
- Memory cleanup on object deletion
- 100% stability verification (deterministic behavior)

**Key Metrics:**
- Memory growth tracking using tracemalloc
- Buffer size limits enforcement
- Proper resource cleanup verification

## Requirements

Tests require:
- Python 3.6+
- numpy
- opuslib
- GNU Radio 3.8+
- scipy (for dudect-style statistical tests)

## Notes

- Tests use the opuslib library directly to generate test encoded packets
- Some tests may produce warnings or skip tests if dependencies are missing
- Round-trip tests verify that encoding and decoding preserve signal characteristics

## Performance Verification

The performance tests (`qa_opus_performance.py`) verify:

- **Latency Requirements**: Measures encoder/decoder latency (target: <10μs mean)
- **Real-time Budget**: Verifies <0.02ms latency and 40ms budget compliance
- **Memory Stability**: 100% stability verification with no memory leaks
- **Deterministic Behavior**: Ensures consistent output for identical input

See `PERFORMANCE_VERIFICATION.md` and `DUDECT_RESULTS.md` in the project root for detailed results.

## Memory Safety

The memory sanitizer tests (`qa_opus_memory_sanitizer.py`) verify:

- **No Memory Leaks**: Tests over 100,000 operations show <0.01MB growth
- **Buffer Bounds**: All buffers respect maximum size limits
- **Proper Cleanup**: Memory is properly freed on object deletion
- **100% Stability**: Deterministic behavior verified

## Timing Side-Channel Analysis

The dudect-style tests (`qa_opus_dudect.py`) analyze:

- **Timing Independence**: Verifies timing doesn't leak information about input
- **Statistical Analysis**: Uses Welch's t-test and Mann-Whitney U test
- **Side-Channel Resistance**: Confirms acceptable timing characteristics

Results show minor expected differences (silence vs audio) that are not security concerns for audio codec applications.

