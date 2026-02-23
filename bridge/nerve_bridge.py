#!/usr/bin/env python3
"""
NERVE Bridge — Face Tracking to VCV Rack UDP
Captures webcam via MediaPipe FaceLandmarker (Tasks API), extracts 17
facial features from blendshapes + landmarks, sends binary UDP packets
to NERVE module on localhost:9000.

Usage:
    pip install mediapipe opencv-python
    python nerve_bridge.py [--port 9000] [--port 9001] [--camera 0] [--show]

Sends to all specified ports simultaneously (default: 9000 for NERVE, 9001 for SKULL).

First run downloads the face_landmarker model (~4MB) automatically.

Protocol v2: 100-byte packets
    Bytes 0-3:   Magic "NERV"
    Bytes 4-5:   Protocol version (uint16, little-endian) = 2
    Bytes 6-7:   Face count (uint16, little-endian) = 1
    Bytes 8-91:  21 floats (float32, little-endian) = face data
    Bytes 92-99: Timestamp (uint64, little-endian) in microseconds
"""

import argparse
import os
import socket
import struct
import sys
import time
import urllib.request

import cv2
import mediapipe as mp
import numpy as np

# ── Model download URL ──
MODEL_URL = "https://storage.googleapis.com/mediapipe-models/face_landmarker/face_landmarker/float16/1/face_landmarker.task"
MODEL_FILENAME = "face_landmarker.task"


def ensure_model(script_dir: str) -> str:
    """Download the FaceLandmarker model if not present."""
    model_path = os.path.join(script_dir, MODEL_FILENAME)
    if not os.path.exists(model_path):
        print(f"Downloading face landmarker model...")
        urllib.request.urlretrieve(MODEL_URL, model_path)
        print(f"Model saved to {model_path}")
    return model_path


# ── Blendshape name → index mapping ──
# MediaPipe FaceLandmarker outputs 52 blendshapes.
# We map the ones we need to NERVE's 17 features.
# Full list: https://ai.google.dev/edge/mediapipe/solutions/vision/face_landmarker#models

BLENDSHAPE_NAMES = {
    'eyeBlinkLeft': None,
    'eyeBlinkRight': None,
    'eyeWideLeft': None,
    'eyeWideRight': None,
    'eyeLookUpLeft': None,
    'eyeLookUpRight': None,
    'eyeLookDownLeft': None,
    'eyeLookDownRight': None,
    'eyeLookInLeft': None,
    'eyeLookInRight': None,
    'eyeLookOutLeft': None,
    'eyeLookOutRight': None,
    'browDownLeft': None,
    'browDownRight': None,
    'browInnerUp': None,
    'browOuterUpLeft': None,
    'browOuterUpRight': None,
    'jawOpen': None,
    'mouthClose': None,
    'mouthSmileLeft': None,
    'mouthSmileRight': None,
    'mouthFrownLeft': None,
    'mouthFrownRight': None,
    'mouthPucker': None,
    'mouthLeft': None,
    'mouthRight': None,
    'mouthStretchLeft': None,
    'mouthStretchRight': None,
    'tongueOut': None,
}


def get_blendshape_dict(blendshapes) -> dict:
    """Convert MediaPipe blendshape list to name→score dict."""
    return {bs.category_name: bs.score for bs in blendshapes}


