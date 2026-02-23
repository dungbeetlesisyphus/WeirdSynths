#!/usr/bin/env python3
"""
WeirdSynths Kinect Bridge
=========================
Unified depth tracking bridge supporting:
  - Kinect 360 / Kinect for Windows v1 (via freenect / libfreenect)
  - Kinect One / Kinect for Windows v2 (via pylibfreenect2 / libfreenect2)
  - Azure Kinect DK (via pyk4a / Azure Kinect SDK)

Sends two UDP packet types:
  Port 9005 — KINT depth CV packet (8 normalized CV values, always)
  Port 9006 — SKEL skeleton packet (joint positions, Azure/NiTE2 only)

KINT Protocol v1 — 48 bytes
  Bytes  0-3:  Magic "KINT"
  Bytes  4-5:  Version uint16 LE = 1
  Byte   6:    Source  uint8  (0=K360, 1=KOne, 2=Azure, 3=Simulated)
  Byte   7:    BodyCount uint8 (0-4)
  Bytes  8-39: 8 × float32 LE (CV values, see below)
  Bytes 40-47: Timestamp uint64 LE (microseconds)

CV Values (indices 0-7):
  0  DIST    nearest foreground depth, normalized 0-1 (1=closest, 0=far/empty)
  1  MOTION  frame-to-frame depth energy (motion detector), 0-1
  2  CNTX    horizontal centroid of depth field, -1 to +1 (left/right)
  3  CNTY    vertical centroid of depth field, -1 to +1 (up/down)
  4  AREA    foreground pixel fraction, 0-1 (body fills frame = 1)
  5  DPTHL   left zone mean depth, 0-1
  6  DPTHR   right zone mean depth, 0-1
  7  ENTR    depth field entropy / complexity, 0-1

SKEL Protocol v1 — variable length
  Bytes  0-3:  Magic "SKEL"
  Bytes  4-5:  Version uint16 LE = 1
  Byte   6:    BodyIndex uint8
  Byte   7:    JointCount uint8 (25=KOne/NiTE2, 32=Azure)
  Bytes  8-N:  JointCount × 3 × float32 LE (x, y, z in metres, normalized ±1)
  Bytes N+0-7: Timestamp uint64 LE

Usage:
  pip install numpy
  # Kinect 360:  pip install freenect
  # Kinect One:  pip install pylibfreenect2
  # Azure:       pip install pyk4a  (+ Azure Kinect SDK installed)

  python kinect_bridge.py [--device auto|k360|kone|azure] [--host 127.0.0.1]
                          [--depth-port 9005] [--skel-port 9006]
                          [--near 0.5] [--far 4.0] [--show]
"""

from __future__ import annotations

import argparse
import importlib
import socket
import struct
import sys
import time
from typing import Optional

import numpy as np

# ──────────────────────────────────────────────────────────
# Constants
# ──────────────────────────────────────────────────────────

KINT_MAGIC   = b'KINT'
SKEL_MAGIC   = b'SKEL'
KINT_VERSION = 1
SKEL_VERSION = 1

SOURCE_K360  = 0
SOURCE_KONE  = 1
SOURCE_AZURE = 2
SOURCE_SIM   = 3

SOURCE_NAMES = {SOURCE_K360: 'Kinect 360', SOURCE_KONE: 'Kinect One', SOURCE_AZURE: 'Azure Kinect', SOURCE_SIM: 'Simulated'}

# Azure Kinect joint IDs (K4ABT_JOINT_*)
AZURE_JOINT_COUNT = 32
# Kinect One / NiTE2 joint count
KONE_JOINT_COUNT  = 25

# Kinect 360 depth range (mm, after raw → mm conversion)
K360_NEAR_MM = 800.0
K360_FAR_MM  = 4000.0

# Kinect One / Azure depth range (mm, native)
KOA_NEAR_MM  = 500.0
KOA_FAR_MM   = 4500.0

# Motion detection: min depth change (mm) to count as motion
MOTION_THRESHOLD_MM = 20.0

# Foreground threshold: pixel is "foreground" if within NEAR_MM + offset
FG_OFFSET_MM = 1500.0


# ──────────────────────────────────────────────────────────
# Depth ↔ CV Feature Extraction
# ──────────────────────────────────────────────────────────

