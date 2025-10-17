import cv2
import csv
import os
import numpy as np
import mediapipe as mp
from yolo_tracker.tracker import normalize_pose_keypoints

# MediaPipe 설정
mp_pose = mp.solutions.pose
pose = mp_pose.Pose(min_detection_confidence=0.5, min_tracking_confidence=0.5)
mp_drawing = mp.solutions.drawing_utils

cap = cv2.VideoCapture(0)
if not cap.isOpened():
    print("Error: Cannot open webcam")
    exit()

CSV_FILE = 'feedback_data.csv'
file_exists = os.path.isfile(CSV_FILE)

print("=== MediaPipe Pose Data Collection ===")
print("Controls:")
print("  'c' - Capture current pose")
print("  'q' - Quit")
print("  's' - Show statistics")

pose_count = {}

while True:
    success, frame = cap.read()
    if not success:
        break

    frame = cv2.flip(frame, 1)
    image_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
    image_rgb.flags.writeable = False
    results = pose.process(image_rgb)
    image_rgb.flags.writeable = True
    
    annotated_frame = frame.copy()
    person_detected = False
    
    if results.pose_landmarks:
        person_detected = True
        mp_drawing.draw_landmarks(
            annotated_frame, results.pose_landmarks, mp_pose.POSE_CONNECTIONS,
            landmark_drawing_spec=mp_drawing.DrawingSpec(color=(0,255,0), thickness=2, circle_radius=3),
            connection_drawing_spec=mp_drawing.DrawingSpec(color=(0,255,255), thickness=2)
        )

    status_color = (0, 255, 0) if person_detected else (0, 0, 255)
    status_text = "Person Detected - Ready to Capture" if person_detected else "No Person Detected"
    cv2.putText(annotated_frame, status_text, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, status_color, 2)
    cv2.putText(annotated_frame, "Press 'c' to capture, 's' for stats, 'q' to quit", (10, 60), 
                cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)

    if pose_count:
        y_pos = 90
        cv2.putText(annotated_frame, "Collected Poses:", (10, y_pos), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 0), 1)
        for pose_name, count in pose_count.items():
            y_pos += 20
            cv2.putText(annotated_frame, f"  {pose_name}: {count}", (10, y_pos), cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 255, 0), 1)

    cv2.imshow("MediaPipe Pose Data Collection", annotated_frame)
    key = cv2.waitKey(1) & 0xFF

    if key == ord('q'):
        break
    elif key == ord('s'):
        print("\n=== Statistics ===")
        if pose_count:
            for pose_name, count in pose_count.items():
                print(f"  {pose_name}: {count} samples")
            print(f"  Total: {sum(pose_count.values())} samples")
        else:
            print("  No poses collected yet")
    elif key == ord('c'):
        if results.pose_landmarks:
            landmarks = results.pose_landmarks.landmark
            h, w, _ = frame.shape
            
            keypoints_raw = []
            for landmark in landmarks:
                keypoints_raw.append([landmark.x * w, landmark.y * h, landmark.visibility])
            keypoints_raw = np.array(keypoints_raw)
            
            normalized_keypoints = normalize_pose_keypoints(keypoints_raw)
            
            if normalized_keypoints is not None:
                print(f"\nPose detected with {len(landmarks)} landmarks")
                label = input("Enter pose name: ").strip()
                
                if label:
                    with open(CSV_FILE, 'a', newline='') as f:
                        writer = csv.writer(f)
                        if not file_exists:
                            header = ['label'] + [f'feat_{i}' for i in range(len(normalized_keypoints.flatten()))]
                            writer.writerow(header)
                            file_exists = True
                        writer.writerow([label] + list(normalized_keypoints.flatten()))
                    
                    pose_count[label] = pose_count.get(label, 0) + 1
                    print(f"Saved pose '{label}' (#{pose_count[label]})")
                    
                    total_samples = sum(pose_count.values())
                    unique_poses = len(pose_count)
                    
                    if total_samples >= 3 and unique_poses >= 2:
                        print(f"\nTip: You have {total_samples} samples across {unique_poses} poses.")
                        print("You can now train a model by running: python train.py")
            else:
                print("Failed to normalize pose")
        else:
            print("No person detected")

cap.release()
cv2.destroyAllWindows()

print(f"\nData collection completed!")
if pose_count:
    print("Final statistics:")
    for pose_name, count in pose_count.items():
        print(f"  {pose_name}: {count} samples")
    print(f"Total: {sum(pose_count.values())} samples saved to '{CSV_FILE}'")
    print("To train a model, run: python train.py")