def extract_features(blendshapes: dict, landmarks, prev_head=None) -> dict:
    """Extract 21 NERVE features from blendshapes and landmarks.

    Blendshapes give us precise facial expressions (0-1 scores).
    Landmarks give us head position and distance.

    Returns dict matching NERVE protocol v2 order (17 original + 4 new).
    """
    # ── Head pose from landmarks ──
    # Nose tip is landmark 1, used for head position
    nose = landmarks[1]
    head_x = (nose.x - 0.5) * 2.0    # -1 to 1
    head_y = -(nose.y - 0.5) * 2.0   # -1 to 1 (up = positive)

    # Roll from eye positions (landmarks 33=left outer, 263=right outer)
    left_eye = landmarks[33]
    right_eye = landmarks[263]
    eye_angle = np.arctan2(
        right_eye.y - left_eye.y,
        right_eye.x - left_eye.x
    )
    head_z = float(np.clip(eye_angle / (np.pi / 4), -1.0, 1.0))

    # Distance from face height (forehead=10, chin=152)
    forehead = landmarks[10]
    chin = landmarks[152]
    face_h = np.sqrt((forehead.x - chin.x)**2 + (forehead.y - chin.y)**2)
    head_dist = float(np.clip(face_h / 0.4, 0.0, 1.0))  # ~0.3-0.4 at normal distance

    # ── Eyes from blendshapes ──
    # eyeWide = how open, eyeBlink = how closed
    # Openness = wide - blink, normalized
    blink_l_raw = blendshapes.get('eyeBlinkLeft', 0.0)
    blink_r_raw = blendshapes.get('eyeBlinkRight', 0.0)
    wide_l = blendshapes.get('eyeWideLeft', 0.0)
    wide_r = blendshapes.get('eyeWideRight', 0.0)

    left_eye_open = float(np.clip(1.0 - blink_l_raw + wide_l * 0.3, 0.0, 1.0))
    right_eye_open = float(np.clip(1.0 - blink_r_raw + wide_r * 0.3, 0.0, 1.0))

    # ── Gaze from blendshapes ──
    look_in_l = blendshapes.get('eyeLookInLeft', 0.0)    # left eye looking right
    look_out_l = blendshapes.get('eyeLookOutLeft', 0.0)   # left eye looking left
    look_in_r = blendshapes.get('eyeLookInRight', 0.0)    # right eye looking left
    look_out_r = blendshapes.get('eyeLookOutRight', 0.0)  # right eye looking right

    # Combine: positive = looking right, negative = looking left
    gaze_x_l = look_in_l - look_out_l   # left eye: in=right, out=left
    gaze_x_r = look_out_r - look_in_r   # right eye: out=right, in=left
    gaze_x = float(np.clip((gaze_x_l + gaze_x_r) / 2, -1.0, 1.0))

    look_up_l = blendshapes.get('eyeLookUpLeft', 0.0)
    look_up_r = blendshapes.get('eyeLookUpRight', 0.0)
    look_down_l = blendshapes.get('eyeLookDownLeft', 0.0)
    look_down_r = blendshapes.get('eyeLookDownRight', 0.0)
    gaze_y = float(np.clip(
        ((look_up_l + look_up_r) - (look_down_l + look_down_r)) / 2,
        -1.0, 1.0
    ))

    # ── Mouth from blendshapes ──
    jaw_open = blendshapes.get('jawOpen', 0.0)
    mouth_close = blendshapes.get('mouthClose', 0.0)

    # Mouth width: smile widens, pucker narrows
    smile_l = blendshapes.get('mouthSmileLeft', 0.0)
    smile_r = blendshapes.get('mouthSmileRight', 0.0)
    stretch_l = blendshapes.get('mouthStretchLeft', 0.0)
    stretch_r = blendshapes.get('mouthStretchRight', 0.0)
    pucker = blendshapes.get('mouthPucker', 0.0)
    mouth_w = float(np.clip(
        (smile_l + smile_r + stretch_l + stretch_r) / 4 - pucker * 0.5 + 0.3,
        0.0, 1.0
    ))

    # Mouth height: jaw open
    mouth_h = float(np.clip(jaw_open, 0.0, 1.0))

    # Jaw
    jaw = float(np.clip(jaw_open, 0.0, 1.0))

    # Lips: pucker/close
    lips = float(np.clip(pucker + mouth_close * 0.5, 0.0, 1.0))

    # ── Eyebrows from blendshapes ──
    brow_inner_up = blendshapes.get('browInnerUp', 0.0)
    brow_outer_up_l = blendshapes.get('browOuterUpLeft', 0.0)
    brow_outer_up_r = blendshapes.get('browOuterUpRight', 0.0)
    brow_down_l = blendshapes.get('browDownLeft', 0.0)
    brow_down_r = blendshapes.get('browDownRight', 0.0)

    brow_l = float(np.clip(brow_inner_up * 0.5 + brow_outer_up_l - brow_down_l * 0.5 + 0.5, 0.0, 1.0))
    brow_r = float(np.clip(brow_inner_up * 0.5 + brow_outer_up_r - brow_down_r * 0.5 + 0.5, 0.0, 1.0))

    # ── Blinks (binary threshold) ──
    BLINK_THRESHOLD = 0.5
    blink_l = 1.0 if blink_l_raw > BLINK_THRESHOLD else 0.0
    blink_r = 1.0 if blink_r_raw > BLINK_THRESHOLD else 0.0

    # ── Expression (composite activity) ──
    frown_l = blendshapes.get('mouthFrownLeft', 0.0)
    frown_r = blendshapes.get('mouthFrownRight', 0.0)
    expression = float(np.clip(
        jaw_open * 0.3 +
        (smile_l + smile_r) * 0.15 +
        (frown_l + frown_r) * 0.1 +
        abs(brow_l - 0.5) * 0.15 +
        abs(brow_r - 0.5) * 0.15,
        0.0, 1.0
    ))

    # ── Tongue from blendshapes ──
    # tongueOut blendshape is weak — boost 3x for usable range
    tongue_raw = blendshapes.get('tongueOut', 0.0)
    tongue = float(np.clip(tongue_raw * 3.0, 0.0, 1.0))

    return {
        'headX': float(head_x),
        'headY': float(head_y),
        'headZ': head_z,
        'headDist': head_dist,
        'leftEye': left_eye_open,
        'rightEye': right_eye_open,
        'gazeX': gaze_x,
        'gazeY': gaze_y,
        'mouthW': mouth_w,
        'mouthH': mouth_h,
        'jaw': jaw,
        'lips': lips,
        'browL': brow_l,
        'browR': brow_r,
        'blinkL': blink_l,
        'blinkR': blink_r,
        'expression': expression,
        # v2 additions
        'tongue': tongue,
        'browInnerUp': float(np.clip(brow_inner_up, 0.0, 1.0)),
        'browDownL': float(np.clip(brow_down_l, 0.0, 1.0)),
        'browDownR': float(np.clip(brow_down_r, 0.0, 1.0)),
    }


