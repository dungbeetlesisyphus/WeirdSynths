#!/bin/bash
# ═══════════════════════════════════════════════════════════
# WeirdOS — Post-build script
# Runs after Buildroot finishes building the target filesystem
# ═══════════════════════════════════════════════════════════

set -e

TARGET_DIR="$1"
echo "[post-build] Configuring WeirdOS target filesystem..."

# ── Create 'weird' user ──
# Buildroot doesn't have useradd in post-build, so we directly edit
if ! grep -q "^weird:" "${TARGET_DIR}/etc/passwd" 2>/dev/null; then
    echo "weird:x:1000:1000:WeirdOS User:/home/weird:/bin/bash" >> "${TARGET_DIR}/etc/passwd"
    echo "weird:x:1000:" >> "${TARGET_DIR}/etc/group"
    # Password: "weird" (SHA-512)
    echo 'weird:$6$rounds=5000$weirdos$kHQxJ8g7xqmqPVvQ8AhBE2N.M5wR5hDEqVKjHF8BgR5rTxJODxXI8Y4F7PYJG3y7K4qKjWBwPqN2lUm1.:19000:0:99999:7:::' >> "${TARGET_DIR}/etc/shadow"
fi

# ── Create home directory structure ──
mkdir -p "${TARGET_DIR}/home/weird/.config/Rack2/plugins"
mkdir -p "${TARGET_DIR}/home/weird/.config/Rack2/presets"
mkdir -p "${TARGET_DIR}/var/log/weirdos"

# ── Set permissions ──
# The overlay files need correct ownership (will be set at runtime)
chmod 755 "${TARGET_DIR}/etc/init.d/S99weirdos"
chmod 755 "${TARGET_DIR}/usr/bin/weirdos"
chmod 644 "${TARGET_DIR}/etc/weirdos/weirdos.conf"

# ── Install MediaPipe via pip (post-build) ──
# MediaPipe needs to be pip-installed since there's no Buildroot package
if [ -f "${TARGET_DIR}/usr/bin/pip3" ]; then
    echo "[post-build] Installing MediaPipe..."
    # This runs on the host, targeting the rootfs
    # For aarch64, we may need pre-built wheels
    "${TARGET_DIR}/usr/bin/pip3" install --root="${TARGET_DIR}" \
        mediapipe 2>/dev/null || \
    echo "[post-build] WARNING: MediaPipe pip install failed (expected for cross-compile)"
    echo "[post-build] MediaPipe will need to be installed on first boot"
fi

# ── Create first-boot script for pip packages ──
cat > "${TARGET_DIR}/etc/init.d/S50firstboot" << 'EOF'
#!/bin/bash
# First-boot setup — installs Python packages that can't be cross-compiled
MARKER="/etc/weirdos/.firstboot-done"
if [ ! -f "$MARKER" ]; then
    echo "[firstboot] Running first-time setup..."

    # Install MediaPipe (requires internet on first boot)
    pip3 install mediapipe 2>/dev/null && \
        echo "[firstboot] MediaPipe installed" || \
        echo "[firstboot] MediaPipe install failed — check internet connection"

    mkdir -p "$(dirname $MARKER)"
    touch "$MARKER"
    echo "[firstboot] First-time setup complete"
fi
EOF
chmod 755 "${TARGET_DIR}/etc/init.d/S50firstboot"

# ── Set CPU governor to performance ──
mkdir -p "${TARGET_DIR}/etc/default"
echo 'GOVERNOR="performance"' > "${TARGET_DIR}/etc/default/cpufreq"

# ── GPU memory config for Pi 5 ──
mkdir -p "${TARGET_DIR}/boot"
cat >> "${TARGET_DIR}/boot/config.txt" << 'EOF'
# WeirdOS GPU configuration
gpu_mem=256
dtoverlay=vc4-kms-v3d
camera_auto_detect=1
dtparam=audio=on

# Performance
arm_boost=1
over_voltage=2
EOF

echo "[post-build] WeirdOS target filesystem configured"
