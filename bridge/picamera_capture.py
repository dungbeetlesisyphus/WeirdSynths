#!/usr/bin/env python3
"""
picamera_capture.py — Pi Camera / OpenCV unified capture module
================================================================
Provides a single get_camera() factory that returns the best available
camera capture backend:

  Priority 1: picamera2 (Pi Camera Module 3 via libcamera CSI)
              IMX708 — Sony 12MP back-illuminated stacked CMOS
              120fps @ 1536×864, continuous PDAF+CDAF autofocus

  Priority 2: OpenCV VideoCapture (USB webcam, any V4L2 device)

Both expose the same interface:
    cam = get_camera(mode='track120', camera_index=0)
    cam.open()
    ok, frame_rgb = cam.read()   # always returns RGB numpy array
    cam.close()

Pi Camera Modes (IMX708 / Camera Module 3):
    track120  1536×864  @ 120fps  — face tracking, max speed (RECOMMENDED)
    track60   2304×1296 @ 56fps   — higher resolution tracking
    hdr       2304×1296 @ 30fps   — HDR mode (stage lighting, high contrast)
    full      4608×2592 @ 14fps   — full 12MP, stills/reference only

Usage:
    from picamera_capture import get_camera, list_modes, MODES
"""

from __future__ import annotations

import time
from typing import Optional, Tuple
import numpy as np

# ──────────────────────────────────────────────────────────
# Pi Camera Mode Definitions (IMX708 sensor modes)
# ──────────────────────────────────────────────────────────

MODES = {
    'track120': {
        'width':  1536,
        'height': 864,
        'fps':    120.0,
        'desc':   '1536×864 @ 120fps — fastest, best for face/body tracking',
        'crop':   'centre 3072×1728 from full sensor',
    },
    'track60': {
        'width':  2304,
        'height': 1296,
        'fps':    56.0,
        'desc':   '2304×1296 @ 56fps — higher resolution tracking',
        'crop':   'full sensor binned',
    },
    'hdr': {
        'width':  2304,
        'height': 1296,
        'fps':    30.0,
        'desc':   '2304×1296 @ 30fps HDR — stage/high-contrast lighting',
        'crop':   'full sensor, HDR enabled',
        'hdr':    True,
    },
    'full': {
        'width':  4608,
        'height': 2592,
        'fps':    14.35,
        'desc':   '4608×2592 @ 14fps — max resolution, slow',
        'crop':   'full 12MP sensor',
    },
}

DEFAULT_MODE = 'track120'

# ──────────────────────────────────────────────────────────
# Pi Camera Backend (picamera2 / libcamera)
# ──────────────────────────────────────────────────────────