def extract_depth_cvs(depth_mm: np.ndarray,
                      prev_mm: Optional[np.ndarray],
                      near_mm: float,
                      far_mm: float) -> dict:
    """
    Extract 8 normalized CV values and body_count from a depth frame.

    depth_mm : H×W float32 array in millimetres (0 = invalid/no data)
    prev_mm  : previous frame (same shape), or None on first frame
    near_mm  : user-defined near clipping plane in mm
    far_mm   : user-defined far clipping plane in mm
    Returns  : dict with keys dist, motion, cntx, cnty, area, dpthl, dpthr, entr, body_count
    """
    h, w = depth_mm.shape

    # Mask: valid pixels within our working range
    valid = (depth_mm > near_mm) & (depth_mm < far_mm)
    valid_count = int(np.count_nonzero(valid))
    total = h * w

    if valid_count == 0:
        return dict(dist=0.0, motion=0.0, cntx=0.0, cnty=0.0,
                    area=0.0, dpthl=0.0, dpthr=0.0, entr=0.0, body_count=0)

    # Normalize depth to 0-1 (0 = near_mm, 1 = far_mm)
    norm = np.zeros_like(depth_mm)
    norm[valid] = (depth_mm[valid] - near_mm) / (far_mm - near_mm)
    norm = np.clip(norm, 0.0, 1.0)

    # --- DIST: nearest valid pixel → inverted so 1=very close, 0=far ---
    min_depth = float(np.min(depth_mm[valid]))
    dist = 1.0 - np.clip((min_depth - near_mm) / (far_mm - near_mm), 0.0, 1.0)

    # --- MOTION: frame-to-frame difference energy ---
    if prev_mm is not None:
        prev_valid = (prev_mm > near_mm) & (prev_mm < far_mm)
        both_valid = valid & prev_valid
        if np.any(both_valid):
            diff = np.abs(depth_mm[both_valid].astype(np.float32) -
                          prev_mm[both_valid].astype(np.float32))
            motion_energy = float(np.mean(diff > MOTION_THRESHOLD_MM))
            motion = float(np.clip(motion_energy * 4.0, 0.0, 1.0))  # boost sensitivity
        else:
            motion = 0.0
    else:
        motion = 0.0

    # --- CENTX / CNTY: weighted centroid of depth field ---
    # Weight = inverse depth (closer = heavier)
    weight = np.zeros_like(depth_mm)
    weight[valid] = 1.0 - norm[valid]
    w_total = float(np.sum(weight))

    if w_total > 1e-6:
        xs = np.arange(w, dtype=np.float32)
        ys = np.arange(h, dtype=np.float32)
        X, Y = np.meshgrid(xs, ys)
        cx = float(np.sum(X * weight)) / w_total
        cy = float(np.sum(Y * weight)) / w_total
        cntx = float(np.clip((cx / w) * 2.0 - 1.0, -1.0, 1.0))
        cnty = float(np.clip((cy / h) * 2.0 - 1.0, -1.0, 1.0))
    else:
        cntx = 0.0
        cnty = 0.0

    # --- AREA: fraction of frame that is "foreground" (within FG threshold) ---
    fg_thresh = near_mm + FG_OFFSET_MM
    fg_mask = (depth_mm > near_mm) & (depth_mm < min(fg_thresh, far_mm))
    area = float(np.count_nonzero(fg_mask)) / total

    # --- DPTHL / DPTHR: mean depth in left/right halves ---
    half = w // 2
    left_valid  = valid[:, :half]
    right_valid = valid[:, half:]
    left_depth  = depth_mm[:, :half]
    right_depth = depth_mm[:, half:]

    if np.any(left_valid):
        dpthl = float(np.clip(
            (np.mean(left_depth[left_valid]) - near_mm) / (far_mm - near_mm),
            0.0, 1.0
        ))
    else:
        dpthl = 0.5  # neutral

    if np.any(right_valid):
        dpthr = float(np.clip(
            (np.mean(right_depth[right_valid]) - near_mm) / (far_mm - near_mm),
            0.0, 1.0
        ))
    else:
        dpthr = 0.5  # neutral

    # --- ENTR: depth field entropy (complexity) ---
    # Histogram entropy of normalized depth values within valid region
    vals = norm[valid]
    if len(vals) > 0:
        hist, _ = np.histogram(vals, bins=16, range=(0.0, 1.0))
        hist = hist.astype(np.float32)
        hist_sum = hist.sum()
        if hist_sum > 0:
            p = hist / hist_sum
            p = p[p > 0]
            raw_entropy = -float(np.sum(p * np.log2(p)))
            entr = float(np.clip(raw_entropy / 4.0, 0.0, 1.0))  # max entropy = 4 bits for 16 bins
        else:
            entr = 0.0
    else:
        entr = 0.0

    # --- Body count heuristic (without skeleton SDK) ---
    # Rough estimation: segment foreground and count connected blobs
    body_count = _estimate_body_count(fg_mask)

    return dict(dist=dist, motion=motion, cntx=cntx, cnty=cnty,
                area=area, dpthl=dpthl, dpthr=dpthr, entr=entr,
                body_count=body_count)


