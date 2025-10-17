# =================================================================
# 1. 라이브러리 import
# =================================================================
import tensorflow as tf
from tensorflow import keras
from tensorflow.keras import layers, regularizers
import numpy as np
import matplotlib.pyplot as plt
from sklearn.model_selection import train_test_split

# =================================================================
# 2. 데이터 로드 및 전처리
# =================================================================
# Fashion MNIST 데이터셋 로드
fashion_mnist = keras.datasets.fashion_mnist
(train_images, train_labels), (test_images, test_labels) = fashion_mnist.load_data()

# 클래스 이름 정의 (오류 수정)
class_names = [
    "T-shirt/top", "Trouser", "Pullover", "Dress", "Coat",
    "Sandal", "Shirt", "Sneaker", "Bag", "Ankle boot"
]

# 픽셀 값을 0~1 사이로 정규화 및 채널 차원 추가 (CNN 입력을 위해)
train_images = np.expand_dims(train_images / 255.0, -1)
test_images = np.expand_dims(test_images / 255.0, -1)

# 훈련 데이터에서 20%를 검증 데이터로 분리
train_images, val_images, train_labels, val_labels = train_test_split(
    train_images, train_labels, test_size=0.2, random_state=42
)

print(f"훈련 데이터: {train_images.shape}, {train_labels.shape}")
print(f"검증 데이터: {val_images.shape}, {val_labels.shape}")
print(f"테스트 데이터: {test_images.shape}, {test_labels.shape}")


# =================================================================
# 3. 데이터 증강 레이어 정의
# =================================================================
# 훈련 시에만 적용될 데이터 증강 레이어를 미리 정의
data_augmentation = keras.Sequential([
    layers.RandomFlip("horizontal"),
    layers.RandomRotation(0.1),
    layers.RandomZoom(0.1),
], name="data_augmentation")


# =================================================================
# 4. 과적합 방지 기법이 적용된 CNN 모델 구축
# =================================================================
def build_regularized_model():
    """과적합 방지 기법이 적용된 모델을 생성하는 함수"""
    model = keras.Sequential([
        # 입력 레이어
        layers.Input(shape=(28, 28, 1)),
        
        # 1. 데이터 증강 레이어 추가
        data_augmentation,
        
        # 첫 번째 Conv 블록
        layers.Conv2D(32, (3, 3), activation="relu",
                      kernel_regularizer=regularizers.l2(0.001)), # 2. L2 규제
        layers.MaxPooling2D((2, 2)),
        
        # 두 번째 Conv 블록
        layers.Conv2D(64, (3, 3), activation="relu",
                      kernel_regularizer=regularizers.l2(0.001)), # 2. L2 규제
        layers.MaxPooling2D((2, 2)),
        
        # Flatten 및 Dense 레이어
        layers.Flatten(),
        layers.Dense(128, activation="relu",
                     kernel_regularizer=regularizers.l2(0.001)), # 2. L2 규제
        layers.Dropout(0.5),  # 3. 드롭아웃 (50%)
        
        # 출력 레이어
        layers.Dense(10, activation="softmax"),
    ])
    
    # 모델 컴파일
    model.compile(
        optimizer="adam",
        loss="sparse_categorical_crossentropy",
        metrics=["accuracy"]
    )
    return model

# 모델 생성 및 구조 확인
model = build_regularized_model()
model.summary()


# =================================================================
# 5. 조기 종료 콜백 설정 및 모델 학습
# =================================================================
# 4. 조기 종료 (Early Stopping) 콜백 정의
# val_loss를 기준으로 10번 동안 개선이 없으면 훈련을 중단하고, 가장 좋았던 가중치를 복원
early_stopping = keras.callbacks.EarlyStopping(
    monitor='val_loss',
    patience=10,
    restore_best_weights=True
)

print("\n--------- 모델 학습 시작 ---------")
history = model.fit(
    train_images,
    train_labels,
    epochs=100,  # 충분히 많은 에포크를 설정 (조기 종료가 최적 시점에 중단시킴)
    validation_data=(val_images, val_labels),
    callbacks=[early_stopping], # 콜백 리스트에 조기 종료 추가
    verbose=1 # 학습 과정 출력
)
print("--------- 모델 학습 종료 ---------")


# =================================================================
# 6. 모델 평가 및 결과 시각화
# =================================================================
# 테스트 데이터로 최종 성능 평가
test_loss, test_acc = model.evaluate(test_images, test_labels, verbose=2)
print(f"\n최종 테스트 정확도: {test_acc:.4f}")

# 훈련/검증 정확도 및 손실 그래프
acc = history.history["accuracy"]
val_acc = history.history["val_accuracy"]
loss = history.history["loss"]
val_loss = history.history["val_loss"]

epochs_range = range(len(acc))

plt.figure(figsize=(14, 5))

# 정확도 그래프
plt.subplot(1, 2, 1)
plt.plot(epochs_range, acc, label="Training Accuracy")
plt.plot(epochs_range, val_acc, label="Validation Accuracy")
plt.legend(loc="lower right")
plt.title("Training and Validation Accuracy")
plt.xlabel("Epoch")
plt.ylabel("Accuracy")

# 손실 그래프
plt.subplot(1, 2, 2)
plt.plot(epochs_range, loss, label="Training Loss")
plt.plot(epochs_range, val_loss, label="Validation Loss")
plt.legend(loc="upper right")
plt.title("Training and Validation Loss")
plt.xlabel("Epoch")
plt.ylabel("Loss")
plt.tight_layout()
plt.show()


# =================================================================
# 7. 예측 결과 시각화 (기존 코드 활용)
# =================================================================
def plot_image(i, predictions_array, true_label, img):
    true_label, img = true_label[i], img[i]
    plt.grid(False)
    plt.xticks([])
    plt.yticks([])
    plt.imshow(img.squeeze(), cmap=plt.cm.binary)
    predicted_label = np.argmax(predictions_array)
    color = "blue" if predicted_label == true_label else "red"
    plt.xlabel(f"Predicted: {class_names[predicted_label]} ({100*np.max(predictions_array):.2f}%)\n"
               f"True: {class_names[true_label]}", color=color)

def plot_value_array(i, predictions_array, true_label):
    true_label = true_label[i]
    plt.grid(False)
    plt.xticks(range(10), class_names, rotation=90)
    plt.yticks([])
    thisplot = plt.bar(range(10), predictions_array, color="#777777")
    plt.ylim([0, 1])
    predicted_label = np.argmax(predictions_array)
    thisplot[predicted_label].set_color("red")
    thisplot[true_label].set_color("blue")

# 테스트셋에 대한 예측 수행
predictions = model.predict(test_images)

# 처음 15개의 테스트 이미지와 예측 결과를 출력
num_rows = 5
num_cols = 3
plt.figure(figsize=(2 * 2 * num_cols, 2 * num_rows))
for i in range(num_rows * num_cols):
    plt.subplot(num_rows, 2, 2 * i + 1)
    plot_image(i, predictions[i], test_labels, test_images)
    plt.subplot(num_rows, 2, 2 * i + 2)
    plot_value_array(i, predictions[i], test_labels)
plt.tight_layout()
plt.show()