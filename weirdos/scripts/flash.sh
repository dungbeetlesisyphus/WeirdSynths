#!/bin/bash
# ═══════════════════════════════════════════════════════════
# WeirdOS — Flash image to SD card
# Usage: ./flash.sh <image.img> <device>
# Example: ./flash.sh build/output/images/weirdos-v0.1.0-rpi5.img /dev/disk4
# ═══════════════════════════════════════════════════════════

set -e

IMAGE="$1"
DEVICE="$2"

if [ -z "$IMAGE" ] || [ -z "$DEVICE" ]; then
    echo "Usage: $0 <image.img> <device>"
    echo ""
    echo "Example (macOS): $0 weirdos.img /dev/disk4"
    echo "Example (Linux): $0 weirdos.img /dev/sdb"
    echo ""
    echo "WARNING: This will ERASE the target device!"
    echo ""
    echo "To find your SD card device:"
    echo "  macOS:  diskutil list"
    echo "  Linux:  lsblk"
    exit 1
fi

if [ ! -f "$IMAGE" ]; then
    echo "ERROR: Image not found: $IMAGE"
    exit 1
fi

if [ ! -b "$DEVICE" ]; then
    echo "ERROR: Not a block device: $DEVICE"
    exit 1
fi

# Safety check — don't flash system disk
case "$DEVICE" in
    /dev/sda|/dev/nvme0n1|/dev/disk0|/dev/disk1)
        echo "ERROR: Refusing to flash what looks like a system disk: $DEVICE"
        exit 1
        ;;
esac

SIZE=$(du -h "$IMAGE" | cut -f1)
echo "╔══════════════════════════════════════╗"
echo "║  WeirdOS — Flash to SD Card          ║"
echo "╚══════════════════════════════════════╝"
echo ""
echo "Image:   $IMAGE ($SIZE)"
echo "Target:  $DEVICE"
echo ""
echo "⚠  WARNING: ALL DATA ON $DEVICE WILL BE ERASED!"
echo ""
read -p "Continue? (type 'yes' to confirm): " CONFIRM

if [ "$CONFIRM" != "yes" ]; then
    echo "Cancelled."
    exit 0
fi

# Unmount any partitions on the device
echo "[1/3] Unmounting device..."
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    diskutil unmountDisk "$DEVICE" 2>/dev/null || true
    RAW_DEVICE="${DEVICE/disk/rdisk}"
    echo "  Using raw device: $RAW_DEVICE"
else
    # Linux
    for part in ${DEVICE}*; do
        umount "$part" 2>/dev/null || true
    done
    RAW_DEVICE="$DEVICE"
fi

# Flash
echo "[2/3] Writing image (this takes a few minutes)..."
if [[ "$OSTYPE" == "darwin"* ]]; then
    sudo dd if="$IMAGE" of="$RAW_DEVICE" bs=4m status=progress
    sudo sync
else
    sudo dd if="$IMAGE" of="$RAW_DEVICE" bs=4M status=progress conv=fsync
fi

# Eject
echo "[3/3] Ejecting..."
if [[ "$OSTYPE" == "darwin"* ]]; then
    diskutil eject "$DEVICE"
else
    sudo eject "$DEVICE" 2>/dev/null || true
fi

echo ""
echo "═══════════════════════════════════════"
echo "✓ WeirdOS flashed successfully!"
echo ""
echo "Insert the SD card into your Pi 5 and power on."
echo "SSH access: ssh weird@weirdos.local (password: weird)"
echo "═══════════════════════════════════════"
