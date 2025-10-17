# -*- coding: utf-8 -*-
# =================================================================
# 라이브러리 import
# =================================================================
import cv2
import numpy as np
import os
import logging
import matplotlib
import matplotlib.pyplot as plt

# 정밀한 레이아웃을 위한 도구 import
from mpl_toolkits.axes_grid1 import make_axes_locatable

# =================================================================
# 로깅 설정
# =================================================================
logging.basicConfig(
    level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s"
)

# =================================================================
# 초기값 및 경로 설정
# =================================================================
logging.info("스크립트 실행 시작. 경로 설정 단계.")
try:
    script_dir = os.path.dirname(os.path.abspath(__file__))
except NameError:
    script_dir = os.getcwd()
logging.info(f"스크립트 디렉토리: {script_dir}")
Videos_dir = os.path.abspath(os.path.join(script_dir, "../00_Sample_Video"))
image_path = os.path.join(Videos_dir, "input", "edge.png")
logging.info(f"기본 이미지 경로 설정: {image_path}")
if not os.path.exists(image_path):
    logging.warning(f"지정된 경로에 파일이 없음. '{image_path}'")
    image_path = "edge.png"
    logging.info(f"대체 이미지 경로 설정: {image_path}")

# =================================================================
# 이미지 로드 및 히스토그램 계산
# =================================================================
logging.info("이미지 로드 시도...")
image = cv2.imread(image_path )
if image is None:
    logging.error(f"이미지 파일을 로드할 수 없음: {image_path}")
else:
    logging.info(f"이미지 로드 성공. 이미지 크기(세로, 가로): {image.shape}")
    h, w = image.shape[:2]

    logging.info("투영 히스토그램 계산 시작...")
    hist_horizontal = np.sum(image, axis=1)
    hist_vertical = np.sum(image, axis=0)
    logging.info("투영 히스토그램 계산 완료.")

    # =================================================================
    # Matplotlib로 시각화 객체 생성
    # =================================================================
    logging.info("Matplotlib 시각화 준비 시작...")
    fig, ax_image = plt.subplots(figsize=(8, 8))
    divider = make_axes_locatable(ax_image)
    ax_hist_v = divider.append_axes("top", size="20%", pad=0.1)
    ax_hist_h = divider.append_axes("right", size="20%", pad=0.1)

    # --- 1. 메인 이미지 그리기 ---
    ax_image.imshow(image, cmap="gray")
    ax_image.set_title("Original Image")
    ax_image.axis("off")  # 메인 이미지는 테두리 없이 유지

    # --- 2. 수직 히스토그램 그리기 ---
    ax_hist_v.plot(hist_vertical)
    ax_hist_v.set_title("Vertical Projection")
    # [핵심 수정] axis('off') 대신, 숫자 라벨만 제거해서 테두리만 남김
    ax_hist_v.set_xticks([])
    ax_hist_v.set_yticks([])

    # --- 3. 수평 히스토그램 그리기 ---
    ax_hist_h.plot(hist_horizontal, np.arange(h))
    ax_hist_h.set_title("Horizontal Projection")
    # [핵심 수정] axis('off') 대신, 숫자 라벨만 제거해서 테두리만 남김
    ax_hist_h.set_xticks([])
    ax_hist_h.set_yticks([])

    logging.info("시각화 준비 완료.")

    # =================================================================
    # Matplotlib Figure를 OpenCV 이미지(Numpy Array)로 변환
    # =================================================================
    logging.info("Matplotlib 그림을 OpenCV 이미지로 변환 중...")
    fig.canvas.draw()
    rgba_buf = fig.canvas.buffer_rgba()
    plot_img_np = np.asarray(rgba_buf)
    plot_img_bgr = cv2.cvtColor(plot_img_np, cv2.COLOR_RGBA2BGR)
    plt.close(fig)
    logging.info("이미지 변환 완료.")

    # =================================================================
    # OpenCV로 시각화 창을 띄우고 사용자가 닫을 때까지 유지
    # =================================================================
    logging.info(
        "OpenCV 창을 화면에 표시합니다. 창을 닫거나 'q'를 누를 때까지 대기합니다..."
    )
    cv2.imshow("Histogram Visualization", plot_img_bgr)
    while True:
        key = cv2.waitKey(1) & 0xFF
        if (
            key == ord("q")
            or cv2.getWindowProperty("Histogram Visualization", cv2.WND_PROP_VISIBLE)
            < 1
        ):
            break
    cv2.destroyAllWindows()
    logging.info("OpenCV 창이 닫혔습니다.")

logging.info("스크립트 실행 종료.")
