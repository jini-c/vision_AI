# =================================================================
# 라이브러리 import
# =================================================================
import tensorflow as tf
from tensorflow import keras
import numpy as np
import matplotlib.pyplot as plt
from sklearn.model_selection import train_test_split

# =================================================================
# 1. 데이터 로드 및 3단계 분리 (Train, Validation, Test)
# =================================================================
# Fashion MNIST 데이터셋 로드
fashion_mnist = keras.datasets.fashion_mnist
(train_val_images, train_val_labels), (test_images_orig, test_labels_orig) = (
    fashion_mnist.load_data()
)

# 클래스 이름 정의
class_names = [
    "T-shirt/top",
    "Trouser",
    "Pullover",
    "Dress",
    "Coat",
    "Sandal",
    "Shirt",
    "Sneaker",
    "Bag",
    "Ankle boot",
]

# 로드된 전체 데이터를 합쳐서 다시 분배함 (총 70,000개)
all_images = np.concatenate([train_val_images, test_images_orig])
all_labels = np.concatenate([train_val_labels, test_labels_orig])

# 훈련셋(60%)과 테스트셋(40%)으로 먼저 나눔
train_val_images, test_images, train_val_labels, test_labels = train_test_split(
    all_images, all_labels, test_size=0.4, random_state=42
)

# 남은 훈련셋(60%)을 다시 훈련셋(50%)과 검증셋(10%)으로 나눔
train_images, val_images, train_labels, val_labels = train_test_split(
    train_val_images, train_val_labels, test_size=0.16666, random_state=42
)  # 0.16666 * 0.6 = 0.1

# =================================================================
# 2. 데이터 전처리
# =================================================================
# 픽셀 값을 0~1 사이로 정규화 및 채널 차원 추가
train_images = np.expand_dims(train_images / 255.0, -1)
val_images = np.expand_dims(val_images / 255.0, -1)
test_images = np.expand_dims(test_images / 255.0, -1)

# =================================================================
# 3. 테스트셋 평가를 위한 커스텀 콜백 정의
# =================================================================
class TestMetricsCallback(keras.callbacks.Callback):
    def __init__(self, test_data):
        self.test_data = test_data
        self.test_accs = []
        self.test_losses = []

    def on_epoch_end(self, epoch, logs=None):
        x_test, y_test = self.test_data
        loss, acc = self.model.evaluate(x_test, y_test, verbose=0)
        self.test_losses.append(loss)
        self.test_accs.append(acc)
        # 훈련 로그에 테스트 결과를 추가하여 출력
        print(f" — test_loss: {loss:.4f} — test_acc: {acc:.4f}")

# 콜백 인스턴스 생성
test_callback = TestMetricsCallback((test_images, test_labels))

# =================================================================
# 4. 오버피팅 해결을 위한 개선된 CNN 모델 구축 및 컴파일
# =================================================================
model = keras.Sequential([
    # 첫 번째 컨볼루션 블록
    keras.layers.Conv2D(32, (3, 3), activation="relu", input_shape=(28, 28, 1)),
    keras.layers.MaxPooling2D((2, 2)),
    
    # 두 번째 컨볼루션 블록
    keras.layers.Conv2D(64, (3, 3), activation="relu"),
    keras.layers.MaxPooling2D((2, 2)),
    
    # 완전연결층
    keras.layers.Flatten(),
    keras.layers.Dropout(0.5),  # 첫 번째 Dropout (높은 비율)
    keras.layers.Dense(64, activation="relu"),  # 128 → 64로 축소 (모델 복잡도 감소)
    keras.layers.Dropout(0.3),  # 두 번째 Dropout (낮은 비율)
    keras.layers.Dense(10, activation="softmax"),
])

# 학습률을 낮춰서 안정적인 학습
model.compile(
    optimizer=keras.optimizers.Adam(learning_rate=0.0005),  # 기본값 0.001에서 0.0005로 감소
    loss="sparse_categorical_crossentropy", 
    metrics=["accuracy"]
)

# 모델 구조 출력
print("모델 구조:")
model.summary()

# =================================================================
# 5. 오버피팅 방지를 위한 콜백 설정
# =================================================================
# Early Stopping: validation loss가 개선되지 않으면 조기 종료
early_stop = keras.callbacks.EarlyStopping(
    monitor='val_loss',
    patience=5,  # 5 epoch 동안 개선이 없으면 중단
    restore_best_weights=True,  # 최고 성능 가중치로 복원
    verbose=1
)

# Learning Rate 감소: validation loss가 plateau에 도달하면 학습률 감소
reduce_lr = keras.callbacks.ReduceLROnPlateau(
    monitor='val_loss',
    factor=0.5,  # 학습률을 절반으로 감소
    patience=3,  # 3 epoch 동안 개선이 없으면 감소
    min_lr=1e-7,
    verbose=1
)

# =================================================================
# 6. 모델 학습 (개선된 콜백 사용)
# =================================================================
print("--------- 개선된 모델 학습 시작 ---------")
history = model.fit(
    train_images,
    train_labels,
    epochs=30,  # epoch 수를 늘림 (early stopping이 알아서 중단)
    batch_size=32,  # 배치 크기 명시적 설정
    validation_data=(val_images, val_labels),
    callbacks=[test_callback, early_stop, reduce_lr],  # 오버피팅 방지 콜백들 추가
    verbose=1
)
print("--------- 모델 학습 종료 ---------")

