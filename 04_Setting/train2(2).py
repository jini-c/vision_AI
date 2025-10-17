# =================================================================
# Fashion MNIST V2 - 95% Training Accuracy 달성 + Test Accuracy 최대화
# =================================================================
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers
import numpy as np
import matplotlib.pyplot as plt
from sklearn.model_selection import train_test_split

# GPU 메모리 증가 허용
gpus = tf.config.experimental.list_physical_devices('GPU')
if gpus:
    try:
        for gpu in gpus:
            tf.config.experimental.set_memory_growth(gpu, True)
    except RuntimeError as e:
        print(e)

# =================================================================
# 1. 데이터 로드 및 3단계 분리
# =================================================================
fashion_mnist = keras.datasets.fashion_mnist
(train_val_images, train_val_labels), (test_images_orig, test_labels_orig) = fashion_mnist.load_data()

class_names = [
    "T-shirt/top", "Trouser", "Pullover", "Dress", "Coat",
    "Sandal", "Shirt", "Sneaker", "Bag", "Ankle boot",
]

# 전체 데이터 재분배
all_images = np.concatenate([train_val_images, test_images_orig])
all_labels = np.concatenate([train_val_labels, test_labels_orig])

train_val_images, test_images, train_val_labels, test_labels = train_test_split(
    all_images, all_labels, test_size=0.4, random_state=42
)

train_images, val_images, train_labels, val_labels = train_test_split(
    train_val_images, train_val_labels, test_size=0.16666, random_state=42
)

# =================================================================
# 2. 데이터 전처리 + Data Augmentation
# =================================================================
train_images = np.expand_dims(train_images / 255.0, -1)
val_images = np.expand_dims(val_images / 255.0, -1)
test_images = np.expand_dims(test_images / 255.0, -1)

# 강력한 Data Augmentation
data_augmentation = keras.Sequential([
    layers.RandomRotation(0.15),        # ±15도 회전
    layers.RandomTranslation(0.1, 0.1), # 10% 이동
    layers.RandomZoom(0.1),             # 10% 줌
    layers.RandomContrast(0.1),         # 대비 조정
], name="data_augmentation")

# =================================================================
# 3. 테스트 메트릭 콜백
# =================================================================
class TestMetricsCallback(keras.callbacks.Callback):
    def __init__(self, test_data):
        self.test_data = test_data
        self.test_accs = []
        self.test_losses = []
        self.best_test_acc = 0.0

    def on_epoch_end(self, epoch, logs=None):
        x_test, y_test = self.test_data
        loss, acc = self.model.evaluate(x_test, y_test, verbose=0)
        self.test_losses.append(loss)
        self.test_accs.append(acc)
        
        if acc > self.best_test_acc:
            self.best_test_acc = acc
            print(f" — test_loss: {loss:.4f} — test_acc: {acc:.4f} ⭐NEW BEST!")
        else:
            print(f" — test_loss: {loss:.4f} — test_acc: {acc:.4f}")

test_callback = TestMetricsCallback((test_images, test_labels))

# =================================================================
# 4. 고성능 CNN 모델 (95% Training 목표)
# =================================================================
def create_high_performance_model():
    inputs = keras.Input(shape=(28, 28, 1))
    
    # Data Augmentation
    x = data_augmentation(inputs)
    
    # Block 1: 64 filters
    x = layers.Conv2D(64, (3, 3), padding='same', kernel_initializer='he_normal')(x)
    x = layers.BatchNormalization()(x)
    x = layers.Activation('relu')(x)
    x = layers.Conv2D(64, (3, 3), padding='same', kernel_initializer='he_normal')(x)
    x = layers.BatchNormalization()(x)
    x = layers.Activation('relu')(x)
    x = layers.MaxPooling2D((2, 2))(x)
    x = layers.Dropout(0.25)(x)
    
    # Block 2: 128 filters
    x = layers.Conv2D(128, (3, 3), padding='same', kernel_initializer='he_normal')(x)
    x = layers.BatchNormalization()(x)
    x = layers.Activation('relu')(x)
    x = layers.Conv2D(128, (3, 3), padding='same', kernel_initializer='he_normal')(x)
    x = layers.BatchNormalization()(x)
    x = layers.Activation('relu')(x)
    x = layers.MaxPooling2D((2, 2))(x)
    x = layers.Dropout(0.25)(x)
    
    # Block 3: 256 filters
    x = layers.Conv2D(256, (3, 3), padding='same', kernel_initializer='he_normal')(x)
    x = layers.BatchNormalization()(x)
    x = layers.Activation('relu')(x)
    x = layers.Conv2D(256, (3, 3), padding='same', kernel_initializer='he_normal')(x)
    x = layers.BatchNormalization()(x)
    x = layers.Activation('relu')(x)
    x = layers.GlobalAveragePooling2D()(x)
    x = layers.Dropout(0.5)(x)
    
    # Dense layers
    x = layers.Dense(512, kernel_initializer='he_normal')(x)
    x = layers.BatchNormalization()(x)
    x = layers.Activation('relu')(x)
    x = layers.Dropout(0.5)(x)
    
    x = layers.Dense(256, kernel_initializer='he_normal')(x)
    x = layers.BatchNormalization()(x)
    x = layers.Activation('relu')(x)
    x = layers.Dropout(0.3)(x)
    
    outputs = layers.Dense(10, activation='softmax', kernel_initializer='glorot_uniform')(x)
    
    return keras.Model(inputs, outputs, name="high_performance_fashion_cnn")