def pack_nerve_packet(features: dict) -> bytes:
    """Pack face features into 100-byte NERVE protocol v2 packet."""
    magic = b'NERV'
    version = struct.pack('<H', 2)
    face_count = struct.pack('<H', 1)

    floats = struct.pack('<21f',
        features['headX'],
        features['headY'],
        features['headZ'],
        features['headDist'],
        features['leftEye'],
        features['rightEye'],
        features['gazeX'],
        features['gazeY'],
        features['mouthW'],
        features['mouthH'],
        features['jaw'],
        features['lips'],
        features['browL'],
        features['browR'],
        features['blinkL'],
        features['blinkR'],
        features['expression'],
        features['tongue'],
        features['browInnerUp'],
        features['browDownL'],
        features['browDownR'],
    )

    timestamp = struct.pack('<Q', int(time.time() * 1_000_000))

    return magic + version + face_count + floats + timestamp


def draw_landmarks_on_frame(frame, landmarks, w, h):
    """Draw simple face landmark dots on the frame."""
    # Draw a subset of landmarks for performance
    key_indices = [
        1,    # nose tip
        33, 263,  # eye outer corners
        61, 291,  # mouth corners
        13, 14,   # mouth top/bottom
        152,  # chin
        10,   # forehead
        159, 145,  # left eye top/bottom
        386, 374,  # right eye top/bottom
    ]
    for idx in key_indices:
        lm = landmarks[idx]
        cx = int(lm.x * w)
        cy = int(lm.y * h)
        cv2.circle(frame, (cx, cy), 2, (100, 255, 100), -1)

    # Draw face oval outline
    oval_indices = [10, 338, 297, 332, 284, 251, 389, 356, 454, 323,
                    361, 288, 397, 365, 379, 378, 400, 377, 152, 148,
                    176, 149, 150, 136, 172, 58, 132, 93, 234, 127,
                    162, 21, 54, 103, 67, 109, 10]
    for i in range(len(oval_indices) - 1):
        p1 = landmarks[oval_indices[i]]
        p2 = landmarks[oval_indices[i + 1]]
        cv2.line(frame,
                 (int(p1.x * w), int(p1.y * h)),
                 (int(p2.x * w), int(p2.y * h)),
                 (120, 80, 255), 1)


def draw_feature_overlay(frame, features, w, h):
    """Draw real-time feature bars and labels on the preview."""
    bar_x = w - 140
    bar_w = 120
    bar_h = 10
    y_start = 20
    spacing = 18

    items = [
        ('BrowL',     features['browL'],     (180, 120, 255)),
        ('BrowR',     features['browR'],     (180, 120, 255)),
        ('BrowUp',    features['browInnerUp'], (220, 160, 255)),
        ('BrowDnL',   features['browDownL'],  (140, 80, 200)),
        ('BrowDnR',   features['browDownR'],  (140, 80, 200)),
        ('Tongue',    features['tongue'],     (255, 100, 100)),
        ('Jaw',       features['jaw'],        (100, 200, 255)),
        ('Expr',      features['expression'], (100, 255, 200)),
    ]

    for i, (label, val, color) in enumerate(items):
        y = y_start + i * spacing
        # Background bar
        cv2.rectangle(frame, (bar_x, y), (bar_x + bar_w, y + bar_h), (40, 40, 40), -1)
        # Value bar
        fill_w = int(val * bar_w)
        cv2.rectangle(frame, (bar_x, y), (bar_x + fill_w, y + bar_h), color, -1)
        # Label
        cv2.putText(frame, label, (bar_x - 60, y + bar_h - 1),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.35, (200, 200, 200), 1)


