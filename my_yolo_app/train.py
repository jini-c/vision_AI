import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split
from sklearn.ensemble import RandomForestClassifier
from sklearn.preprocessing import LabelEncoder
from sklearn.metrics import accuracy_score, classification_report
import joblib
import os

def train_pose_classifier():
    """Train a pose classifier from feedback data."""
    
    # Check if feedback data exists
    feedback_file = 'feedback_data.csv'
    if not os.path.exists(feedback_file):
        print("❌ No feedback data found. Please provide some feedback first.")
        return False
    
    try:
        # Load data
        print("📊 Loading training data...")
        df = pd.read_csv(feedback_file)
        print(f"   - Found {len(df)} samples")
        print(f"   - Found {df['label'].nunique()} unique poses: {list(df['label'].unique())}")
        
        # Check minimum requirements
        if len(df) < 3:
            print("❌ Need at least 3 samples to train. Please provide more feedback.")
            return False
        
        if df['label'].nunique() < 2:
            print("❌ Need at least 2 different poses to train. Please provide feedback for different poses.")
            return False
        
        # Prepare features and labels
        X = df.drop('label', axis=1)
        y = df['label']
        
        # Encode labels
        label_encoder = LabelEncoder()
        y_encoded = label_encoder.fit_transform(y)
        
        print(f"🔄 Training model with {len(X.columns)} features...")
        
        # Train model
        model = RandomForestClassifier(
            n_estimators=50,
            random_state=42,
            class_weight='balanced',
            max_depth=10
        )
        
        # Train-test split if enough data
        if len(df) >= 10:
            X_train, X_test, y_train, y_test = train_test_split(
                X, y_encoded, test_size=0.2, random_state=42, stratify=y_encoded
            )
            model.fit(X_train, y_train)
            
            # Evaluate
            y_pred = model.predict(X_test)
            accuracy = accuracy_score(y_test, y_pred)
            print(f"✅ Model trained! Test accuracy: {accuracy:.2%}")
            
            # Detailed report
            print("\n📈 Classification Report:")
            print(classification_report(y_test, y_pred, target_names=label_encoder.classes_))
        else:
            # Train on all data if dataset is small
            model.fit(X, y_encoded)
            train_accuracy = model.score(X, y_encoded)
            print(f"✅ Model trained! Training accuracy: {train_accuracy:.2%}")
            print("ℹ️  Note: Small dataset - no test split performed")
        
        # Save model and encoder
        joblib.dump(model, 'pose_classifier.joblib')
        joblib.dump(label_encoder, 'label_encoder.pkl')
        print("💾 Model and label encoder saved!")
        
        return True
        
    except Exception as e:
        print(f"❌ Training failed: {str(e)}")
        return False

if __name__ == "__main__":
    print("🚀 Starting pose classifier training...")
    success = train_pose_classifier()
    
    if success:
        print("\n✨ Training completed successfully!")
        print("   The web app will now use the trained model for pose recognition.")
    else:
        print("\n💡 To fix this:")
        print("   1. Run the web app and provide feedback using the 👍/👎 buttons")
        print("   2. Make sure you have feedback for at least 2 different poses")
        print("   3. Provide at least 3 total feedback samples")
        print("   4. Run this script again")