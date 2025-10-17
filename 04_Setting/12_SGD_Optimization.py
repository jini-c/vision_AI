import numpy as np
import matplotlib.pyplot as plt

# =================================================================
# 1. 데이터 준비
# =================================================================
# y = 2 * x 관계를 가지는 데이터 (정답 W = 2)
X = np.array([1, 2, 3, 4, 5])
y = np.array([2, 4, 6, 8, 10])


# =================================================================
# 2. 손실 함수 (Loss Function) 정의
# =================================================================
# 손실 함수는 이전과 동일함 (평균 제곱 오차)
def calculate_loss(y_pred, y_true):
    return np.mean((y_pred - y_true) ** 2)


# =================================================================
# 3. SGD 최적화 (Optimization) 과정
# =================================================================
# 임의의 W로 시작
W = 0.0

# 학습률 및 반복 횟수
learning_rate = 0.01
epochs = 20
n_data = len(X)  # 전체 데이터 개수

print("--------- SGD 최적화 시작 ---------")

# 전체 데이터를 여러 번 반복 학습 (epoch)
for i in range(epochs):

    # [SGD 핵심 1] 데이터 순서를 무작위로 섞음
    indices = np.arange(n_data)
    np.random.shuffle(indices)

    # [SGD 핵심 2] 데이터 '하나씩' 꺼내서 W를 업데이트함
    for idx in indices:
        # 데이터 포인트 1개 선택
        x_i = X[idx]
        y_i = y[idx]

        # 예측 (데이터 1개에 대해서만)
        y_pred_i = W * x_i

        # 그래디언트 계산 (데이터 1개에 대해서만)
        gradient = 2 * x_i * (y_pred_i - y_i)

        # W를 즉시 업데이트
        W = W - learning_rate * gradient

    # 한 epoch이 끝날 때마다 전체 데이터에 대한 손실을 계산해서 출력
    current_loss = calculate_loss(W * X, y)
    print(f"Epoch {i+1:2d}: W = {W:.4f}, Loss = {current_loss:.4f}")

print("--------- 최적화 종료 ---------")
print(f"\n찾아낸 최적의 W: {W:.4f}")

# =================================================================
# 4. 결과 시각화
# =================================================================
plt.figure(figsize=(8, 6))
plt.scatter(X, y, label="Original Data")
plt.plot(X, W * X, color="red", label=f"Predicted Line (W={W:.2f})")
plt.title("SGD Optimization Result")
plt.xlabel("X")
plt.ylabel("y")
plt.legend()
plt.grid(True)
plt.show()