class PiCameraCapture:
    """
    Capture from Raspberry Pi Camera Module via picamera2 (libcamera).
    Returns RGB frames as numpy arrays (NOT BGR — no cvtColor needed for MediaPipe).
    Enables continuous autofocus (PDAF+CDAF) by default.
    """

    def __init__(self, mode: str = DEFAULT_MODE, autofocus: bool = True):
        m = MODES.get(mode, MODES[DEFAULT_MODE])
        self.mode_name  = mode
        self.width      = m['width']
        self.height     = m['height']
        self.fps        = m['fps']
        self.use_hdr    = m.get('hdr', False)
        self.autofocus  = autofocus
        self._cam       = None
        self._is_open   = False

    @property
    def is_picamera(self) -> bool:
        return True

    def open(self) -> bool:
        try:
            from picamera2 import Picamera2
        except ImportError:
            print("[PiCamera] picamera2 not installed.")
            print("           On Pi OS: sudo apt install python3-picamera2")
            print("           On WeirdOS: included in default image")
            return False

        try:
            self._cam = Picamera2()

            # Build controls dict
            controls: dict = {"FrameRate": float(self.fps)}

            if self.autofocus:
                # AfMode 1 = Continuous, AfTrigger 0 = start immediately
                controls["AfMode"]    = 1
                controls["AfTrigger"] = 0
            else:
                controls["AfMode"] = 0   # Manual focus

            if self.use_hdr:
                # HDR mode — must configure before start
                Picamera2.set_logging(Picamera2.WARNING)
                self._cam.options["enable_hdr"] = True

            config = self._cam.create_video_configuration(
                main={"size": (self.width, self.height), "format": "RGB888"},
                controls=controls,
                buffer_count=4,    # 4 frame buffers for smooth capture
            )
            self._cam.configure(config)
            self._cam.start()
            self._is_open = True

            mode_info = MODES.get(self.mode_name, {})
            print(f"[PiCamera] Camera Module 3 (IMX708) — mode: {self.mode_name}")
            print(f"[PiCamera] {self.width}×{self.height} @ {self.fps:.0f}fps | "
                  f"{'HDR ' if self.use_hdr else ''}"
                  f"{'Autofocus' if self.autofocus else 'Manual focus'}")
            print(f"[PiCamera] {mode_info.get('desc', '')}")
            return True

        except Exception as e:
            print(f"[PiCamera] Failed to open: {e}")
            if self._cam:
                try:
                    self._cam.close()
                except Exception:
                    pass
            self._cam = None
            return False

    def read(self) -> Tuple[bool, Optional[np.ndarray]]:
        """
        Capture a frame. Returns (success, frame_rgb).
        frame_rgb is H×W×3 uint8 numpy array in RGB order (not BGR).
        """
        if not self._is_open or self._cam is None:
            return False, None
        try:
            frame = self._cam.capture_array("main")
            # picamera2 with format "RGB888" returns RGB directly
            return True, frame
        except Exception as e:
            print(f"[PiCamera] Read error: {e}")
            return False, None

    def get_frame_time(self) -> float:
        """Target frame interval in seconds."""
        return 1.0 / self.fps

    def get_size(self) -> Tuple[int, int]:
        return self.width, self.height

    def set_focus_position(self, lens_position: float):
        """Manual focus — lens_position 0.0 (∞) to 32.0 (macro). No-op if autofocus is on."""
        if self._cam and not self.autofocus:
            self._cam.set_controls({"LensPosition": lens_position, "AfMode": 0})

    def get_autofocus_state(self) -> str:
        """Return current AF state string for status display."""
        if not self._cam or not self.autofocus:
            return "Manual"
        try:
            meta = self._cam.capture_metadata()
            af_state = meta.get("AfState", 0)
            return {0: "Idle", 1: "Scanning", 2: "Focused", 3: "Failed"}.get(af_state, "?")
        except Exception:
            return "?"

    def close(self):
        if self._cam:
            try:
                self._cam.stop()
                self._cam.close()
            except Exception:
                pass
        self._is_open = False
        self._cam = None
        print("[PiCamera] Camera closed.")


# ──────────────────────────────────────────────────────────
# OpenCV Backend (USB webcam / V4L2 fallback)
# ──────────────────────────────────────────────────────────

