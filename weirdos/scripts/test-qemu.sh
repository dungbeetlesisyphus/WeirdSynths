#!/bin/bash
# ═══════════════════════════════════════════════════════════
# WeirdOS — Test in QEMU (aarch64 emulation)
# Boots the image without real Pi hardware
# ═══════════════════════════════════════════════════════════

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$(dirname "$SCRIPT_DIR")/build"
OUTPUT_DIR="${BUILD_DIR}/output"

KERNEL="${OUTPUT_DIR}/images/Image"
ROOTFS="${OUTPUT_DIR}/images/rootfs.ext4"
DTB="${OUTPUT_DIR}/images/bcm2712-rpi-5-b.dtb"

if [ ! -f "$KERNEL" ]; then
    echo "ERROR: Kernel not found. Run build.sh first."
    exit 1
fi

if ! command -v qemu-system-aarch64 &> /dev/null; then
    echo "ERROR: qemu-system-aarch64 not found."
    echo "Install: brew install qemu (macOS) or apt install qemu-system-arm (Linux)"
    exit 1
fi

echo "╔══════════════════════════════════════╗"
echo "║  WeirdOS — QEMU Test Boot            ║"
echo "╚══════════════════════════════════════╝"
echo ""
echo "NOTE: Graphics and camera won't work in QEMU."
echo "This tests boot sequence, init scripts, and CLI tools."
echo ""
echo "Login: root / weird"
echo "Exit:  Ctrl-A X"
echo ""

qemu-system-aarch64 \
    -M virt \
    -cpu cortex-a76 \
    -m 2G \
    -smp 4 \
    -kernel "$KERNEL" \
    -append "root=/dev/vda console=ttyAMA0" \
    -drive file="$ROOTFS",format=raw,if=virtio \
    -nographic \
    -net nic -net user,hostfwd=tcp::2222-:22