def _estimate_body_count(fg_mask: np.ndarray) -> int:
    """
    Crude body count from foreground mask.
    Downsample to 80×60, morphologically clean, count blobs.
    Returns 0-3 (capped).
    """
    try:
        from scipy.ndimage import label, binary_erosion, binary_dilation
        small = fg_mask[::8, ::8].astype(bool)
        # Clean noise
        cleaned = binary_erosion(small, iterations=2)
        cleaned = binary_dilation(cleaned, iterations=3)
        labeled, n_features = label(cleaned)
        # Filter small blobs (< 5% of downsampled frame)
        min_size = small.size * 0.05
        real_bodies = sum(
            1 for i in range(1, n_features + 1)
            if np.sum(labeled == i) >= min_size
        )
        return min(real_bodies, 3)
    except ImportError:
        # scipy not available: use simple threshold
        fg_frac = float(np.mean(fg_mask))
        if fg_frac < 0.02:
            return 0
        elif fg_frac < 0.15:
            return 1
        elif fg_frac < 0.30:
            return 2
        else:
            return 3


# ──────────────────────────────────────────────────────────
# Packet Builders
# ──────────────────────────────────────────────────────────

def pack_kint(cvs: dict, source: int, body_count: int) -> bytes:
    """Build 48-byte KINT depth CV packet."""
    ts = int(time.time() * 1_000_000)
    return struct.pack('<4sHBB8fQ',
        KINT_MAGIC,
        KINT_VERSION,
        source & 0xFF,
        body_count & 0xFF,
        cvs['dist'],
        cvs['motion'],
        cvs['cntx'],
        cvs['cnty'],
        cvs['area'],
        cvs['dpthl'],
        cvs['dpthr'],
        cvs['entr'],
        ts,
    )


def pack_skel(body_index: int, joints_xyz: np.ndarray) -> bytes:
    """
    Build SKEL packet for joint position data.
    joints_xyz: (N, 3) float32 array, positions in metres, normalized -1 to +1
    """
    assert joints_xyz.ndim == 2 and joints_xyz.shape[1] == 3
    joint_count = joints_xyz.shape[0]
    ts = int(time.time() * 1_000_000)
    header = struct.pack('<4sHBB',
        SKEL_MAGIC,
        SKEL_VERSION,
        body_index & 0xFF,
        joint_count & 0xFF,
    )
    body = joints_xyz.astype(np.float32).tobytes()
    footer = struct.pack('<Q', ts)
    return header + body + footer


# ──────────────────────────────────────────────────────────
# Kinect 360 Backend (libfreenect)
# ──────────────────────────────────────────────────────────

