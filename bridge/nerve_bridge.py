#!/usr/bin/env python3
"""
NERVE Bridge — Face Tracking to VCV Rack UDP
Captures webcam via MediaPipe Face Mesh, extracts 17 facial features,
sends binary UDP packets to NERVE module on localhost:9000.

Usage:
    pip install mediapipe opencv-python
    python nerve_bridge.py [--port 9000] [--camera 0] [--show]

Protocol: 84-byte packets
    Bytes 0-3:   Magic "NERV"
    Bytes 4-5:   Protocol version (uint16, little-endian) = 1
    Bytes 6-7:   Face count (uint16, little-endian) = 1
    Bytes 8-75:  17 floats (float32, little-endian) = face data
    Bytes 76-83: Timestamp (uint64, little-endian) in microseconds
"""

import argparse
import socket
import struct
import sys
import time

import cv2
import mediapipe as mp
import numpy as np


# ── MediaPipe Face Mesh landmark indices ──────────────────────

# Key landmarks for feature extraction
# See: https://github.com/google/mediapipe/blob/master/mediapipe/modules/face_geometry/data/canonical_face_model_uv_visualization.png

# Eyes
LEFT_EYE_TOP = 159
LEFT_EYE_BOTTOM = 145
LEFT_EYE_INNER = 133
LEFT_EYE_OUTER = 33

RIGHT_EYE_TOP = 386
RIGHT_EYE_BOTTOM = 374
RIGHT_EYE_INNER = 362
RIGHT_EYE_OUTER = 263

# Iris (from refined landmarks)
LEFT_IRIS_CENTER = 468
RIGHT_IRIS_CENTER = 473

# Eyebrows
LEFT_BROW_INNER = 107
LEFT_BROW_MID = 105
LEFT_BROW_OUTER = 70
RIGHT_BROW_INNER = 336
RIGHT_BROW_MID = 334
RIGHT_BROW_OUTER = 300

# Mouth
MOUTH_LEFT = 61
MOUTH_RIGHT = 291
MOUTH_TOP = 13
MOUTH_BOTTOM = 14
UPPER_LIP_TOP = 0
LOWER_LIP_BOTTOM = 17

# Jaw / chin
JAW_TIP = 152

# Nose (reference points)
NOSE_TIP = 1
NOSE_BRIDGE = 6

# Forehead reference
FOREHEAD_CENTER = 10


def distance(a, b):
    """Euclidean distance between two landmark points."""
    return np.sqrt((a.x - b.x)**2 + (a.y - b.y)**2 + (a.z - b.z)**2)


def distance_2d(a, b):
    """2D distance (x, y only)."""
    return np.sqrt((a.x - b.x)**2 + (a.y - b.y)**2)


