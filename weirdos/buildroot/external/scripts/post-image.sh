#!/bin/bash
# ═══════════════════════════════════════════════════════════
# WeirdOS — Post-image script
# Generates the final SD card image using genimage
# ═══════════════════════════════════════════════════════════

set -e

BOARD_DIR="$(dirname "$0")"
GENIMAGE_CFG="${BOARD_DIR}/../board/rpi5/genimage.cfg"
GENIMAGE_TMP="${BUILD_DIR}/genimage.tmp"

# Use genimage to create partitioned SD card image
rm -rf "${GENIMAGE_TMP}"

genimage \
    --rootpath "${TARGET_DIR}" \
    --tmppath "${GENIMAGE_TMP}" \
    --inputpath "${BINARIES_DIR}" \
    --outputpath "${BINARIES_DIR}" \
    --config "${GENIMAGE_CFG}"

# Rename to our versioned filename
if [ -f "${BINARIES_DIR}/sdcard.img" ]; then
    cp "${BINARIES_DIR}/sdcard.img" "${BINARIES_DIR}/weirdos-v0.1.0-rpi5.img"
    echo "[post-image] SD card image: weirdos-v0.1.0-rpi5.img"
fi