class Kinect360Backend:
    """
    Kinect 360 / Kinect for Windows v1 backend.
    Requires: pip install freenect
    USB: VID 0x045E PID 0x02AE / 0x02B0
    Depth: 640×480, raw 11-bit (0-2047), structured light
    Range: 0.8m – 4.0m
    """

    SOURCE_ID = SOURCE_K360
    DEPTH_W, DEPTH_H = 640, 480
    NEAR_MM = K360_NEAR_MM
    FAR_MM  = K360_FAR_MM

    def __init__(self):
        self.freenect = None
        self._ctx = None

    def open(self) -> bool:
        try:
            self.freenect = importlib.import_module('freenect')
        except ImportError:
            print("[K360] 'freenect' not installed. Run: pip install freenect")
            return False

        # Quick check: can we get a depth frame?
        try:
            result = self.freenect.sync_get_depth(index=0, format=self.freenect.DEPTH_11BIT)
            if result is None:
                print("[K360] No Kinect 360 device found.")
                return False
            print(f"[K360] Kinect 360 open — {self.DEPTH_W}×{self.DEPTH_H} depth")
            return True
        except Exception as e:
            print(f"[K360] Failed to open: {e}")
            return False

    def get_depth_mm(self) -> Optional[np.ndarray]:
        """
        Returns H×W float32 array in mm, or None on failure.
        Kinect 360 raw disparity → mm conversion (Meier-Brown formula).
        """
        try:
            result = self.freenect.sync_get_depth(index=0, format=self.freenect.DEPTH_11BIT)
            if result is None:
                return None
            raw, _ts = result
            # raw: uint16 H×W, values 0-2047 (2047=no data)
            raw = raw.astype(np.float32)
            # Disparity → depth in mm (standard formula)
            # d = 0.1236 * tan(raw / 2842.5 + 1.1863) * 1000
            # But simpler and more widely used:
            # z_m = 1.0 / (raw * -0.0030711016 + 3.3309495161)
            with np.errstate(invalid='ignore', divide='ignore'):
                depth_m = 1.0 / (raw * -0.0030711016 + 3.3309495161)
            depth_mm = depth_m * 1000.0
            # Mask invalid (raw==2047 or negative depth)
            invalid = (raw >= 2047) | (depth_mm < 0) | ~np.isfinite(depth_mm)
            depth_mm[invalid] = 0.0
            return depth_mm
        except Exception as e:
            print(f"[K360] get_depth error: {e}")
            return None

    def get_skeleton(self) -> list:
        """Kinect 360 via libfreenect has no built-in skeleton tracking. Returns []."""
        return []

    def close(self):
        if self.freenect:
            try:
                self.freenect.sync_stop()
            except Exception:
                pass
        print("[K360] Closed.")


# ──────────────────────────────────────────────────────────
# Kinect One Backend (libfreenect2)
# ──────────────────────────────────────────────────────────

class KinectOneBackend:
    """
    Kinect for Xbox One / Kinect for Windows v2 backend.
    Requires: pip install pylibfreenect2
    Also requires libfreenect2 native library to be installed.
    USB 3.0 required. VID 0x045E PID 0x02C4.
    Depth: 512×424, uint16 ToF, mm, range 0.5m – 4.5m
    Color: 1920×1080
    Skeleton: Not included in libfreenect2 (requires NiTE2, optional).
    """

    SOURCE_ID = SOURCE_KONE
    DEPTH_W, DEPTH_H = 512, 424
    NEAR_MM = KOA_NEAR_MM
    FAR_MM  = KOA_FAR_MM

    def __init__(self):
        self.fn2 = None
        self.freenect2 = None
        self.device = None
        self.listener = None
        self.pipeline = None

    def open(self) -> bool:
        try:
            self.fn2 = importlib.import_module('pylibfreenect2')
        except ImportError:
            print("[KOne] 'pylibfreenect2' not installed.")
            print("       Install: https://github.com/r9y9/pylibfreenect2")
            return False

        try:
            fn2 = self.fn2
            # Select best available pipeline
            for pipeline_cls in ('OpenGLPacketPipeline', 'OpenCLPacketPipeline',
                                 'CpuPacketPipeline'):
                if hasattr(fn2, pipeline_cls):
                    self.pipeline = getattr(fn2, pipeline_cls)()
                    break

            if self.pipeline is None:
                self.pipeline = fn2.CpuPacketPipeline()

            self.freenect2 = fn2.Freenect2()
            num_devices = self.freenect2.enumerateDevices()
            if num_devices == 0:
                print("[KOne] No Kinect One devices found.")
                return False

            serial = self.freenect2.getDeviceSerialNumber(0)
            self.device = self.freenect2.openDevice(serial, self.pipeline)

            self.listener = fn2.SyncMultiFrameListener(
                fn2.FrameType.Depth
            )
            self.device.setIrAndDepthFrameListener(self.listener)
            self.device.start()

            print(f"[KOne] Kinect One open — serial {serial} — {self.DEPTH_W}×{self.DEPTH_H} ToF depth")
            return True

        except Exception as e:
            print(f"[KOne] Failed to open: {e}")
            return False

    def get_depth_mm(self) -> Optional[np.ndarray]:
        """Returns 424×512 float32 depth in mm, or None."""
        try:
            fn2 = self.fn2
            frames = {}
            if self.listener.hasNewFrame():
                frames = self.listener.waitForNewFrame(timeout=100)
                depth_frame = frames['depth']
                # pylibfreenect2 depth frame: 424×512, float32, values in mm
                depth = depth_frame.asarray(np.float32).copy()
                self.listener.release(frames)
                return depth
            return None
        except Exception as e:
            print(f"[KOne] get_depth error: {e}")
            return None

    def get_skeleton(self) -> list:
        """
        Skeleton requires NiTE2 SDK — not included in libfreenect2.
        Returns [].
        To enable: install NiTE2 and use OpenNI2 + NiTE2 bindings.
        """
        return []

    def close(self):
        try:
            if self.device:
                self.device.stop()
                self.device.close()
        except Exception:
            pass
        print("[KOne] Closed.")


