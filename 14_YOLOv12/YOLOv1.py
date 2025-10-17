import cv2
import time
import os
import torch
from ultralytics import YOLO

# 경로 설정
script_dir = os.path.dirname(os.path.abspath(__file__))
os.chdir(script_dir)
model_path = os.path.abspath(os.path.join(script_dir, "../00_Models/yolov12m.pt"))
video_path = os.path.abspath(os.path.join(script_dir, "../00_Sample_video/input/road.mp4"))

video_out_path = os.path.abspath(os.path.join(script_dir, "../00_Sample_video/output/outputv12.mp4"))

# YOLO 모델 로드 (GPU 사용: 모델을 CUDA로 이동)
model = YOLO(model_path)
if torch.cuda.is_available():
    model.model.to("cuda:0")
else:
    print("GPU를 사용할 수 없습니다. CPU로 실행됩니다.")

# 비디오 파일 열기
cap = cv2.VideoCapture(video_path)
#cap = cv2.VideoCapture(0)


if not cap.isOpened():
    print("비디오 파일을 열 수 없습니다!")
    exit()

frame_width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
frame_height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
fps_input = cap.get(cv2.CAP_PROP_FPS) if cap.get(cv2.CAP_PROP_FPS) > 0 else 20.0

# MP4 파일로 저장 (mp4v 코덱 사용)
fourcc = cv2.VideoWriter_fourcc(*'mp4v')
out = cv2.VideoWriter(video_out_path, fourcc, fps_input, (frame_width, frame_height))

prev_time = time.time()

while True:
    ret, frame = cap.read()
    if not ret:
        break

    # GPU에서 ultralytics 모델로 추론 수행
    results = model(frame)
    result = results[0]  # 한 프레임에 대한 결과

    # 검출된 객체들의 bounding box와 레이블을 붉은색으로 표시
    if result.boxes is not None:
        for box in result.boxes:
            b = box.xyxy.cpu().numpy()[0]
            x1, y1, x2, y2 = map(int, b)
            conf = box.conf.cpu().numpy()[0]
            cls = int(box.cls.cpu().numpy()[0])
            label = f"{model.names[cls]} {conf*100:.1f}%"
            color = (0, 255, 0)  # 붉은색
            cv2.rectangle(frame, (x1, y1), (x2, y2), color, 2)
            cv2.putText(frame, label, (x1, y1 - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, color, 2)

    # FPS 계산 및 왼쪽 상단에 붉은색 굵은 텍스트로 표시
    curr_time = time.time()
    fps = 1 / (curr_time - prev_time)
    prev_time = curr_time
    cv2.putText(frame, f"YOLO v12 FPS: {int(fps)}", (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 3)

    # 결과 영상 실시간 표출 및 MP4 파일로 저장
    cv2.imshow("YOLO v12 Object Detection", frame)
    out.write(frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

cap.release()
out.release()
cv2.destroyAllWindows()
