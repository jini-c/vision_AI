# =================================================================
# 0) 라이브러리 & 설정
# =================================================================
import os, random
import numpy as np
import matplotlib.pyplot as plt
import tensorflow as tf
from tensorflow import keras
from sklearn.model_selection import train_test_split

# 재현성(참고: 완전 고정은 아님)
SEED = 42
random.seed(SEED); np.random.seed(SEED); tf.random.set_seed(SEED)

# =================================================================
# 1) 데이터 로드 & 3단계 분리 (Train / Val / Test)
# =================================================================
fashion_mnist = keras.datasets.fashion_mnist
(train_val_images, train_val_labels), (test_images_orig, test_labels_orig) = fashion_mnist.load_data()

class_names = ["T-shirt/top","Trouser","Pullover","Dress","Coat",
               "Sandal","Shirt","Sneaker","Bag","Ankle boot"]

# 전체 7만장을 다시 섞어 60/10/30 (train/val/test) 비율로 분리
all_images = np.concatenate([train_val_images, test_images_orig], axis=0)
all_labels = np.concatenate([train_val_labels, test_labels_orig], axis=0)

train_val_images, test_images, train_val_labels, test_labels = train_test_split(
    all_images, all_labels, test_size=0.30, random_state=SEED, stratify=all_labels
)
train_images, val_images, train_labels, val_labels = train_test_split(
    train_val_images, train_val_labels, test_size=0.142857,  # ≈ 10% of total
    random_state=SEED, stratify=train_val_labels
)

# =================================================================
# 2) 전처리 (정규화 + 채널축 추가)
# =================================================================
def norm_expand(x):
    x = x.astype("float32") / 255.0
    return np.expand_dims(x, -1)

train_images = norm_expand(train_images)
val_images   = norm_expand(val_images)
test_images  = norm_expand(test_images)

# =================================================================
# 3) Test 측정 콜백 (에폭마다 테스트 성능 찍기)
# =================================================================
class TestMetricsCallback(keras.callbacks.Callback):
    def __init__(self, test_data):
        self.test_data = test_data
        self.test_accs, self.test_losses = [], []

    def on_epoch_end(self, epoch, logs=None):
        x_test, y_test = self.test_data
        loss, acc = self.model.evaluate(x_test, y_test, verbose=0)
        self.test_losses.append(loss)
        self.test_accs.append(acc)
        print(f" — test_loss: {loss:.4f} — test_acc: {acc:.4f}")

test_callback = TestMetricsCallback((test_images, test_labels))

# =================================================================
# 4) tf.data 파이프라인 + Augmentation
# =================================================================
data_augment = keras.Sequential([
    keras.layers.RandomFlip("horizontal"),
    keras.layers.RandomRotation(0.1),
    keras.layers.RandomZoom(0.1),
    keras.layers.RandomTranslation(0.05, 0.05),
], name="augment")

batch_size = 128
train_ds = (tf.data.Dataset.from_tensor_slices((train_images, train_labels))
            .shuffle(10000, seed=SEED)
            .batch(batch_size)
            .map(lambda x,y: (data_augment(x, training=True), y),
                 num_parallel_calls=tf.data.AUTOTUNE)
            .prefetch(tf.data.AUTOTUNE))

val_ds  = (tf.data.Dataset.from_tensor_slices((val_images, val_labels))
           .batch(batch_size)
           .prefetch(tf.data.AUTOTUNE))

test_ds = (tf.data.Dataset.from_tensor_slices((test_images, test_labels))
           .batch(batch_size)
           .prefetch(tf.data.AUTOTUNE))

# =================================================================
# 5) 모델: Conv-BN-ReLU + Dropout + L2 + GAP
# =================================================================
l2 = keras.regularizers.l2(1e-4)

def conv_block(filters):
    return keras.Sequential([
        keras.layers.Conv2D(filters, 3, padding="same", use_bias=False,
                            kernel_regularizer=l2),
        keras.layers.BatchNormalization(),
        keras.layers.Activation("relu"),
    ])

inputs = keras.Input(shape=(28, 28, 1))
x = conv_block(32)(inputs)
x = conv_block(32)(x)
x = keras.layers.MaxPooling2D()(x)
x = keras.layers.Dropout(0.25)(x)