# ──────────────────────────────────────────────────────────
# Azure Kinect Backend (k4a)
# ──────────────────────────────────────────────────────────

class AzureKinectBackend:
    """
    Azure Kinect DK backend.
    Requires: pip install pyk4a
    Also requires Azure Kinect SDK (k4a) native library.
    For body tracking: additionally requires Azure Kinect Body Tracking SDK.
    ARM64 Linux: https://docs.microsoft.com/en-us/azure/kinect-dk/sensor-sdk-download
    USB 3.1 required.
    Depth: 1024×1024 NFOV, uint16, mm, range 0.25m–2.88m (NFOV)
           or 512×512 NFOV 2x2 binned (faster)
    Body: 32 joints per body (up to 6 bodies)
    """

    SOURCE_ID = SOURCE_AZURE
    DEPTH_W, DEPTH_H = 512, 512   # NFOV_2X2BINNED (good speed/quality balance)
    NEAR_MM = 250.0
    FAR_MM  = 2880.0

    def __init__(self):
        self.k4a_module = None
        self.device = None
        self.bt_tracker = None   # Body tracking, optional

    def open(self) -> bool:
        try:
            self.k4a_module = importlib.import_module('pyk4a')
        except ImportError:
            print("[Azure] 'pyk4a' not installed. Run: pip install pyk4a")
            print("        Also requires Azure Kinect SDK: https://aka.ms/azurekinect")
            return False

        try:
            k4a = self.k4a_module
            self.device = k4a.PyK4A(k4a.Config(
                color_resolution=k4a.ColorResolution.OFF,  # disable color for performance
                depth_mode=k4a.DepthMode.NFOV_2X2BINNED,  # 512×512, better FPS
                synchronized_images_only=False,
                camera_fps=k4a.FPS.FPS_30,
            ))
            self.device.start()

            # Try to init body tracking (optional, requires body tracking SDK)
            self._init_body_tracker()

            print(f"[Azure] Azure Kinect open — {self.DEPTH_W}×{self.DEPTH_H} NFOV_2X2BINNED depth")
            if self.bt_tracker:
                print("[Azure] Body tracking SDK available — 32-joint skeleton active")
            else:
                print("[Azure] Body tracking SDK not available — depth CVs only")
            return True

        except Exception as e:
            print(f"[Azure] Failed to open: {e}")
            return False

    def _init_body_tracker(self):
        """Try to initialize Azure Kinect Body Tracking SDK."""
        try:
            from pyk4a import PyK4ABT, TrackerConfiguration
            self.bt_tracker = PyK4ABT(self.device,
                TrackerConfiguration(
                    sensor_orientation=0,  # K4ABT_SENSOR_ORIENTATION_DEFAULT
                    processing_mode=1,     # K4ABT_TRACKER_PROCESSING_MODE_GPU
                    gpu_device_id=0,
                )
            )
            self.bt_tracker.start()
        except (ImportError, AttributeError, Exception):
            self.bt_tracker = None

    def get_depth_mm(self) -> Optional[np.ndarray]:
        """Returns H×W float32 depth in mm, or None."""
        try:
            capture = self.device.get_capture()
            if capture.depth is None:
                return None
            # pyk4a depth: H×W uint16, values in mm
            depth = capture.depth.astype(np.float32)
            return depth
        except Exception as e:
            print(f"[Azure] get_depth error: {e}")
            return None

    def get_skeleton(self) -> list:
        """
        Returns list of body frames.
        Each body: dict with 'body_id' and 'joints' (32×3 float array in metres)
        Joints are normalized to [-1, 1] relative to depth sensor field-of-view.
        """
        if self.bt_tracker is None:
            return []
        try:
            frame = self.bt_tracker.pop_result()
            if frame is None:
                return []

            bodies = []
            for i in range(frame.get_num_bodies()):
                body_id = frame.get_body_id(i)
                # joints: list of K4ABT_JOINT structs with .position (x,y,z in mm) and .confidence
                joints = frame.get_body_skeleton(i).joints  # 32 joints
                positions = np.array([[j.position.x / 1000.0,   # mm → m
                                       j.position.y / 1000.0,
                                       j.position.z / 1000.0]
                                      for j in joints], dtype=np.float32)
                # Normalize: divide by FAR in metres so values are roughly 0-1
                far_m = self.FAR_MM / 1000.0
                positions = np.clip(positions / far_m, -1.0, 1.0)
                bodies.append({'body_id': body_id, 'joints': positions})
            return bodies

        except Exception as e:
            return []

    def close(self):
        try:
            if self.bt_tracker:
                self.bt_tracker.stop()
            if self.device:
                self.device.stop()
        except Exception:
            pass
        print("[Azure] Closed.")