class OpenCVCapture:
    """
    Capture from a USB webcam or any V4L2 device via OpenCV.
    Converts BGR → RGB before returning so interface matches PiCameraCapture.
    """

    def __init__(self, device_index: int = 0, width: int = 640, height: int = 480, fps: int = 30):
        self.device_index = device_index
        self.width  = width
        self.height = height
        self.fps    = fps
        self._cap   = None

    @property
    def is_picamera(self) -> bool:
        return False

    def open(self) -> bool:
        try:
            import cv2
        except ImportError:
            print("[OpenCV] opencv-python not installed. Run: pip install opencv-python")
            return False

        self._cap = cv2.VideoCapture(self.device_index)
        if not self._cap.isOpened():
            print(f"[OpenCV] Cannot open camera {self.device_index}")
            return False

        self._cap.set(cv2.CAP_PROP_FRAME_WIDTH,  self.width)
        self._cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.height)
        self._cap.set(cv2.CAP_PROP_FPS,          self.fps)

        actual_w = int(self._cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        actual_h = int(self._cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        actual_fps = self._cap.get(cv2.CAP_PROP_FPS)

        print(f"[OpenCV] Camera {self.device_index}: {actual_w}×{actual_h} @ {actual_fps:.0f}fps")
        return True

    def read(self) -> Tuple[bool, Optional[np.ndarray]]:
        """Returns (success, frame_rgb) — BGR→RGB converted."""
        if self._cap is None:
            return False, None
        try:
            import cv2
            ok, frame_bgr = self._cap.read()
            if not ok:
                return False, None
            # Flip horizontally for mirror effect (matching original nerve_bridge.py)
            frame_bgr = cv2.flip(frame_bgr, 1)
            # Convert BGR → RGB for MediaPipe
            frame_rgb = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2RGB)
            return True, frame_rgb
        except Exception as e:
            print(f"[OpenCV] Read error: {e}")
            return False, None

    def get_frame_time(self) -> float:
        return 1.0 / max(self.fps, 1)

    def get_size(self) -> Tuple[int, int]:
        if self._cap:
            import cv2
            return (int(self._cap.get(cv2.CAP_PROP_FRAME_WIDTH)),
                    int(self._cap.get(cv2.CAP_PROP_FRAME_HEIGHT)))
        return self.width, self.height

    def close(self):
        if self._cap:
            self._cap.release()
        self._cap = None
        print("[OpenCV] Camera closed.")


# ──────────────────────────────────────────────────────────
# Auto-Detection
# ──────────────────────────────────────────────────────────

def _is_raspberry_pi() -> bool:
    """Check if running on a Raspberry Pi."""
    import os
    try:
        with open('/proc/device-tree/model', 'r') as f:
            return 'Raspberry Pi' in f.read()
    except Exception:
        pass
    return os.path.exists('/dev/rpivid') or os.path.exists('/sys/bus/platform/drivers/imx708')


def _picamera2_available() -> bool:
    """Check if picamera2 is importable AND a camera is attached."""
    try:
        from picamera2 import Picamera2
        cams = Picamera2.global_camera_info()
        return len(cams) > 0
    except Exception:
        return False


def get_camera(mode: str = DEFAULT_MODE,
               camera_index: int = 0,
               prefer_picamera: bool = True,
               width: int = 640,
               height: int = 480,
               fps: int = 30) -> object:
    """
    Factory: return the best available camera capture object.

    On a Raspberry Pi with Camera Module attached:
        → Returns PiCameraCapture (picamera2/libcamera)
    On any other system, or if picamera2 fails:
        → Returns OpenCVCapture (V4L2/USB webcam)

    Args:
        mode          : Pi Camera mode ('track120', 'track60', 'hdr', 'full')
        camera_index  : OpenCV fallback device index
        prefer_picamera: Try Pi Camera first if available
        width/height/fps: Fallback OpenCV dimensions
    """
    if prefer_picamera and _is_raspberry_pi() and _picamera2_available():
        print("[Camera] Raspberry Pi detected — using picamera2 (libcamera CSI)")
        return PiCameraCapture(mode=mode, autofocus=True)

    # Try picamera2 even on non-Pi (could be USB Pi Camera or custom setup)
    if prefer_picamera and _picamera2_available():
        print("[Camera] picamera2 available — using Pi Camera backend")
        return PiCameraCapture(mode=mode, autofocus=True)

    # Fallback to OpenCV
    print(f"[Camera] Using OpenCV backend (device {camera_index})")
    return OpenCVCapture(device_index=camera_index, width=width, height=height, fps=fps)


def list_modes():
    """Print available Pi Camera modes."""
    print("Pi Camera Module 3 (IMX708) Modes:")
    print()
    for name, m in MODES.items():
        hdr = " [HDR]" if m.get('hdr') else ""
        print(f"  {name:<12} {m['width']}×{m['height']} @ {m['fps']:.0f}fps{hdr}")
        print(f"             {m['desc']}")
    print()
    print("Default: track120 (1536×864 @ 120fps — recommended for face/body tracking)")