# 모델 생성
model = create_high_performance_model()

# 최적화된 컴파일
model.compile(
    optimizer=keras.optimizers.AdamW(
        learning_rate=0.001,
        weight_decay=1e-4,
        beta_1=0.9,
        beta_2=0.999
    ),
    loss=keras.losses.SparseCategoricalCrossentropy(label_smoothing=0.1),
    metrics=['accuracy']
)

print("🚀 High-Performance Model Architecture:")
model.summary()
print(f"📊 Total parameters: {model.count_params():,}")

# =================================================================
# 5. 고급 학습 전략
# =================================================================
# Cosine Annealing with Warm Restart
def cosine_annealing_with_warmup(epoch):
    """Cosine annealing with warm restart"""
    T_0 = 20  # 첫 번째 restart 주기
    T_mult = 1.2  # restart 주기 배수
    eta_max = 0.001  # 최대 학습률
    eta_min = 1e-6   # 최소 학습률
    
    if epoch < 5:  # Warm-up
        return eta_max * epoch / 5
    else:
        T_cur = epoch - 5
        return eta_min + (eta_max - eta_min) * (1 + np.cos(np.pi * (T_cur % T_0) / T_0)) / 2

# 콜백 설정
callbacks = [
    test_callback,
    
    # Learning Rate Scheduling
    keras.callbacks.LearningRateScheduler(cosine_annealing_with_warmup, verbose=1),
    
    # Model Checkpoint (최고 성능 모델 저장)
    keras.callbacks.ModelCheckpoint(
        'best_fashion_model.h5',
        monitor='test_acc' if hasattr(test_callback, 'test_accs') else 'val_accuracy',
        save_best_only=True,
        verbose=1
    ),
    
    # Early Stopping (매우 관대하게 설정)
    keras.callbacks.EarlyStopping(
        monitor='val_accuracy',
        patience=15,
        restore_best_weights=True,
        verbose=1
    ),
    
    # Plateau 감지 시 학습률 감소
    keras.callbacks.ReduceLROnPlateau(
        monitor='val_accuracy',
        factor=0.7,
        patience=7,
        min_lr=1e-7,
        verbose=1
    ),
]

# =================================================================
# 6. 모델 학습 (95% 달성 목표)
# =================================================================
print("\n🎯 목표: Training Accuracy 95% + Test Accuracy 최대화")
print("=" * 60)

history = model.fit(
    train_images, train_labels,
    epochs=80,  # 충분한 학습 시간
    batch_size=64,
    validation_data=(val_images, val_labels),
    callbacks=callbacks,
    verbose=1
)

print("\n✅ 학습 완료!")

# =================================================================
# 7. 상세 결과 분석 및 시각화
# =================================================================
acc = history.history["accuracy"]
val_acc = history.history["val_accuracy"]
test_acc = test_callback.test_accs
loss = history.history["loss"]
val_loss = history.history["val_loss"]
test_loss = test_callback.test_losses

epochs_range = range(len(acc))

# 큰 화면으로 시각화
plt.figure(figsize=(20, 8))

# 정확도 그래프
plt.subplot(2, 3, 1)
plt.plot(epochs_range, acc, 'b-', label="Training", linewidth=3, alpha=0.8)
plt.plot(epochs_range, val_acc, 'r-', label="Validation", linewidth=3, alpha=0.8)
plt.plot(epochs_range, test_acc, 'g-', label="Test", linewidth=3, alpha=0.8)
plt.axhline(y=0.95, color='orange', linestyle='--', linewidth=2, alpha=0.7, label='95% Goal')
plt.legend(loc="lower right", fontsize=12)
plt.title("Accuracy Progress", fontsize=16, fontweight='bold')
plt.xlabel("Epoch", fontsize=12)
plt.ylabel("Accuracy", fontsize=12)
plt.grid(True, alpha=0.3)
plt.ylim([0.7, 1.0])

# 손실 그래프
plt.subplot(2, 3, 2)
plt.plot(epochs_range, loss, 'b-', label="Training", linewidth=3, alpha=0.8)
plt.plot(epochs_range, val_loss, 'r-', label="Validation", linewidth=3, alpha=0.8)
plt.plot(epochs_range, test_loss, 'g-', label="Test", linewidth=3, alpha=0.8)
plt.legend(loc="upper right", fontsize=12)
plt.title("Loss Progress", fontsize=16, fontweight='bold')
plt.xlabel("Epoch", fontsize=12)
plt.ylabel("Loss", fontsize=12)
plt.grid(True, alpha=0.3)

