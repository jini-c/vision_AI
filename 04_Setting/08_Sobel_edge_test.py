import cv2
import os
import time
import imutils
import numpy as np

# =================================================================
# 초기값 설정
# =================================================================
try:
    script_dir = os.path.dirname(os.path.abspath(__file__))
except NameError:
    script_dir = os.getcwd()

os.chdir(script_dir)
Videos_dir = os.path.abspath(os.path.join(script_dir, "../00_Sample_Video"))
video_file = os.path.join(Videos_dir, "input", "driver.mp4")

# =================================================================
# 카메라 / 파일 읽기
# =================================================================
cap = cv2.VideoCapture(0)  # 웹캠
# cap = cv2.VideoCapture(video_file)  # 파일

if not cap.isOpened():
    print(f"Error: 비디오를 열 수 없습니다. 경로: {video_file}")
    exit()

time.sleep(2.0)

while True:
    ret, frame = cap.read()
    if not ret:
        print("비디오가 끝났거나 에러가 발생했습니다.")
        break

    frame = imutils.resize(frame, width=720)

    # 입력영상 graysale 처리
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

    # (선택) 노이즈 감소
    blurred = cv2.GaussianBlur(gray, (5, 5), 0)

    # =================================================================
    # Sobel Edge Detection
    # =================================================================
    sobelX = cv2.Sobel(blurred, cv2.CV_64F, 1, 0, ksize=3)
    sobelY = cv2.Sobel(blurred, cv2.CV_64F, 0, 1, ksize=3)
    abs_sobelX = cv2.convertScaleAbs(sobelX)
    abs_sobelY = cv2.convertScaleAbs(sobelY)
    sobel_combined = cv2.addWeighted(abs_sobelX, 0.5, abs_sobelY, 0.5, 0)
    
    # --- 추가: Sobel "magnitude" 계산 → 0~255로 정규화 → 임계값 150 이진화 ---
    sobel_mag = cv2.magnitude(sobelX.astype(np.float32), sobelY.astype(np.float32))
    sobel_mag_u8 = cv2.normalize(sobel_mag, None, 0, 255, cv2.NORM_MINMAX).astype(np.uint8)
    _, sobel_binary = cv2.threshold(sobel_mag_u8, 150, 255, cv2.THRESH_BINARY)


    # =================================================================
    # Canny Edge Detection
    # =================================================================
    canny_edges = cv2.Canny(blurred, 50, 150)

    # =================================================================
    # Prewitt Edge Detection (개선 버전: float 연산 + magnitude)
    # =================================================================
    # 커널 (부호는 관례상 아래처럼 사용)
    kx = np.array([[-1, 0, 1],
                   [-1, 0, 1],
                   [-1, 0, 1]], dtype=np.float32)     # Gx
    ky = np.array([[ 1,  1,  1],
                   [ 0,  0,  0],
                   [-1, -1, -1]], dtype=np.float32)   # Gy

    # float으로 컨볼루션 → 음수/양수 보존
    px = cv2.filter2D(blurred, cv2.CV_32F, kx)
    py = cv2.filter2D(blurred, cv2.CV_32F, ky)

    # 크기(표준) 계산 후 8비트로 변환
    prewitt_mag = cv2.magnitude(px, py)           # sqrt(Gx^2 + Gy^2)
    operator_edges = cv2.convertScaleAbs(prewitt_mag)

    # =================================================================
    # 결과 영상 출력
    # =================================================================
    cv2.imshow("Original Frame", frame)
    cv2.imshow("Grayscale", gray)
    cv2.imshow("Sobel Edge Detection", sobel_combined)
    cv2.imshow("Canny Edge Detection", canny_edges)
    cv2.imshow("Prewitt Edge Detection", operator_edges)
    cv2.imshow("Sobel Magnitude (0~255)", sobel_mag_u8)     # 비교용(그레이스케일)
    cv2.imshow("Sobel Binary (T=150)", sobel_binary)        # 이진 영상


    key = cv2.waitKey(1) & 0xFF
    if key == ord("q"):
        break

cap.release()
cv2.destroyAllWindows()
