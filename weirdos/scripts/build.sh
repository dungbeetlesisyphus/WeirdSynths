#!/bin/bash
# ═══════════════════════════════════════════════════════════
# WeirdOS v0.1.0 — Build Script
# Builds a bootable SD card image for Raspberry Pi 5
# ═══════════════════════════════════════════════════════════

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
WEIRDOS_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${WEIRDOS_DIR}/build"
BUILDROOT_VERSION="2024.02.3"
BUILDROOT_URL="https://buildroot.org/downloads/buildroot-${BUILDROOT_VERSION}.tar.xz"
BUILDROOT_DIR="${BUILD_DIR}/buildroot-${BUILDROOT_VERSION}"
OUTPUT_DIR="${BUILD_DIR}/output"

echo "╔══════════════════════════════════════╗"
echo "║  WeirdOS v0.1.0 — Build System       ║"
echo "╚══════════════════════════════════════╝"
echo ""

# ── Check dependencies ──
echo "[1/6] Checking build dependencies..."
for cmd in make gcc g++ wget tar patch cpio unzip rsync python3 bc; do
    if ! command -v $cmd &> /dev/null; then
        echo "ERROR: Missing dependency: $cmd"
        echo "Install with: sudo apt install build-essential wget bc python3 cpio unzip rsync"
        exit 1
    fi
done
echo "  ✓ All dependencies found"

# ── Download Buildroot ──
echo "[2/6] Setting up Buildroot ${BUILDROOT_VERSION}..."
mkdir -p "$BUILD_DIR"

if [ ! -d "$BUILDROOT_DIR" ]; then
    echo "  Downloading Buildroot..."
    wget -q --show-progress -O "${BUILD_DIR}/buildroot.tar.xz" "$BUILDROOT_URL"
    echo "  Extracting..."
    tar xf "${BUILD_DIR}/buildroot.tar.xz" -C "$BUILD_DIR"
    rm "${BUILD_DIR}/buildroot.tar.xz"
    echo "  ✓ Buildroot ready"
else
    echo "  ✓ Buildroot already present"
fi

# ── Configure ──
echo "[3/6] Configuring for Raspberry Pi 5..."
cd "$BUILDROOT_DIR"

# Set BR2_EXTERNAL to our custom packages
export BR2_EXTERNAL="${WEIRDOS_DIR}/buildroot/external"

# Load our defconfig
make O="$OUTPUT_DIR" BR2_EXTERNAL="$BR2_EXTERNAL" \
    weirdos_rpi5_defconfig

echo "  ✓ Configuration applied"

# ── Build ──
echo "[4/6] Building WeirdOS image (this takes 1-3 hours on first run)..."
echo "  CPU cores: $(nproc)"
echo "  Output: ${OUTPUT_DIR}"
echo ""

make O="$OUTPUT_DIR" BR2_EXTERNAL="$BR2_EXTERNAL" \
    -j$(nproc) 2>&1 | tee "${BUILD_DIR}/build.log"

echo ""
echo "  ✓ Build complete"

# ── Generate SD card image ──
echo "[5/6] Generating SD card image..."

IMAGE_FILE="${OUTPUT_DIR}/images/weirdos-v0.1.0-rpi5.img"
if [ -f "${OUTPUT_DIR}/images/sdcard.img" ]; then
    cp "${OUTPUT_DIR}/images/sdcard.img" "$IMAGE_FILE"
    echo "  ✓ Image: ${IMAGE_FILE}"
else
    echo "  WARNING: sdcard.img not found, checking for rootfs..."
    ls "${OUTPUT_DIR}/images/"
fi

# ── Summary ──
echo "[6/6] Build summary"
echo ""
echo "═══════════════════════════════════════"
echo "WeirdOS v0.1.0 build complete!"
echo ""
if [ -f "$IMAGE_FILE" ]; then
    SIZE=$(du -h "$IMAGE_FILE" | cut -f1)
    echo "Image:  ${IMAGE_FILE}"
    echo "Size:   ${SIZE}"
    echo ""
    echo "Flash to SD card:"
    echo "  ./scripts/flash.sh ${IMAGE_FILE} /dev/sdX"
fi
echo "═══════════════════════════════════════"