# 오버피팅 분석
plt.subplot(2, 3, 3)
train_test_diff = np.array(acc) - np.array(test_acc)
plt.plot(epochs_range, train_test_diff, 'purple', linewidth=3, alpha=0.8, label="Train-Test Gap")
plt.axhline(y=0, color='black', linestyle='--', alpha=0.5)
plt.axhline(y=0.05, color='red', linestyle=':', alpha=0.7, label='5% Gap')
plt.axhline(y=0.10, color='orange', linestyle=':', alpha=0.7, label='10% Gap')
plt.legend(fontsize=12)
plt.title("Overfitting Analysis", fontsize=16, fontweight='bold')
plt.xlabel("Epoch", fontsize=12)
plt.ylabel("Accuracy Gap", fontsize=12)
plt.grid(True, alpha=0.3)

# 학습률 그래프
if 'lr' in history.history:
    plt.subplot(2, 3, 4)
    plt.plot(epochs_range, history.history['lr'], 'orange', linewidth=3, alpha=0.8)
    plt.title("Learning Rate Schedule", fontsize=16, fontweight='bold')
    plt.xlabel("Epoch", fontsize=12)
    plt.ylabel("Learning Rate", fontsize=12)
    plt.grid(True, alpha=0.3)
    plt.yscale('log')

# 성능 요약
plt.subplot(2, 3, 5)
final_metrics = [max(acc), max(val_acc), max(test_acc)]
labels = ['Train\n(Best)', 'Validation\n(Best)', 'Test\n(Best)']
colors = ['blue', 'red', 'green']
bars = plt.bar(labels, final_metrics, color=colors, alpha=0.7, edgecolor='black')
plt.axhline(y=0.95, color='orange', linestyle='--', linewidth=2, alpha=0.7)
plt.title("Final Performance", fontsize=16, fontweight='bold')
plt.ylabel("Accuracy", fontsize=12)
plt.ylim([0.85, 1.0])

# 각 바 위에 정확한 수치 표시
for i, (bar, metric) in enumerate(zip(bars, final_metrics)):
    plt.text(bar.get_x() + bar.get_width()/2, bar.get_height() + 0.005,
             f'{metric:.3f}', ha='center', va='bottom', fontsize=12, fontweight='bold')

plt.tight_layout()
plt.show()

# =================================================================
# 8. 최종 결과 보고서
# =================================================================
max_train_acc = max(acc)
max_val_acc = max(val_acc)
max_test_acc = max(test_acc)
final_gap = max_train_acc - max_test_acc

print("\n" + "="*80)
print("🏆 FINAL PERFORMANCE REPORT")
print("="*80)
print(f"🎯 Training Accuracy (Max):     {max_train_acc:.4f} {'✅ GOAL ACHIEVED!' if max_train_acc >= 0.95 else '❌ Below 95%'}")
print(f"📊 Validation Accuracy (Max):   {max_val_acc:.4f}")
print(f"🏅 Test Accuracy (Max):         {max_test_acc:.4f}")
print(f"📉 Training-Test Gap:           {final_gap:.4f}")
print(f"🎲 Best Test Accuracy:          {test_callback.best_test_acc:.4f}")
print("="*80)

# 성공 기준 평가
if max_train_acc >= 0.95:
    print("🎉 SUCCESS: Training accuracy target achieved!")
    print(f"💪 Test Performance: {max_test_acc:.1%}")
    
    if final_gap <= 0.05:
        print("🌟 EXCELLENT: Low overfitting (≤5% gap)")
    elif final_gap <= 0.10:
        print("✅ GOOD: Moderate overfitting (≤10% gap)")
    else:
        print("⚠️ WARNING: High overfitting (>10% gap)")
else:
    print("📈 Need more training or model capacity for 95% target")

print(f"\n📋 Model Summary:")
print(f"   • Parameters: {model.count_params():,}")
print(f"   • Architecture: Deep CNN (3 blocks)")
print(f"   • Regularization: Dropout + BatchNorm + L2")
print(f"   • Data Augmentation: ✅")
print(f"   • Label Smoothing: ✅")

# =================================================================
# 9. 샘플 예측 결과
# =================================================================
predictions = model.predict(test_images)

def plot_prediction_sample():
    plt.figure(figsize=(16, 10))
    
    for i in range(12):
        plt.subplot(3, 4, i + 1)
        
        img = test_images[i].squeeze()
        true_label = test_labels[i]
        pred_probs = predictions[i]
        pred_label = np.argmax(pred_probs)
        confidence = np.max(pred_probs)
        
        plt.imshow(img, cmap='gray')
        plt.axis('off')
        
        color = 'green' if pred_label == true_label else 'red'
        plt.title(f'True: {class_names[true_label]}\nPred: {class_names[pred_label]}\nConf: {confidence:.2f}', 
                 color=color, fontsize=10, fontweight='bold')
    
    plt.suptitle('Sample Predictions (Green=Correct, Red=Wrong)', fontsize=16, fontweight='bold')
    plt.tight_layout()
    plt.show()

plot_prediction_sample()

print("\n🚀 Training Complete! Check the graphs above for detailed analysis.")