def main():
    parser = argparse.ArgumentParser(description='NERVE Bridge — Face to VCV Rack')
    parser.add_argument('--port', type=int, nargs='+', default=[9000, 9001],
                        help='UDP port(s) to send to (default: 9000 9001)')
    parser.add_argument('--host', type=str, default='127.0.0.1', help='Target host (default: 127.0.0.1)')
    parser.add_argument('--camera', type=int, default=0, help='Camera index (default: 0)')
    parser.add_argument('--show', action='store_true', help='Show camera preview window')
    parser.add_argument('--fps', type=int, default=30, help='Target FPS (default: 30)')
    args = parser.parse_args()

    # ── Download model if needed ──
    script_dir = os.path.dirname(os.path.abspath(__file__))
    model_path = ensure_model(script_dir)

    # ── Setup UDP ──
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    ports = args.port if isinstance(args.port, list) else [args.port]
    targets = [(args.host, p) for p in ports]
    port_str = ', '.join(str(p) for p in ports)
    print(f"NERVE Bridge -> {args.host}:[{port_str}]")

    # ── Setup MediaPipe FaceLandmarker (new Tasks API) ──
    BaseOptions = mp.tasks.BaseOptions
    FaceLandmarker = mp.tasks.vision.FaceLandmarker
    FaceLandmarkerOptions = mp.tasks.vision.FaceLandmarkerOptions
    VisionRunningMode = mp.tasks.vision.RunningMode

    options = FaceLandmarkerOptions(
        base_options=BaseOptions(model_asset_path=model_path),
        running_mode=VisionRunningMode.VIDEO,
        num_faces=1,
        min_face_detection_confidence=0.5,
        min_face_presence_confidence=0.5,
        min_tracking_confidence=0.5,
        output_face_blendshapes=True,
        output_facial_transformation_matrixes=False,
    )

    landmarker = FaceLandmarker.create_from_options(options)

    # ── Setup Camera ──
    cap = cv2.VideoCapture(args.camera)
    if not cap.isOpened():
        print(f"ERROR: Cannot open camera {args.camera}")
        sys.exit(1)

    cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
    cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
    cap.set(cv2.CAP_PROP_FPS, args.fps)

    w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    print(f"Camera: {w}x{h}")

    frame_time = 1.0 / args.fps
    frame_count = 0
    fps_timer = time.time()
    actual_fps = 0.0
    timestamp_ms = 0

    print("Tracking... (Ctrl+C or 'q' to quit)")
    print()

    try:
        while True:
            t0 = time.time()

            ret, frame = cap.read()
            if not ret:
                print("Camera read failed")
                break

            # Flip horizontally for mirror effect
            frame = cv2.flip(frame, 1)

            # Convert BGR to RGB for MediaPipe
            rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)

            # Detect face landmarks
            timestamp_ms += int(frame_time * 1000)
            result = landmarker.detect_for_video(mp_image, timestamp_ms)

            if result.face_landmarks and result.face_blendshapes:
                landmarks = result.face_landmarks[0]
                blendshapes = get_blendshape_dict(result.face_blendshapes[0])
                features = extract_features(blendshapes, landmarks)

                # Send UDP packet to all targets
                packet = pack_nerve_packet(features)
                for t in targets:
                    sock.sendto(packet, t)

                # Status display
                frame_count += 1
                now = time.time()
                if now - fps_timer >= 1.0:
                    actual_fps = frame_count / (now - fps_timer)
                    frame_count = 0
                    fps_timer = now
                    print(f"\r  {actual_fps:.0f} fps | "
                          f"head({features['headX']:+.2f},{features['headY']:+.2f}) "
                          f"eyes({features['leftEye']:.2f},{features['rightEye']:.2f}) "
                          f"brow({features['browL']:.2f},{features['browR']:.2f}) "
                          f"mouth({features['mouthW']:.2f},{features['mouthH']:.2f}) "
                          f"tongue({features['tongue']:.2f}) "
                          f"blink({'L' if features['blinkL'] else '-'}{'R' if features['blinkR'] else '-'})",
                          end='', flush=True)

                # Draw landmarks and feature overlays if showing preview
                if args.show:
                    draw_landmarks_on_frame(frame, landmarks, w, h)
                    draw_feature_overlay(frame, features, w, h)
            else:
                if args.show:
                    cv2.putText(frame, "No face detected", (20, 40),
                                cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)

            if args.show:
                cv2.putText(frame, f"NERVE Bridge | {actual_fps:.0f} fps", (20, h - 20),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (100, 255, 100), 1)
                cv2.imshow('NERVE Bridge', frame)
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break

            # Frame rate limiting
            elapsed = time.time() - t0
            if elapsed < frame_time:
                time.sleep(frame_time - elapsed)

    except KeyboardInterrupt:
        print("\n\nStopped.")

    finally:
        cap.release()
        if args.show:
            cv2.destroyAllWindows()
        sock.close()
        landmarker.close()
        print("NERVE Bridge closed.")


if __name__ == '__main__':
    main()