# ──────────────────────────────────────────────────────────
# Simulated Backend (for testing without hardware)
# ──────────────────────────────────────────────────────────

class SimulatedBackend:
    """
    Generates synthetic depth data for testing without hardware.
    Simulates a person walking towards and away from the sensor.
    """

    SOURCE_ID = SOURCE_SIM
    DEPTH_W, DEPTH_H = 512, 424
    NEAR_MM = KOA_NEAR_MM
    FAR_MM  = KOA_FAR_MM

    def __init__(self):
        self._t = 0.0
        self._rng = np.random.default_rng(42)

    def open(self) -> bool:
        print("[Sim] Simulated Kinect backend active (no hardware required)")
        return True

    def get_depth_mm(self) -> np.ndarray:
        self._t += 1.0 / 30.0

        h, w = self.DEPTH_H, self.DEPTH_W
        depth = np.full((h, w), self.FAR_MM, dtype=np.float32)

        # Simulate a "body" as an ellipse in the center
        cx = int(w * 0.5 + w * 0.2 * np.sin(self._t * 0.5))
        cy = int(h * 0.5)
        # Body depth oscillates between 0.8m and 2.5m
        body_depth = 1500.0 + 700.0 * np.sin(self._t * 0.3)

        ys, xs = np.mgrid[0:h, 0:w]
        dist2 = ((xs - cx) / (w * 0.15))**2 + ((ys - cy) / (h * 0.35))**2
        body_mask = dist2 < 1.0
        noise = self._rng.normal(0.0, 15.0, (h, w)).astype(np.float32)
        depth[body_mask] = body_depth + noise[body_mask]

        # Add motion noise occasionally
        if np.sin(self._t * 7) > 0.8:
            depth += self._rng.normal(0.0, 30.0, (h, w)).astype(np.float32)
            depth = np.clip(depth, self.NEAR_MM, self.FAR_MM)

        return depth

    def get_skeleton(self) -> list:
        return []

    def close(self):
        print("[Sim] Simulated backend closed.")


# ──────────────────────────────────────────────────────────
# Device Auto-Detection
# ──────────────────────────────────────────────────────────

def _usb_device_present(vid: int, pids: list[int]) -> bool:
    """Check if a USB device with given VID/any PID is present."""
    try:
        import usb.core
        for pid in pids:
            dev = usb.core.find(idVendor=vid, idProduct=pid)
            if dev is not None:
                return True
        return False
    except ImportError:
        # pyusb not available — try /sys/bus/usb on Linux
        import os
        if not os.path.exists('/sys/bus/usb/devices'):
            return False
        try:
            for dev_dir in os.listdir('/sys/bus/usb/devices'):
                vid_path = f'/sys/bus/usb/devices/{dev_dir}/idVendor'
                pid_path = f'/sys/bus/usb/devices/{dev_dir}/idProduct'
                if os.path.exists(vid_path) and os.path.exists(pid_path):
                    with open(vid_path) as f:
                        fvid = int(f.read().strip(), 16)
                    with open(pid_path) as f:
                        fpid = int(f.read().strip(), 16)
                    if fvid == vid and fpid in pids:
                        return True
        except Exception:
            pass
        return False


