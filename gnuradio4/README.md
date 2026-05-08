# gr-opus for GNU Radio 4.0

Header-only C++ blocks and CMake packaging for the GNU Radio 4 runtime. The classic GNU Radio 3.10 tree on `main` is unchanged; this port lives on the **`gnuradio4`** branch under this directory.

## Blocks

| Type | Role |
|------|------|
| `gnuradio4::opus::OpusEncoderCc` | Float PCM in, one Opus frame per `processBulk`, PDU out (`pdu_bytes` tensor on the message) |
| `gnuradio4::opus::OpusDecoderCc` | PDUs in on `pdu_in`, float PCM frame out |

The `Cc` suffix avoids a name clash with libopus C types `OpusEncoder` / `OpusDecoder` in `opus.h`.

## Requirements

- Installed [GNU Radio 4](https://www.gnuradio.org/) (e.g. prefix `/opt/gnuradio4-gcc`)
- **GCC 14+** (or another compiler with C++23 including `std::print`, as used by gnuradio-core headers)
- `pkg-config` and **libopus** (`opus` module)
- Optional: **Opus with DRED** (`OPUS_SET_DRED_DURATION_REQUEST` in headers) enables `enable_fargan` on the encoder

## Configure and build

```bash
cmake -S /path/to/gr-opus/gnuradio4 \
      -B /path/to/gr-opus/gnuradio4/build \
      -DCMAKE_PREFIX_PATH="/opt/gnuradio4-gcc" \
      -DCMAKE_BUILD_TYPE=Debug
cmake --build /path/to/gr-opus/gnuradio4/build -j"$(nproc)"
ctest --test-dir /path/to/gr-opus/gnuradio4/build --output-on-failure
```

If CMake picks an older default `g++`, pass `-DCMAKE_CXX_COMPILER=/usr/bin/g++-14` or set `CXX`.

## Install

```bash
cmake --install /path/to/gr-opus/gnuradio4/build --prefix /your/prefix
```

Consuming projects use `find_package(gr-opus4)` and link `gnuradio4::gr-opus`. A pkg-config file `gnuradio4-gr-opus.pc` is installed with `Requires: opus gnuradio4`.

## PDU format

Control messages use `gr::message::Command::Notify` and a `gr::property_map` entry:

- Key: `gr::convert_string_domain(std::string_view("pdu_bytes"))`
- Value: `gr::pmt::Value(gr::Tensor<std::uint8_t>(...))`

## Tests

Boost.UT binaries `qa_OpusEncoder` and `qa_OpusDecoder` call `init(std::make_shared<gr::Sequence>())` and `start()` before exercising the blocks; there is no scheduler-based flowgraph in the tests.

## Branch

Develop and build this port from the repository branch **`gnuradio4`**:  
https://github.com/Supermagnum/gr-opus/tree/gnuradio4
