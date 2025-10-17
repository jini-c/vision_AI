# ================================================================
# 0) Imports & seed
# ================================================================
import numpy as np, random, matplotlib.pyplot as plt, tensorflow as tf
from tensorflow import keras
from sklearn.model_selection import train_test_split

SEED = 42
random.seed(SEED); np.random.seed(SEED); tf.random.set_seed(SEED)

# ================================================================
# 1) Load & split (train/val/test ≈ 60/10/30)
# ================================================================
(f_train, y_train), (f_test, y_test) = keras.datasets.fashion_mnist.load_data()
X_all  = np.concatenate([f_train, f_test], axis=0)
y_all  = np.concatenate([y_train, y_test], axis=0)

X_trv, X_te, y_trv, y_te = train_test_split(
    X_all, y_all, test_size=0.30, random_state=SEED, stratify=y_all
)
X_tr, X_va, y_tr, y_va = train_test_split(
    X_trv, y_trv, test_size=0.142857, random_state=SEED, stratify=y_trv
)

def norm_expand(x):
    x = x.astype("float32")/255.0
    return np.expand_dims(x, -1)

X_tr, X_va, X_te = map(norm_expand, [X_tr, X_va, X_te])

# ================================================================
# 2) Datasets (증강은 2단계에서만 사용)
# ================================================================
batch_size = 128

# 클린(증강 없는) 훈련셋: 1단계/평가용
train_clean_ds = (tf.data.Dataset.from_tensor_slices((X_tr, y_tr))
                  .shuffle(10000, seed=SEED).batch(batch_size)
                  .prefetch(tf.data.AUTOTUNE))

val_ds  = (tf.data.Dataset.from_tensor_slices((X_va, y_va))
           .batch(batch_size).prefetch(tf.data.AUTOTUNE))
test_ds = (tf.data.Dataset.from_tensor_slices((X_te, y_te))
           .batch(batch_size).prefetch(tf.data.AUTOTUNE))

# 2단계에서 사용할 증강 파이프라인
augment = keras.Sequential([
    keras.layers.RandomFlip("horizontal"),
    keras.layers.RandomRotation(0.1),
    keras.layers.RandomZoom(0.1),
    keras.layers.RandomTranslation(0.05, 0.05),
], name="augment")

train_aug_ds = (tf.data.Dataset.from_tensor_slices((X_tr, y_tr))
                .shuffle(10000, seed=SEED).batch(batch_size)
                .map(lambda x,y: (augment(x, training=True), y),
                     num_parallel_calls=tf.data.AUTOTUNE)
                .prefetch(tf.data.AUTOTUNE))

# ================================================================
# 3) Model: Mini-ResNet (Conv-BN-ReLU x2 + shortcut)
# ================================================================
l2 = keras.regularizers.l2(5e-5)

def conv_bn_relu(x, filters, stride=1):
    x = keras.layers.Conv2D(filters, 3, strides=stride, padding="same",
                            use_bias=False, kernel_regularizer=l2)(x)
    x = keras.layers.BatchNormalization()(x)
    return keras.layers.Activation("relu")(x)

def basic_block(x, filters, stride=1):
    shortcut = x
    x = conv_bn_relu(x, filters, stride)
    x = keras.layers.Conv2D(filters, 3, padding="same",
                            use_bias=False, kernel_regularizer=l2)(x)
    x = keras.layers.BatchNormalization()(x)
    if shortcut.shape[-1] != filters or stride != 1:
        shortcut = keras.layers.Conv2D(filters, 1, strides=stride, padding="same",
                                       use_bias=False, kernel_regularizer=l2)(shortcut)
        shortcut = keras.layers.BatchNormalization()(shortcut)
    x = keras.layers.Add()([x, shortcut])
    x = keras.layers.Activation("relu")(x)
    return x

def build_model():
    inputs = keras.Input(shape=(28,28,1))
    x = conv_bn_relu(inputs, 64)                 # stem
    x = basic_block(x, 64); x = basic_block(x, 64)
    x = basic_block(x, 128, stride=2); x = basic_block(x, 128)
    x = basic_block(x, 256, stride=2); x = basic_block(x, 256)
    x = keras.layers.GlobalAveragePooling2D()(x)
    x = keras.layers.Dense(256, activation="relu")(x)  # 용량 충분히
    outputs = keras.layers.Dense(10, activation="softmax")(x)
    return keras.Model(inputs, outputs)