def auto_detect_backend() -> Optional[object]:
    """
    Probe USB for Kinect devices and return the appropriate backend.
    Priority: Azure > Kinect One > Kinect 360 > Simulated
    """
    MICROSOFT_VID = 0x045E
    AZURE_PIDS  = [0x097C, 0x097D]
    KONE_PIDS   = [0x02C4, 0x02C3, 0x02BB, 0x02D8]
    K360_PIDS   = [0x02AE, 0x02A8, 0x02B0]

    if _usb_device_present(MICROSOFT_VID, AZURE_PIDS):
        print("[AutoDetect] Azure Kinect detected")
        return AzureKinectBackend()

    if _usb_device_present(MICROSOFT_VID, KONE_PIDS):
        print("[AutoDetect] Kinect One detected")
        return KinectOneBackend()

    if _usb_device_present(MICROSOFT_VID, K360_PIDS):
        print("[AutoDetect] Kinect 360 detected")
        return Kinect360Backend()

    print("[AutoDetect] No Kinect device found — using simulated backend")
    return SimulatedBackend()


BACKEND_MAP = {
    'auto':  auto_detect_backend,
    'k360':  Kinect360Backend,
    'kone':  KinectOneBackend,
    'azure': AzureKinectBackend,
    'sim':   SimulatedBackend,
}


# ──────────────────────────────────────────────────────────
# Main Bridge Loop
# ──────────────────────────────────────────────────────────

