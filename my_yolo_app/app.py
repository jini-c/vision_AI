from flask import Flask, render_template, Response, request, jsonify
from yolo_tracker.tracker import VideoTracker, normalize_pose_keypoints
import os
import csv
import numpy as np
import pandas as pd
from sklearn.ensemble import RandomForestClassifier
from sklearn.preprocessing import LabelEncoder
import joblib
import threading
import time

app = Flask(__name__)

# Global State
tracker = VideoTracker()
FEEDBACK_FILE = 'feedback_data.csv'
RETRAIN_THRESHOLD = 5
feedback_counter = 0
is_training = False
training_status = ""

def train_model_task():
    global is_training, feedback_counter, training_status
    is_training = True
    training_status = "Loading data..."
    print("\n--- [TRAINING] Starting background model training... ---")
    
    if not os.path.exists(FEEDBACK_FILE):
        print("[TRAINING] Feedback data file not found.")
        is_training = False
        training_status = ""
        return

    try:
        training_status = "Reading feedback data..."
        df = pd.read_csv(FEEDBACK_FILE)
        
        if df.shape[0] < 3:
            print(f"[TRAINING] Not enough data. Found {df.shape[0]} samples.")
            is_training = False
            training_status = ""
            return
        
        if df['label'].nunique() < 2:
            print(f"[TRAINING] Not enough classes. Found {df['label'].nunique()} classes.")
            is_training = False
            training_status = ""
            return
            
    except Exception as e:
        print(f"[TRAINING] Error reading data: {e}")
        is_training = False
        training_status = ""
        return

    print(f"[TRAINING] Training with {df.shape[0]} data points across {df['label'].nunique()} classes.")
    training_status = f"Training model with {df.shape[0]} samples..."
    
    X = df.drop('label', axis=1)
    y = df['label']
    le = LabelEncoder()
    y_encoded = le.fit_transform(y)
    
    model = RandomForestClassifier(
        n_estimators=50,
        random_state=42, 
        class_weight='balanced',
        max_depth=10
    )
    
    model.fit(X, y_encoded)
    train_score = model.score(X, y_encoded)
    print(f"[TRAINING] Training complete. Accuracy: {train_score:.2%}")

    training_status = "Saving model..."
    joblib.dump(model, 'pose_classifier.joblib')
    joblib.dump(le, 'label_encoder.pkl')
    print("[TRAINING] Model saved.")

    training_status = "Reloading model..."
    tracker.load_model()
    feedback_counter = 0
    is_training = False
    training_status = ""
    print("--- [TRAINING] Finished. ---")

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/video_feed')
def video_feed():
    def gen(tracker_instance):
        yield from tracker_instance.generate_frames_for_stream()
    return Response(gen(tracker), mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/api/current_pose', methods=['GET'])
def get_current_pose():
    pose_info = tracker.latest_pose_info.copy()
    pose_info['is_training'] = is_training
    pose_info['training_status'] = training_status
    pose_info['feedback_count'] = feedback_counter
    pose_info['model_loaded'] = tracker.classifier is not None
    return jsonify(pose_info)

@app.route('/api/feedback', methods=['POST'])
def handle_feedback():
    global feedback_counter
    data = request.json
    true_label = data.get('true_label', '').strip()

    if not true_label:
        return jsonify({"error": "True label is required"}), 400

    # Debug information
    print(f"\n=== FEEDBACK DEBUG ===")
    print(f"Requested label: '{true_label}'")
    print(f"Latest keypoints is None: {tracker.latest_keypoints is None}")
    print(f"Person detected: {tracker.person_detected}")
    print(f"Latest pose info: {tracker.latest_pose_info}")

    keypoints_to_save = tracker.latest_keypoints
    if keypoints_to_save is None:
        return jsonify({"error": "No pose detected - please ensure you're visible in the camera"}), 400

    normalized_keypoints = normalize_pose_keypoints(keypoints_to_save)
    if normalized_keypoints is None:
        return jsonify({"error": "Could not normalize pose - please try a different position"}), 400

    print(f"Keypoints shape: {keypoints_to_save.shape}")
    print(f"Normalized keypoints shape: {normalized_keypoints.shape}")

    # Save feedback
    file_exists = os.path.isfile(FEEDBACK_FILE)
    try:
        with open(FEEDBACK_FILE, 'a', newline='') as f:
            writer = csv.writer(f)
            if not file_exists:
                header = ['label'] + [f'feat_{i}' for i in range(len(normalized_keypoints.flatten()))]
                writer.writerow(header)
            writer.writerow([true_label] + list(normalized_keypoints.flatten()))
        
        feedback_counter += 1
        print(f"Feedback saved successfully. Count: {feedback_counter}")
    except Exception as e:
        print(f"Error saving feedback: {e}")
        return jsonify({"error": f"Failed to save feedback: {str(e)}"}), 500

    should_retrain = feedback_counter >= RETRAIN_THRESHOLD and not is_training
    
    response_message = f"Feedback for '{true_label}' recorded."
    if should_retrain:
        response_message += " Auto-training will start..."
        threading.Thread(target=train_model_task).start()
    elif feedback_counter < RETRAIN_THRESHOLD:
        remaining = RETRAIN_THRESHOLD - feedback_counter
        response_message += f" ({remaining} more needed for auto-training)"
    
    return jsonify({
        "status": "success", 
        "message": response_message,
        "feedback_count": feedback_counter,
        "will_retrain": should_retrain
    })

@app.route('/api/learned_labels', methods=['GET'])
def get_learned_labels():
    labels = []
    if tracker.label_encoder:
        labels = list(tracker.label_encoder.classes_)
    
    label_counts = {}
    if os.path.exists(FEEDBACK_FILE):
        try:
            df = pd.read_csv(FEEDBACK_FILE)
            label_counts = df['label'].value_counts().to_dict()
        except:
            pass
    
    return jsonify({
        "labels": labels,
        "counts": label_counts,
        "total_feedback": sum(label_counts.values()) if label_counts else 0
    })

@app.route('/api/manual_train', methods=['POST'])
def manual_train():
    global is_training
    
    if is_training:
        return jsonify({"error": "Training already in progress"}), 400
    
    if not os.path.exists(FEEDBACK_FILE):
        return jsonify({"error": "No feedback data available"}), 400
    
    threading.Thread(target=train_model_task).start()
    return jsonify({"message": "Manual training started"})

@app.route('/api/define_pose', methods=['POST'])
def define_pose():
    data = request.json
    pose_name = data.get('pose_name', '').strip()
    
    if not pose_name:
        return jsonify({"error": "Pose name is required"}), 400
    
    # Enhanced debug information
    print(f"\n=== DEFINE POSE DEBUG ===")
    print(f"Pose name: '{pose_name}'")
    print(f"Tracker status: {tracker.get_current_status()}")
    
    current_keypoints = tracker.latest_keypoints
    if current_keypoints is None:
        print("ERROR: No keypoints available")
        return jsonify({"error": "No person detected. Please ensure you're visible in the camera."}), 400
    
    normalized_keypoints = normalize_pose_keypoints(current_keypoints)
    if normalized_keypoints is None:
        print("ERROR: Normalization failed")
        return jsonify({"error": "Could not normalize current pose. Please try a different position."}), 400
    
    print(f"Keypoints shape: {current_keypoints.shape}")
    print(f"Normalized keypoints shape: {normalized_keypoints.shape}")
    
    # Save as feedback data
    file_exists = os.path.isfile(FEEDBACK_FILE)
    try:
        with open(FEEDBACK_FILE, 'a', newline='') as f:
            writer = csv.writer(f)
            if not file_exists:
                header = ['label'] + [f'feat_{i}' for i in range(len(normalized_keypoints.flatten()))]
                writer.writerow(header)
            writer.writerow([pose_name] + list(normalized_keypoints.flatten()))
        
        print(f"Pose '{pose_name}' saved successfully")
        return jsonify({
            "message": f"Pose '{pose_name}' defined successfully!",
            "pose_name": pose_name
        })
    except Exception as e:
        print(f"Error saving pose: {e}")
        return jsonify({"error": f"Failed to save pose: {str(e)}"}), 500

@app.route('/api/stats', methods=['GET'])
def get_stats():
    stats = {
        "model_loaded": tracker.classifier is not None,
        "is_training": is_training,
        "training_status": training_status,
        "feedback_count": feedback_counter,
        "retrain_threshold": RETRAIN_THRESHOLD,
        "feedback_file_exists": os.path.exists(FEEDBACK_FILE),
        "person_detected": tracker.person_detected,
        "has_keypoints": tracker.latest_keypoints is not None
    }
    
    if os.path.exists(FEEDBACK_FILE):
        try:
            df = pd.read_csv(FEEDBACK_FILE)
            stats.update({
                "total_samples": len(df),
                "unique_poses": df['label'].nunique(),
                "pose_distribution": df['label'].value_counts().to_dict()
            })
        except:
            pass
    
    return jsonify(stats)

@app.route('/api/debug', methods=['GET'])
def debug_info():
    """Debug endpoint to check tracker status."""
    return jsonify(tracker.get_current_status())

if __name__ == '__main__':
    print("🚀 Starting ML-Powered Pose Trainer...")
    print(f"📊 Auto-training threshold: {RETRAIN_THRESHOLD} feedback samples")
    print("💡 Provide feedback to train your personalized model!")
    app.run(debug=True, threaded=True, use_reloader=False)