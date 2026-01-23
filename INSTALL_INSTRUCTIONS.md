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

1. **Check GRC block location:**
   ```bash
   ls /usr/share/gnuradio/grc/blocks/gr_opus_*.block.yml
   ```
   If using a custom install prefix, check:
   ```bash
   ls ${CMAKE_INSTALL_PREFIX}/share/gnuradio/grc/blocks/gr_opus_*.block.yml
   ```

2. **Check Python module:**
   ```bash
   python3 -c "from gnuradio import gr_opus; print(gr_opus.__file__)"
   ```

3. **Clear GRC cache:**
   ```bash
   rm -rf ~/.gnuradio/grc_cache
   ```
   Then restart GRC.

4. **Check GRC block search path:**
   ```bash
   python3 -c "from gnuradio import grc; import os; print(os.pathsep.join(grc.get_blocks_path()))"
   ```

## Troubleshooting

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