class FaceFeatureExtractor:
    """Extracts 17 normalized features from MediaPipe Face Mesh landmarks."""

    def __init__(self):
        # Calibration baselines (updated over first few frames)
        self.baseline_face_height = None
        self.baseline_brow_height_l = None
        self.baseline_brow_height_r = None
        self.calibration_frames = 0
        self.calibration_target = 30  # frames to average

        # Running averages for calibration
        self._cal_face_h = []
        self._cal_brow_l = []
        self._cal_brow_r = []

    def extract(self, landmarks) -> dict:
        """Extract 17 features from face mesh landmarks.

        Returns dict with keys matching NERVE protocol order:
            headX, headY, headZ, headDist,
            leftEye, rightEye, gazeX, gazeY,
            mouthW, mouthH, jaw, lips,
            browL, browR, blinkL, blinkR, expression
        """
        lm = landmarks

        # Reference measurements for normalization
        face_height = distance_2d(lm[FOREHEAD_CENTER], lm[JAW_TIP])

        # Calibrate baseline
        if self.calibration_frames < self.calibration_target:
            self._cal_face_h.append(face_height)
            self.calibration_frames += 1
            if self.calibration_frames == self.calibration_target:
                self.baseline_face_height = np.mean(self._cal_face_h)

        if self.baseline_face_height is None:
            self.baseline_face_height = face_height

        norm = self.baseline_face_height if self.baseline_face_height > 0.01 else 0.15

        # ── Head pose (from nose position relative to face center) ──
        nose = lm[NOSE_TIP]
        # X: left-right rotation (yaw) — nose tip horizontal displacement
        head_x = (nose.x - 0.5) * 2.0  # -1 to 1
        # Y: up-down rotation (pitch) — nose tip vertical displacement
        head_y = -(nose.y - 0.5) * 2.0  # -1 to 1 (inverted: up = positive)
        # Z: tilt (roll) — angle between eyes
        left_eye_center = lm[LEFT_EYE_INNER]
        right_eye_center = lm[RIGHT_EYE_INNER]
        eye_angle = np.arctan2(
            right_eye_center.y - left_eye_center.y,
            right_eye_center.x - left_eye_center.x
        )
        head_z = np.clip(eye_angle / (np.pi / 4), -1.0, 1.0)  # normalized tilt

        # Distance — face height relative to baseline (closer = larger)
        head_dist = np.clip(face_height / norm, 0.0, 2.0) / 2.0

        # ── Eyes (openness) ──
        left_eye_open = distance_2d(lm[LEFT_EYE_TOP], lm[LEFT_EYE_BOTTOM]) / norm
        right_eye_open = distance_2d(lm[RIGHT_EYE_TOP], lm[RIGHT_EYE_BOTTOM]) / norm
        # Normalize to ~0-1 range (typical open eye is ~0.04-0.06 of face height)
        left_eye = np.clip(left_eye_open / 0.06, 0.0, 1.0)
        right_eye = np.clip(right_eye_open / 0.06, 0.0, 1.0)

        # ── Gaze (iris position within eye) ──
        try:
            # Iris landmarks available with refine_landmarks=True
            left_iris = lm[LEFT_IRIS_CENTER]
            li_inner = lm[LEFT_EYE_INNER]
            li_outer = lm[LEFT_EYE_OUTER]
            eye_width_l = distance_2d(li_inner, li_outer)
            if eye_width_l > 0.001:
                gaze_x_l = (left_iris.x - (li_inner.x + li_outer.x) / 2) / (eye_width_l / 2)
            else:
                gaze_x_l = 0.0

            right_iris = lm[RIGHT_IRIS_CENTER]
            ri_inner = lm[RIGHT_EYE_INNER]
            ri_outer = lm[RIGHT_EYE_OUTER]
            eye_width_r = distance_2d(ri_inner, ri_outer)
            if eye_width_r > 0.001:
                gaze_x_r = (right_iris.x - (ri_inner.x + ri_outer.x) / 2) / (eye_width_r / 2)
            else:
                gaze_x_r = 0.0

            gaze_x = np.clip((gaze_x_l + gaze_x_r) / 2, -1.0, 1.0)

            # Vertical gaze
            left_eye_mid_y = (lm[LEFT_EYE_TOP].y + lm[LEFT_EYE_BOTTOM].y) / 2
            right_eye_mid_y = (lm[RIGHT_EYE_TOP].y + lm[RIGHT_EYE_BOTTOM].y) / 2
            gaze_y_l = (left_iris.y - left_eye_mid_y) / (left_eye_open + 0.001)
            gaze_y_r = (right_iris.y - right_eye_mid_y) / (right_eye_open + 0.001)
            gaze_y = np.clip(-(gaze_y_l + gaze_y_r) / 2, -1.0, 1.0)
        except (IndexError, AttributeError):
            gaze_x = 0.0
            gaze_y = 0.0

        # ── Mouth ──
        mouth_w_raw = distance_2d(lm[MOUTH_LEFT], lm[MOUTH_RIGHT]) / norm
        mouth_h_raw = distance_2d(lm[MOUTH_TOP], lm[MOUTH_BOTTOM]) / norm
        mouth_w = np.clip(mouth_w_raw / 0.35, 0.0, 1.0)
        mouth_h = np.clip(mouth_h_raw / 0.15, 0.0, 1.0)

        # Jaw openness — distance from chin to nose
        jaw_raw = distance_2d(lm[JAW_TIP], lm[NOSE_TIP]) / norm
        jaw = np.clip((jaw_raw - 0.3) / 0.2, 0.0, 1.0)  # offset baseline

        # Lip pursing — upper to lower lip distance vs mouth height
        lip_dist = distance_2d(lm[UPPER_LIP_TOP], lm[LOWER_LIP_BOTTOM]) / norm
        lips = np.clip(lip_dist / 0.15, 0.0, 1.0)

        # ── Eyebrows ──
        brow_l_height = distance_2d(lm[LEFT_BROW_MID], lm[LEFT_EYE_TOP]) / norm
        brow_r_height = distance_2d(lm[RIGHT_BROW_MID], lm[RIGHT_EYE_TOP]) / norm
        brow_l = np.clip(brow_l_height / 0.06, 0.0, 1.0)
        brow_r = np.clip(brow_r_height / 0.06, 0.0, 1.0)

        # ── Blinks (binary — eye aspect ratio threshold) ──
        EAR_THRESHOLD = 0.2
        blink_l = 1.0 if left_eye < EAR_THRESHOLD else 0.0
        blink_r = 1.0 if right_eye < EAR_THRESHOLD else 0.0

        # ── Expression (composite — mouth + brow activity) ──
        expression = np.clip(
            (mouth_h * 0.4 + abs(brow_l - 0.5) * 0.3 + abs(brow_r - 0.5) * 0.3),
            0.0, 1.0
        )

        return {
            'headX': float(head_x),
            'headY': float(head_y),
            'headZ': float(head_z),
            'headDist': float(head_dist),
            'leftEye': float(left_eye),
            'rightEye': float(right_eye),
            'gazeX': float(gaze_x),
            'gazeY': float(gaze_y),
            'mouthW': float(mouth_w),
            'mouthH': float(mouth_h),
            'jaw': float(jaw),
            'lips': float(lips),
            'browL': float(brow_l),
            'browR': float(brow_r),
            'blinkL': float(blink_l),
            'blinkR': float(blink_r),
            'expression': float(expression),
        }