# =================================================================
# 7. 3개 데이터셋의 정확도 및 손실 그래프 시각화
# =================================================================
acc = history.history["accuracy"]
val_acc = history.history["val_accuracy"]
test_acc = test_callback.test_accs
loss = history.history["loss"]
val_loss = history.history["val_loss"]
test_loss = test_callback.test_losses

epochs_range = range(len(acc))

plt.figure(figsize=(15, 5))

# 정확도 그래프
plt.subplot(1, 3, 1)
plt.plot(epochs_range, acc, 'bo-', label="Training Accuracy", linewidth=2)
plt.plot(epochs_range, val_acc, 'ro-', label="Validation Accuracy", linewidth=2)
plt.plot(epochs_range, test_acc, 'go-', label="Test Accuracy", linewidth=2)
plt.legend(loc="lower right")
plt.title("Training, Validation, and Test Accuracy")
plt.xlabel("Epoch")
plt.ylabel("Accuracy")
plt.grid(True, alpha=0.3)

# 손실 그래프
plt.subplot(1, 3, 2)
plt.plot(epochs_range, loss, 'bo-', label="Training Loss", linewidth=2)
plt.plot(epochs_range, val_loss, 'ro-', label="Validation Loss", linewidth=2)
plt.plot(epochs_range, test_loss, 'go-', label="Test Loss", linewidth=2)
plt.legend(loc="upper right")
plt.title("Training, Validation, and Test Loss")
plt.xlabel("Epoch")
plt.ylabel("Loss")
plt.grid(True, alpha=0.3)

# 오버피팅 정도 분석
plt.subplot(1, 3, 3)
train_val_diff = np.array(acc) - np.array(val_acc)
train_test_diff = np.array(acc) - np.array(test_acc)
plt.plot(epochs_range, train_val_diff, 'mo-', label="Train-Val Accuracy Gap", linewidth=2)
plt.plot(epochs_range, train_test_diff, 'co-', label="Train-Test Accuracy Gap", linewidth=2)
plt.axhline(y=0, color='black', linestyle='--', alpha=0.5)
plt.legend(loc="upper right")
plt.title("Overfitting Analysis")
plt.xlabel("Epoch")
plt.ylabel("Accuracy Difference")
plt.grid(True, alpha=0.3)

plt.tight_layout()
plt.show()

# =================================================================
# 8. 최종 성능 평가 및 개선사항 출력
# =================================================================
final_train_acc = acc[-1]
final_val_acc = val_acc[-1]
final_test_acc = test_acc[-1]

print("\n" + "="*50)
print("최종 성능 결과:")
print("="*50)
print(f"최종 Training Accuracy:   {final_train_acc:.4f}")
print(f"최종 Validation Accuracy: {final_val_acc:.4f}")
print(f"최종 Test Accuracy:       {final_test_acc:.4f}")
print(f"Train-Val 차이:           {final_train_acc - final_val_acc:.4f}")
print(f"Train-Test 차이:          {final_train_acc - final_test_acc:.4f}")
print("="*50)

print("\n적용된 오버피팅 해결 방법:")
print("1. Dropout 추가 (0.5, 0.3)")
print("2. Dense layer 크기 축소 (128 → 64)")
print("3. 학습률 감소 (0.001 → 0.0005)")
print("4. Early Stopping (patience=5)")
print("5. Learning Rate Scheduling")

# =================================================================
# 9. 예측 결과 시각화
# =================================================================
# 테스트셋에 대한 예측 수행
predictions = model.predict(test_images)

# 이미지와 예측 결과를 함께 보여주는 함수
def plot_image(i, predictions_array, true_label, img):
    true_label, img = true_label[i], img[i]
    plt.grid(False)
    plt.xticks([])
    plt.yticks([])
    plt.imshow(img.squeeze(), cmap=plt.cm.binary)
    predicted_label = np.argmax(predictions_array)
    color = "blue" if predicted_label == true_label else "red"
    plt.xlabel(
        f"Predicted: {class_names[predicted_label]} ({100*np.max(predictions_array):.2f}%)\nTrue: {class_names[true_label]}",
        color=color,
    )

# 예측 확률을 막대 그래프로 보여주는 함수
def plot_value_array(i, predictions_array, true_label):
    true_label = true_label[i]
    plt.grid(False)
    plt.xticks(range(10), class_names, rotation=45)
    plt.yticks([])
    thisplot = plt.bar(range(10), predictions_array, color="#777777")
    plt.ylim([0, 1])
    predicted_label = np.argmax(predictions_array)
    thisplot[predicted_label].set_color("red")
    thisplot[true_label].set_color("blue")

# 처음 15개의 테스트 이미지와 예측 결과를 출력
num_rows = 5
num_cols = 3
num_images = num_rows * num_cols
plt.figure(figsize=(2 * 2 * num_cols, 2 * num_rows))
for i in range(num_images):
    plt.subplot(num_rows, 2 * num_cols, 2 * i + 1)
    plot_image(i, predictions[i], test_labels, test_images)
    plt.subplot(num_rows, 2 * num_cols, 2 * i + 2)
    plot_value_array(i, predictions[i], test_labels)
plt.tight_layout()
plt.show()