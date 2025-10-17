import cv2
import numpy as np
import mediapipe as mp
import joblib
import os

def normalize_pose_keypoints(keypoints):
    """Normalizes 33 pose keypoints from MediaPipe to be invariant to position and scale."""
    try:
        left_shoulder = keypoints[11, :2]
        right_shoulder = keypoints[12, :2]
        left_hip = keypoints[23, :2]
        right_hip = keypoints[24, :2]

        torso_center = np.mean([left_shoulder, right_shoulder, left_hip, right_hip], axis=0)
        shoulder_midpoint = np.mean([left_shoulder, right_shoulder], axis=0)
        hip_midpoint = np.mean([left_hip, right_hip], axis=0)
        torso_size = np.linalg.norm(shoulder_midpoint - hip_midpoint)

        if torso_size < 1e-6:
            return None

        normalized_keypoints = (keypoints[:, :2] - torso_center) / torso_size
        return np.hstack((normalized_keypoints, keypoints[:, 2:]))
    except (IndexError, ValueError) as e:
        print(f"Normalization error: {e}")
        return None

class VideoTracker:
    def __init__(self):
        print("Initializing VideoTracker...")
        self.mp_pose = mp.solutions.pose
        self.pose = self.mp_pose.Pose(
            min_detection_confidence=0.3, 
            min_tracking_confidence=0.3,
            model_complexity=1
        )
        self.mp_drawing = mp.solutions.drawing_utils
        
        # Initialize camera
        self.cap = cv2.VideoCapture(0)
        if not self.cap.isOpened():
            raise IOError("Cannot open webcam")
            
        # Set camera properties
        self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
        self.cap.set(cv2.CAP_PROP_FPS, 30)
        
        # Initialize state
        self.classifier = None
        self.label_encoder = None
        self.latest_pose_info = {"name": "LOADING...", "probability": 0.0}
        self.latest_keypoints = None
        self.person_detected = False
        
        # Load model if available
        self.load_model()
        print("VideoTracker initialized successfully")

    def load_model(self):
        """Loads the scikit-learn model and label encoder."""
        model_path = 'pose_classifier.joblib'
        encoder_path = 'label_encoder.pkl'
        
        if os.path.exists(model_path) and os.path.exists(encoder_path):
            try:
                print("Loading ML model...")
                self.classifier = joblib.load(model_path)
                self.label_encoder = joblib.load(encoder_path)
                print(f"ML Model loaded successfully with {len(self.label_encoder.classes_)} classes:")
                print(f"  Classes: {list(self.label_encoder.classes_)}")
            except Exception as e:
                print(f"Error loading model: {e}")
                self.classifier = None
                self.label_encoder = None
        else:
            print("Model files not found. Please provide feedback to train one.")
            self.classifier = None
            self.label_encoder = None

    def predict_pose_with_model(self, keypoints_raw):
        """Predict pose using the trained model."""
        if self.classifier is None:
            return {"name": "NEEDS_TRAINING", "probability": 0.0}
        
        try:
            # Normalize keypoints
            normalized_keypoints = normalize_pose_keypoints(keypoints_raw)
            if normalized_keypoints is None:
                print("Failed to normalize keypoints")
                return {"name": "NORMALIZATION_FAIL", "probability": 0.0}

            # Make prediction
            features = normalized_keypoints.flatten().reshape(1, -1)
            prediction_proba = self.classifier.predict_proba(features)
            best_class_index = np.argmax(prediction_proba[0])
            
            pose_name = self.label_encoder.classes_[best_class_index]
            probability = prediction_proba[0][best_class_index]
            
            return {"name": pose_name, "probability": probability}
            
        except Exception as e:
            print(f"Prediction error: {e}")
            return {"name": "PREDICTION_ERROR", "probability": 0.0}

    def process_pose_landmarks(self, landmarks, frame_shape):
        """Process MediaPipe landmarks and update state."""
        try:
            h, w, _ = frame_shape
            
            # Convert landmarks to numpy array with pixel coordinates
            keypoints_raw = np.array([[lm.x * w, lm.y * h, lm.visibility] for lm in landmarks])
            
            # Store for feedback system
            self.latest_keypoints = keypoints_raw
            self.person_detected = True
            
            # Debug info
            print(f"Processed {len(landmarks)} landmarks, stored keypoints shape: {keypoints_raw.shape}")
            
            # Predict pose if model is available
            return self.predict_pose_with_model(keypoints_raw)
            
        except Exception as e:
            print(f"Error processing landmarks: {e}")
            self.latest_keypoints = None
            self.person_detected = False
            return {"name": "PROCESSING_ERROR", "probability": 0.0}

    def generate_frames_for_stream(self):
        """Generate video frames with pose detection for streaming."""
        frame_count = 0
        
        while True:
            success, frame = self.cap.read()
            if not success:
                print("Failed to read frame from camera")
                break

            frame_count += 1
            
            # Flip frame horizontally for mirror effect
            frame = cv2.flip(frame, 1)
            
            # Convert BGR to RGB for MediaPipe
            rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            rgb_frame.flags.writeable = False
            
            # Process with MediaPipe
            results = self.pose.process(rgb_frame)
            rgb_frame.flags.writeable = True
            
            # Create output frame
            output_frame = frame.copy()
            
            # Process results
            if results.pose_landmarks:
                # Draw pose landmarks
                self.mp_drawing.draw_landmarks(
                    output_frame,
                    results.pose_landmarks,
                    self.mp_pose.POSE_CONNECTIONS,
                    landmark_drawing_spec=self.mp_drawing.DrawingSpec(
                        color=(245, 117, 66), thickness=2, circle_radius=2
                    ),
                    connection_drawing_spec=self.mp_drawing.DrawingSpec(
                        color=(245, 66, 230), thickness=2, circle_radius=2
                    )
                )
                
                # Process landmarks and update pose info
                self.latest_pose_info = self.process_pose_landmarks(
                    results.pose_landmarks.landmark, frame.shape
                )
                
                # Add visual feedback
                cv2.putText(output_frame, "PERSON DETECTED", (10, 30),
                           cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
                
                # Show current pose prediction
                pose_text = f"{self.latest_pose_info['name']} ({int(self.latest_pose_info['probability']*100)}%)"
                cv2.putText(output_frame, pose_text, (10, 70),
                           cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 2)
                
            else:
                # No person detected
                self.latest_pose_info = {"name": "NO_PERSON", "probability": 0.0}
                self.latest_keypoints = None
                self.person_detected = False
                
                cv2.putText(output_frame, "NO PERSON DETECTED", (10, 30),
                           cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2)
                cv2.putText(output_frame, "Move closer or improve lighting", (10, 60),
                           cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1)
            
            # Debug output every 60 frames
            if frame_count % 60 == 0:
                print(f"Frame {frame_count}: Person={self.person_detected}, "
                      f"Pose={self.latest_pose_info['name']}, "
                      f"Keypoints={'Available' if self.latest_keypoints is not None else 'None'}")
            
            # Encode frame for streaming
            try:
                ret, buffer = cv2.imencode('.jpg', output_frame, 
                                         [cv2.IMWRITE_JPEG_QUALITY, 85])
                if not ret:
                    print("Failed to encode frame")
                    continue
                    
                frame_bytes = buffer.tobytes()
                yield (b'--frame\r\n'
                       b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')
                       
            except Exception as e:
                print(f"Streaming error: {e}")
                continue

    def get_current_status(self):
        """Get current detection and model status for API."""
        return {
            "person_detected": self.person_detected,
            "has_keypoints": self.latest_keypoints is not None,
            "keypoints_shape": self.latest_keypoints.shape if self.latest_keypoints is not None else None,
            "model_loaded": self.classifier is not None,
            "pose_info": self.latest_pose_info.copy()
        }

    def __del__(self):
        """Clean up resources."""
        try:
            if hasattr(self, 'cap') and self.cap and self.cap.isOpened():
                self.cap.release()
            if hasattr(self, 'pose') and self.pose:
                self.pose.close()
            print("VideoTracker cleaned up successfully")
        except Exception as e:
            print(f"Cleanup error: {e}")