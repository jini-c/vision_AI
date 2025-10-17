import cv2
import torch
import time
import numpy as np
from ultralytics import YOLO
import os
from tqdm import tqdm

# =============================
# 1. 설정 및 모델 로드
# =============================

#=================================================================
#초기값 설정
#=================================================================
#실행 경로 설정 
# 경로 설정
script_dir = os.path.dirname(os.path.abspath(__file__))
os.chdir(script_dir)
Models_dir = os.path.abspath(os.path.join(script_dir, "../00_Models"))
Videos_dir = os.path.abspath(os.path.join(script_dir, "../00_Sample_Video"))
video_file = os.path.join(Videos_dir, "input","road.mp4" )
yolo_model_file = os.path.join(Models_dir, "yolov8n.pt" )
road_seg_model_file  = os.path.join(Models_dir, "road-segmentation-adas-0001", "FP16-INT8", "road-segmentation-adas-0001.xml" )
video_out_file = os.path.join(Videos_dir, "output","road_depth_output_video.mp4" )


print("CUDA Available: ", torch.cuda.is_available())
print("CUDA Device Count: ", torch.cuda.device_count())
print("Current CUDA Device: ", torch.cuda.current_device())
print("CUDA Device Name: ", torch.cuda.get_device_name(0))


# MiDaS 모델 로드(DPT_Large | DPT_Hybrid | MiDaS_small  택 1)
#midas = torch.hub.load("intel-isl/MiDaS", "MiDaS_small").to('cuda' if torch.cuda.is_available() else 'cpu').eval()
midas = torch.hub.load("intel-isl/MiDaS", "MiDaS_small").to('cuda' if torch.cuda.is_available() else 'cpu').eval()

# MiDaS 전처리 변환
midas_transforms = torch.hub.load("intel-isl/MiDaS", "transforms")

# MiDaS 전처리( small_transform | dpt_transform 택 1)
transform = midas_transforms.small_transform  # 경량화된 변환 사용
#transform = midas_transforms.dpt_transform  # 경량화된 변환 사용

# YOLO 모델 로드
yolo_model = YOLO(yolo_model_file)
device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
yolo_model.to(device)

# =============================
# 2. 비디오 설정
# =============================

cap = cv2.VideoCapture(video_file)
fps = int(cap.get(cv2.CAP_PROP_FPS))
frame_width, frame_height = 640, 480
fourcc = cv2.VideoWriter_fourcc(*'mp4v')
out = cv2.VideoWriter(video_out_file, fourcc, fps, (frame_width, frame_height))

# =============================
# 3. PIP 함수 (Depth 맵을 오른쪽 상단에 오버레이)
# =============================

def picture_in_picture(main_image, overlay_image, scale=0.25, border_size=2, x_margin=20, y_margin=20):
    """Depth 맵을 오른쪽 상단에 오버레이"""
    h, w = main_image.shape[:2]
    new_size = (int(w * scale), int(h * scale))
    overlay_resized = cv2.resize(overlay_image, new_size, interpolation=cv2.INTER_AREA)

    # 테두리 추가
    overlay_with_border = cv2.copyMakeBorder(overlay_resized, border_size, border_size,
                                             border_size, border_size, cv2.BORDER_CONSTANT, value=[255, 255, 255])

    # 위치 설정 및 오버레이 적용
    x_offset = w - overlay_with_border.shape[1] - x_margin
    y_offset = y_margin
    main_image[y_offset:y_offset + overlay_with_border.shape[0],
               x_offset:x_offset + overlay_with_border.shape[1]] = overlay_with_border
    return main_image

# =============================
# 4. 비디오 처리 루프
# =============================

frame_id = 0
start_time = time.time()
fps_display = ""

progress_bar = tqdm(total=int(cap.get(cv2.CAP_PROP_FRAME_COUNT)), desc="Processing Frames", unit="frame")
while cap.isOpened():
    ret, frame = cap.read()
    if not ret:
        break

    frame = cv2.resize(frame, (frame_width, frame_height))
    frame_id += 1

    # =============================
    # 4.1 MiDaS Depth 예측
    # =============================
    small_frame = cv2.resize(frame, (256, 256), interpolation=cv2.INTER_AREA)
    input_batch = transform(small_frame).to(device, dtype=torch.float32)  # float32 강제 설정
    with torch.no_grad():
        prediction = midas(input_batch)
        depth_map = torch.nn.functional.interpolate(
            prediction.unsqueeze(1), size=(frame_height, frame_width),
            mode='bilinear', align_corners=False
        ).squeeze().cpu().numpy()
    depth_normalized = (depth_map - depth_map.min()) / (depth_map.max() - depth_map.min())
    depth_colored = cv2.applyColorMap((depth_normalized * 255).astype(np.uint8), cv2.COLORMAP_MAGMA)

    # =============================
    # 4.2 YOLO 객체 감지
    # =============================
    results = yolo_model.predict(frame, conf=0.5, verbose=False, device=device, half=True)
    for result in results:
        for bbox in result.boxes:
            xmin, ymin, xmax, ymax = map(int, bbox.xyxy[0])
            label = f"{result.names[int(bbox.cls[0])]} {bbox.conf[0]:.2f}"
            cv2.rectangle(frame, (xmin, ymin), (xmax, ymax), (0, 255, 0), 2)
            cv2.putText(frame, label, (xmin, ymin - 5), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 1)

    # =============================
    # 4.3 Depth 맵을 오버레이
    # =============================
    frame_with_overlay = picture_in_picture(frame, depth_colored)

    # FPS 계산 및 표시
    if frame_id % 10 == 0:
        end_time = time.time()
        fps_display = f"FPS: {10 / (end_time - start_time):.2f}"
        start_time = time.time()
    cv2.putText(frame_with_overlay, fps_display, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

    # 결과 저장 및 출력
    out.write(frame_with_overlay)
    cv2.imshow("YOLO and Depth Map", frame_with_overlay)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break
    progress_bar.update(1)

# 종료 및 리소스 해제
cap.release()
out.release()
cv2.destroyAllWindows()
progress_bar.close()
