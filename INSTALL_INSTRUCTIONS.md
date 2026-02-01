# Installation Instructions for gr-opus

## Quick Install (with sudo)

Run these commands from the project root directory:

```bash
cd build
sudo make install
sudo ldconfig
```

## Verify Installation

After installation, verify the module can be imported:

```bash
python3 -c "from gnuradio import gr_opus; print('gr-opus installed successfully!')"
```

## Make GRC Blocks Visible

After installation, restart GNU Radio Companion:

```bash
gnuradio-companion
```

The blocks should appear under the `[gr-opus]` category in the block tree.

## If Blocks Don't Appear

1. **Check GRC block location:** Blocks install to GNU Radio's share path (detected at configure time):
   ```bash
   ls /usr/local/share/gnuradio/grc/blocks/gr_opus_*.block.yml
   # or if GNU Radio from packages:
   ls /usr/share/gnuradio/grc/blocks/gr_opus_*.block.yml
   ```

2. **Ensure install path matches GRC search:** CMake uses `pkg-config --variable=prefix gnuradio-runtime` so blocks go where GRC looks. If using a custom prefix, add to `~/.gnuradio/config.conf`:
   ```ini
   [grc]
   global_blocks_path = /path/to/share/gnuradio/grc/blocks
   ```
   Or set `GRC_BLOCKS_PATH` to include your block directory:
   ```bash
   export GRC_BLOCKS_PATH=/path/to/share/gnuradio/grc/blocks:$GRC_BLOCKS_PATH
   ```

3. **Check Python module:**
   ```bash
   python3 -c "from gnuradio import gr_opus; print(gr_opus.__file__)"
   ```

4. **Clear GRC cache:**
   ```bash
   rm -rf ~/.cache/gnuradio/grc/cache_v2.json
   ```
   Then restart GRC.

5. **Verify GRC block paths:**
   ```bash
   python3 -c "
   from gnuradio.grc.core.Config import Config
   from gnuradio import gr
   c = Config(gr.version(), prefs=gr.prefs())
   print('Block paths:', c.block_paths)
   "
   ```

## Troubleshooting

### SWIG / gnuradio.i warnings

If CMake reports "gnuradio.i not in GNU Radio install - using bundled swig/gnuradio.i", this is normal. GNU Radio 3.9+ often omits SWIG interface files. gr-opus includes a minimal `gnuradio.i` fallback, so the build succeeds. No action needed.

### Module import errors

If you see import errors about missing symbols, ensure libraries are properly linked:
```bash
sudo ldconfig
```

### GRC still doesn't show blocks

1. Check that the block YAML files are valid:
   ```bash
   python3 -c "import yaml; yaml.safe_load(open('/usr/share/gnuradio/grc/blocks/gr_opus_opus_encoder.block.yml'))"
   ```

2. Check GRC logs:
   ```bash
   gnuradio-companion 2>&1 | grep -i opus
   ```

3. Verify the Python module exports the blocks:
   ```bash
   python3 -c "from gnuradio import gr_opus; print(dir(gr_opus))"
   ```
   Should show: `['opus_decoder', 'opus_encoder']`
