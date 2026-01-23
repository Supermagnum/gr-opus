#!/bin/bash
# Manual installation script for gr-opus (without sudo)

set -e

# Get Python version
PYTHON_VERSION=$(python3 -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
PYTHON_SITE_PACKAGES=$(python3 -c "import site; print(site.getsitepackages()[0])")

# Installation directories
INSTALL_PREFIX="${HOME}/.local"
GR_PYTHON_DIR="${PYTHON_SITE_PACKAGES}"
GR_LIB_DIR="${INSTALL_PREFIX}/lib"
GR_INCLUDE_DIR="${INSTALL_PREFIX}/include"
GRC_BLOCKS_DIR="${INSTALL_PREFIX}/share/gnuradio/grc/blocks"

# Build directory
BUILD_DIR="build"

echo "Installing gr-opus to ${INSTALL_PREFIX}..."

# Create directories
mkdir -p "${GR_PYTHON_DIR}/gr_opus"
mkdir -p "${GR_LIB_DIR}"
mkdir -p "${GR_INCLUDE_DIR}/gnuradio/gr_opus"
mkdir -p "${GRC_BLOCKS_DIR}"

# Copy Python module
echo "Copying Python module..."
cp -v "${BUILD_DIR}/swig/_gr_opus_swig.so" "${GR_PYTHON_DIR}/gr_opus/"
cp -v "${BUILD_DIR}/swig/_gr_opus_swig.py" "${GR_PYTHON_DIR}/gr_opus/" 2>/dev/null || true
cp -v python/__init__.py "${GR_PYTHON_DIR}/gr_opus/"

# Copy C++ library
echo "Copying C++ library..."
cp -v "${BUILD_DIR}/lib/libgnuradio-gr_opus.so" "${GR_LIB_DIR}/"

# Copy headers
echo "Copying headers..."
cp -v include/gnuradio/gr_opus/*.h "${GR_INCLUDE_DIR}/gnuradio/gr_opus/"

# Copy GRC blocks
echo "Copying GRC blocks..."
cp -v grc/*.block.yml "${GRC_BLOCKS_DIR}/"

echo ""
echo "Installation complete!"
echo ""
echo "Add these to your ~/.bashrc or ~/.profile:"
echo ""
echo "export PYTHONPATH=\"${GR_PYTHON_DIR}:\$PYTHONPATH\""
echo "export LD_LIBRARY_PATH=\"${GR_LIB_DIR}:\$LD_LIBRARY_PATH\""
echo "export GRC_BLOCKS_PATH=\"${GRC_BLOCKS_DIR}:\$GRC_BLOCKS_PATH\""
echo ""
echo "Then run: source ~/.bashrc"
echo "And restart GNU Radio Companion"