x = conv_block(64)(x)
x = conv_block(64)(x)
x = keras.layers.MaxPooling2D()(x)
x = keras.layers.Dropout(0.25)(x)

# Flatten 대신 파라미터가 적은 GlobalAveragePooling
x = keras.layers.GlobalAveragePooling2D()(x)
x = keras.layers.Dense(128, activation="relu", kernel_regularizer=l2)(x)
x = keras.layers.Dropout(0.5)(x)
outputs = keras.layers.Dense(10, activation="softmax")(x)

model = keras.Model(inputs, outputs)
model.summary()

# =================================================================
# 6) 컴파일 (AdamW + 스케줄러/조기종료)
# =================================================================
model.compile(
    optimizer=keras.optimizers.AdamW(learning_rate=1e-3, weight_decay=1e-4),
    loss="sparse_categorical_crossentropy",
    metrics=["accuracy"],
)

early = keras.callbacks.EarlyStopping(
    monitor="val_loss", patience=5, restore_best_weights=True)
reduce = keras.callbacks.ReduceLROnPlateau(
    monitor="val_loss", factor=0.5, patience=2, min_lr=1e-5)

# =================================================================
# 7) 학습
# =================================================================
print("--------- 모델 학습 시작 ---------")
history = model.fit(
    train_ds,
    epochs=50,
    validation_data=val_ds,
    callbacks=[test_callback, early, reduce],
)
print("--------- 모델 학습 종료 ---------")

# =================================================================
# 8) 학습 곡선 시각화 (Train / Val / Test)
# =================================================================
acc      = history.history["accuracy"]
val_acc  = history.history["val_accuracy"]
loss     = history.history["loss"]
val_loss = history.history["val_loss"]
test_acc = test_callback.test_accs
test_loss= test_callback.test_losses

epochs_range = range(len(acc))
plt.figure(figsize=(12,5))
plt.subplot(1,2,1)
plt.plot(epochs_range, acc, label="Training Accuracy")
plt.plot(epochs_range, val_acc, label="Validation Accuracy")
plt.plot(epochs_range, test_acc, label="Test Accuracy")
plt.legend(); plt.title("Training, Validation, and Test Accuracy")
plt.xlabel("Epoch"); plt.ylabel("Accuracy")

plt.subplot(1,2,2)
plt.plot(epochs_range, loss, label="Training Loss")
plt.plot(epochs_range, val_loss, label="Validation Loss")
plt.plot(epochs_range, test_loss, label="Test Loss")
plt.legend(); plt.title("Training, Validation, and Test Loss")
plt.xlabel("Epoch"); plt.ylabel("Loss")
plt.tight_layout(); plt.show()

# =================================================================
# 9) 예측 시각화
# =================================================================
pred_probs = model.predict(test_ds)
pred_labels = np.argmax(pred_probs, axis=1)

def plot_image(i, predictions_array, true_label, img):
    plt.grid(False); plt.xticks([]); plt.yticks([])
    plt.imshow(img.squeeze(), cmap=plt.cm.binary)
    pred = np.argmax(predictions_array)
    color = "blue" if pred == true_label else "red"
    plt.xlabel(f"Pred: {class_names[pred]} ({100*np.max(predictions_array):.1f}%)\n"
               f"True: {class_names[true_label]}", color=color)

def plot_value_array(i, predictions_array, true_label):
    plt.grid(False); plt.xticks(range(10), class_names, rotation=90); plt.yticks([])
    bars = plt.bar(range(10), predictions_array, color="#777777"); plt.ylim([0,1])
    pred = np.argmax(predictions_array)
    bars[pred].set_color("red"); bars[true_label].set_color("blue")

# test_ds를 다시 배열로 모아 시각화(간단히)
test_imgs_np  = test_images
test_labels_np= test_labels
num_rows, num_cols = 5, 3
num_images = num_rows * num_cols
plt.figure(figsize=(2*2*num_cols, 2*num_rows))
for i in range(num_images):
    plt.subplot(num_rows, 2, 2*i+1)
    plot_image(i, pred_probs[i], test_labels_np, test_imgs_np)
    plt.subplot(num_rows, 2, 2*i+2)
    plot_value_array(i, pred_probs[i], test_labels_np)
plt.tight_layout(); plt.show()