def pack_nerve_packet(features: dict) -> bytes:
    """Pack face features into 84-byte NERVE protocol packet."""
    magic = b'NERV'
    version = struct.pack('<H', 1)
    face_count = struct.pack('<H', 1)

    # 17 floats in protocol order
    floats = struct.pack('<17f',
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
    )

    timestamp = struct.pack('<Q', int(time.time() * 1_000_000))

    return magic + version + face_count + floats + timestamp


def main():
    parser = argparse.ArgumentParser(description='NERVE Bridge — Face to VCV Rack')
    parser.add_argument('--port', type=int, default=9000, help='UDP port (default: 9000)')
    parser.add_argument('--host', type=str, default='127.0.0.1', help='Target host (default: 127.0.0.1)')
    parser.add_argument('--camera', type=int, default=0, help='Camera index (default: 0)')
    parser.add_argument('--show', action='store_true', help='Show camera preview window')
    parser.add_argument('--fps', type=int, default=30, help='Target FPS (default: 30)')
    args = parser.parse_args()

    # ── Setup UDP ──
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    target = (args.host, args.port)
    print(f"NERVE Bridge → {args.host}:{args.port}")

    # ── Setup MediaPipe ──
    mp_face = mp.solutions.face_mesh
    face_mesh = mp_face.FaceMesh(
        static_image_mode=False,
        max_num_faces=1,
        refine_landmarks=True,  # enables iris tracking (landmarks 468-477)
        min_detection_confidence=0.5,
        min_tracking_confidence=0.5,
    )

    if args.show:
        mp_drawing = mp.solutions.drawing_utils
        mp_drawing_styles = mp.solutions.drawing_styles

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

    extractor = FaceFeatureExtractor()
    frame_time = 1.0 / args.fps
    frame_count = 0
    fps_timer = time.time()
    actual_fps = 0.0

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
            rgb.flags.writeable = False
            results = face_mesh.process(rgb)
            rgb.flags.writeable = True

            if results.multi_face_landmarks:
                face_landmarks = results.multi_face_landmarks[0]
                features = extractor.extract(face_landmarks.landmark)

                # Send UDP packet
                packet = pack_nerve_packet(features)
                sock.sendto(packet, target)

                # Status display
                frame_count += 1
                now = time.time()
                if now - fps_timer >= 1.0:
                    actual_fps = frame_count / (now - fps_timer)
                    frame_count = 0
                    fps_timer = now
                    # Print compact status line
                    print(f"\r  {actual_fps:.0f} fps | "
                          f"head({features['headX']:+.2f},{features['headY']:+.2f}) "
                          f"eyes({features['leftEye']:.2f},{features['rightEye']:.2f}) "
                          f"mouth({features['mouthW']:.2f},{features['mouthH']:.2f}) "
                          f"blink({'L' if features['blinkL'] else '-'}{'R' if features['blinkR'] else '-'})",
                          end='', flush=True)

                # Draw face mesh if showing preview
                if args.show:
                    mp_drawing.draw_landmarks(
                        image=frame,
                        landmark_list=face_landmarks,
                        connections=mp_face.FACEMESH_TESSELATION,
                        landmark_drawing_spec=None,
                        connection_drawing_spec=mp_drawing_styles.get_default_face_mesh_tesselation_style(),
                    )
                    mp_drawing.draw_landmarks(
                        image=frame,
                        landmark_list=face_landmarks,
                        connections=mp_face.FACEMESH_IRISES,
                        landmark_drawing_spec=None,
                        connection_drawing_spec=mp_drawing_styles.get_default_face_mesh_iris_connections_style(),
                    )
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
        face_mesh.close()
        print("NERVE Bridge closed.")


if __name__ == '__main__':
    main()