def run_bridge(backend,
               host: str,
               depth_port: int,
               skel_port: int,
               near_mm: float,
               far_mm: float,
               show: bool,
               fps_target: int = 30):

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    depth_target = (host, depth_port)
    skel_target  = (host, skel_port)

    source_id = backend.SOURCE_ID
    print(f"WeirdSynths Kinect Bridge → {host}:{depth_port} (depth) | {host}:{skel_port} (skel)")
    print(f"Source: {SOURCE_NAMES[source_id]}")
    print(f"Range:  {near_mm:.0f}mm – {far_mm:.0f}mm")
    print()

    frame_time = 1.0 / fps_target
    prev_depth = None
    frame_count = 0
    fps_timer = time.time()
    actual_fps = 0.0

    # Optional OpenCV preview
    show_cv2 = False
    if show:
        try:
            import cv2 as _cv
            show_cv2 = True
        except ImportError:
            print("[Bridge] opencv-python not installed — preview disabled")

    try:
        while True:
            t0 = time.time()

            # --- Get depth frame ---
            depth_mm = backend.get_depth_mm()
            if depth_mm is None:
                time.sleep(0.01)
                continue

            # --- Extract CVs ---
            cvs = extract_depth_cvs(depth_mm, prev_depth, near_mm, far_mm)
            prev_depth = depth_mm.copy()

            # --- Send KINT packet ---
            kint_pkt = pack_kint(cvs, source_id, cvs['body_count'])
            sock.sendto(kint_pkt, depth_target)

            # --- Send SKEL packets if skeleton data available ---
            skeletons = backend.get_skeleton()
            for i, body in enumerate(skeletons):
                skel_pkt = pack_skel(i, body['joints'])
                sock.sendto(skel_pkt, skel_target)

            # --- FPS tracking ---
            frame_count += 1
            now = time.time()
            if now - fps_timer >= 1.0:
                actual_fps = frame_count / (now - fps_timer)
                frame_count = 0
                fps_timer = now
                skel_str = f" | {len(skeletons)} body" + ("ies" if len(skeletons) != 1 else "") if skeletons else ""
                print(f"\r  {actual_fps:.0f} fps | "
                      f"dist={cvs['dist']:.3f} "
                      f"motion={cvs['motion']:.3f} "
                      f"centX={cvs['cntx']:+.2f} "
                      f"centY={cvs['cnty']:+.2f} "
                      f"area={cvs['area']:.3f} "
                      f"L={cvs['dpthl']:.2f} R={cvs['dpthr']:.2f} "
                      f"entr={cvs['entr']:.2f} "
                      f"bodies={cvs['body_count']}"
                      f"{skel_str}",
                      end='', flush=True)

            # --- Optional preview ---
            if show_cv2:
                import cv2
                vis = np.clip((depth_mm - near_mm) / (far_mm - near_mm), 0.0, 1.0)
                vis = (vis * 255).astype(np.uint8)
                vis_color = cv2.applyColorMap(vis, cv2.COLORMAP_TURBO)
                h, w = vis_color.shape[:2]
                # Overlay CV values
                overlay = [
                    f"DIST  {cvs['dist']:.3f}",
                    f"MOTN  {cvs['motion']:.3f}",
                    f"CNTX  {cvs['cntx']:+.2f}",
                    f"CNTY  {cvs['cnty']:+.2f}",
                    f"AREA  {cvs['area']:.3f}",
                    f"L/R   {cvs['dpthl']:.2f}/{cvs['dpthr']:.2f}",
                    f"ENTR  {cvs['entr']:.3f}",
                    f"BODY  {cvs['body_count']}",
                ]
                for j, txt in enumerate(overlay):
                    cv2.putText(vis_color, txt, (10, 20 + j * 22),
                                cv2.FONT_HERSHEY_SIMPLEX, 0.55, (100, 255, 100), 1)
                cv2.putText(vis_color,
                            f"WeirdSynths | {SOURCE_NAMES[source_id]} | {actual_fps:.0f} fps",
                            (10, h - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.45, (200, 200, 200), 1)
                cv2.imshow('Kinect Bridge', vis_color)
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break

            # --- Frame rate limit ---
            elapsed = time.time() - t0
            if elapsed < frame_time:
                time.sleep(frame_time - elapsed)

    except KeyboardInterrupt:
        print("\n\nStopped.")

    finally:
        backend.close()
        sock.close()
        if show_cv2:
            import cv2
            cv2.destroyAllWindows()
        print("Kinect Bridge closed.")


# ──────────────────────────────────────────────────────────
# Entry Point
# ──────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description='WeirdSynths Kinect Bridge — depth tracking to VCV Rack UDP',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Devices:
  auto    Auto-detect (Azure > Kinect One > Kinect 360 > Simulated)
  k360    Force Kinect 360 (Kinect for Xbox 360 / Kinect for Windows v1)
  kone    Force Kinect One (Kinect for Xbox One / Kinect for Windows v2)
  azure   Force Azure Kinect DK
  sim     Simulated (no hardware, for testing)

VCV Rack port assignments:
  9005    DEPTH module (8 CV values)
  9006    BODY module (skeleton joints, Azure only)

Install guide:
  Kinect 360:  pip install freenect
               (libfreenect must be installed: brew/apt install libfreenect-dev)
  Kinect One:  pip install pylibfreenect2
               (libfreenect2 must be installed)
  Azure:       pip install pyk4a
               (Azure Kinect SDK must be installed from Microsoft)
""",
    )
    parser.add_argument('--device',     type=str, default='auto',
                        choices=list(BACKEND_MAP.keys()),
                        help='Kinect device backend (default: auto)')
    parser.add_argument('--host',       type=str, default='127.0.0.1',
                        help='UDP target host (default: 127.0.0.1)')
    parser.add_argument('--depth-port', type=int, default=9005,
                        help='UDP port for DEPTH CVs (default: 9005)')
    parser.add_argument('--skel-port',  type=int, default=9006,
                        help='UDP port for skeleton joints (default: 9006)')
    parser.add_argument('--near',       type=float, default=None,
                        help='Near clipping in metres (default: device-specific)')
    parser.add_argument('--far',        type=float, default=None,
                        help='Far clipping in metres (default: device-specific)')
    parser.add_argument('--fps',        type=int,   default=30,
                        help='Target FPS (default: 30)')
    parser.add_argument('--show',       action='store_true',
                        help='Show depth preview window (requires opencv-python)')
    args = parser.parse_args()

    # Create backend
    factory = BACKEND_MAP[args.device]
    backend = factory() if callable(factory) and not isinstance(factory, type) else (factory if not callable(factory) else factory())

    if not backend.open():
        print("Failed to open Kinect device. Exiting.")
        sys.exit(1)

    # Determine working range
    near_mm = (args.near * 1000.0) if args.near is not None else backend.NEAR_MM
    far_mm  = (args.far  * 1000.0) if args.far  is not None else backend.FAR_MM

    run_bridge(
        backend=backend,
        host=args.host,
        depth_port=args.depth_port,
        skel_port=args.skel_port,
        near_mm=near_mm,
        far_mm=far_mm,
        show=args.show,
        fps_target=args.fps,
    )


if __name__ == '__main__':
    main()