model = build_model()
model.summary()

# ================================================================
# 4) Callbacks (클린 Train/Test 메트릭 기록 + 조기종료/스케줄)
# ================================================================
class EvalDataset(keras.callbacks.Callback):
    def __init__(self, ds, name):
        self.ds, self.name = ds, name
        self.accs, self.losses = [], []
    def on_epoch_end(self, epoch, logs=None):
        loss, acc = self.model.evaluate(self.ds, verbose=0)
        self.losses.append(loss); self.accs.append(acc)
        print(f" — {self.name}_loss: {loss:.4f} — {self.name}_acc: {acc:.4f}")

train_clean_cb = EvalDataset(train_clean_ds, "train(clean)")
test_cb        = EvalDataset(test_ds, "test")

# ================================================================
# 5) Stage 1 — Overfit(=High Train Acc) on clean data
#     * 증강/드롭아웃 최소화, Adam, 적당한 epochs
# ================================================================
model.compile(optimizer=keras.optimizers.Adam(1e-3),
              loss="sparse_categorical_crossentropy",
              metrics=["accuracy"])

early1 = keras.callbacks.EarlyStopping(monitor="accuracy",
                                       patience=3, mode="max",
                                       min_delta=1e-3, restore_best_weights=True)

print("\n==== Stage 1: Fit (aim high train acc) ====")
hist1 = model.fit(
    train_clean_ds,
    epochs=20,
    callbacks=[train_clean_cb, test_cb, early1],
    verbose=1
)

# ================================================================
# 6) Stage 2 — Generalize with augmentation + AdamW + Cosine decay
# ================================================================
steps_per_epoch = int(np.ceil(len(X_tr)/batch_size))
cosine = keras.optimizers.schedules.CosineDecayRestarts(initial_learning_rate=5e-4,
                                                        first_decay_steps=steps_per_epoch*5)
model.compile(
    optimizer=keras.optimizers.AdamW(learning_rate=cosine, weight_decay=1e-4),
    loss="sparse_categorical_crossentropy",
    metrics=["accuracy"]
)
early2  = keras.callbacks.EarlyStopping(monitor="val_loss", patience=5, restore_best_weights=True)

print("\n==== Stage 2: Generalize (aug + weight decay) ====")
hist2 = model.fit(
    train_aug_ds,
    epochs=30,
    validation_data=val_ds,
    callbacks=[train_clean_cb, test_cb, early2],
    verbose=1
)

# ================================================================
# 7) Curves: Train(clean) / Val / Test
# ================================================================
acc_tr_clean = train_clean_cb.accs
loss_tr_clean= train_clean_cb.losses

acc_val = hist2.history.get("val_accuracy", [])
loss_val= hist2.history.get("val_loss", [])

# 훈련 단계 전체 epoch 축 계산
epochs_total = range(len(acc_tr_clean))

plt.figure(figsize=(12,5))
plt.subplot(1,2,1)
plt.plot(epochs_total, acc_tr_clean, label="Train Accuracy (clean)")  # ★ 핵심
plt.plot(range(len(acc_tr_clean)-len(acc_val), len(acc_tr_clean)), acc_val, label="Validation Accuracy")
plt.plot(test_cb.accs, label="Test Accuracy")
plt.legend(); plt.title("Training(clean), Validation, Test Accuracy"); plt.xlabel("Epoch"); plt.ylabel("Accuracy")

plt.subplot(1,2,2)
plt.plot(epochs_total, loss_tr_clean, label="Train Loss (clean)")
plt.plot(range(len(acc_tr_clean)-len(loss_val), len(acc_tr_clean)), loss_val, label="Validation Loss")
plt.plot(test_cb.losses, label="Test Loss")
plt.legend(); plt.title("Training(clean), Validation, Test Loss"); plt.xlabel("Epoch"); plt.ylabel("Loss")
plt.tight_layout(); plt.show()

# ================================================================
# 8) Final test eval
# ================================================================
final_loss, final_acc = model.evaluate(test_ds, verbose=0)
print(f"\n[FINAL] Test acc: {final_acc:.4f}, loss: {final_loss:.4f}")
