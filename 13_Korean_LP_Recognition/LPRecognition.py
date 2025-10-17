import cv2
import easyocr
import numpy as np
from PIL import ImageFont, ImageDraw, Image
import torch
import os
import time
import concurrent.futures
from ultralytics import YOLO
import re

# 경로 설정
script_dir = os.path.dirname(os.path.abspath(__file__))
os.chdir(script_dir)
Models_dir = os.path.abspath(os.path.join(script_dir, "../00_Models"))
Font_dir = os.path.abspath(os.path.join(script_dir, "../00_font"))
Videos_dir = os.path.abspath(os.path.join(script_dir, "../00_Sample_Video"))

def load_model():
    print("모델 로드 중...")
    car_m = YOLO(os.path.join(Models_dir, "yolov8s.pt"))  # YOLOv8 차량 모델 로드
    lp_m = torch.hub.load('ultralytics/yolov5', 'custom', os.path.join(Models_dir, "lp_det.pt"))  # YOLOv5 번호판 모델 로드

    reader = easyocr.Reader(['en'], detect_network='craft', recog_network='best_acc',
                             user_network_directory=os.path.join(Models_dir, 'lp_models/user_network'),
                             model_storage_directory=os.path.join(Models_dir, 'lp_models/models'))
    print("모델 로드 완료!")
    return car_m, lp_m, reader

def detect_vehicles(car_m, frame):
    results = car_m(frame)  # YOLOv8 모델 호출
    filtered_boxes = []
    for result in results:  # results는 리스트 형태
        for box in result.boxes.data.cpu().numpy():
            x1, y1, x2, y2, score, class_id = box
            if int(class_id) in [2, 3, 5, 7]:  # 자동차, 버스, 트럭 클래스 필터링
                filtered_boxes.append([x1, y1, x2, y2, score, class_id])
    return filtered_boxes

def process_detected_text(detected_text):
    # 공백 제거
    detected_text = detected_text.replace(" ", "")
    
    # 중복된 텍스트 제거
    if len(detected_text) >= 7 and detected_text[:7] == detected_text[-7:]:
        detected_text = detected_text[:7]
    elif len(detected_text) >= 8 and detected_text[:8] == detected_text[-8:]:
        detected_text = detected_text[:8]
        detected_text = detected_text[:8]
    
    # 한국 번호판 형식 정규식 패턴
    korean_plate_pattern = re.compile(r'^\d{2,3}[가-힣]\d{4}$')
    
    # 한국 번호판 형식과 일치하는지 확인
    if korean_plate_pattern.match(detected_text):
        return detected_text
    else:
        return None

def detect_license_plates(lp_m, reader, frame, car_boxes):
    lp_results = []

    for box in car_boxes:
        x1, y1, x2, y2 = [int(coord) for coord in box[:4]]
        roi = frame[y1:y2, x1:x2]

        # 번호판 탐지
        results = lp_m(Image.fromarray(roi))  # YOLOv5 모델 호출
        lp_boxes = results.xyxy[0].cpu().numpy()
        for lp_box in lp_boxes:
            lx1, ly1, lx2, ly2 = [int(coord) for coord in lp_box[:4]]
            license_plate = roi[ly1:ly2, lx1:lx2]
            try:
                gray_plate = cv2.cvtColor(license_plate, cv2.COLOR_BGR2GRAY)
                text = reader.readtext(gray_plate, detail=0)

                if text:
                    detected_text = "".join(text)
                    processed_text = process_detected_text(detected_text)
                    if processed_text:
                        lp_results.append(((x1 + lx1, y1 + ly1, x1 + lx2, y1 + ly2), processed_text))
            except Exception as e:
                print(f"번호판 인식 오류: {e}")
    return lp_results

def draw_results(frame, car_boxes, lp_results):
    fontpath = os.path.join(Font_dir, "SpoqaHanSansNeo-Light.ttf")
    if not os.path.exists(fontpath):
        fontpath = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
    font = ImageFont.truetype(fontpath, 24)

    pil_frame = Image.fromarray(frame)
    draw = ImageDraw.Draw(pil_frame)

    for lp_box, text in lp_results:
        x1, y1, x2, y2 = lp_box
        draw.rectangle([(x1, y1), (x2, y2)], outline="red", width=2)
        draw.text((x1, y1 - 20), text, font=font, fill=(0, 255, 0))

    return np.array(pil_frame)

def process_frame(car_m, lp_m, reader, frame, process_frame, last_lp_results, frame_counter):
    to_draw = np.array(frame)
    car_boxes = detect_vehicles(car_m, to_draw) if process_frame else []

    if process_frame:
        lp_results = detect_license_plates(lp_m, reader, to_draw, car_boxes)
        if lp_results:
            last_lp_results["results"] = lp_results
            last_lp_results["counter"] = frame_counter
    else:
        if frame_counter - last_lp_results["counter"] < 10:  # 유지 프레임 수 설정 (10 프레임 유지)
            lp_results = last_lp_results["results"]
        else:
            lp_results = []

    processed_frame = draw_results(to_draw, car_boxes, lp_results)
    return processed_frame

def main():
    car_m, lp_m, reader = load_model()

    video_in_dir = os.path.join(Videos_dir, "input")
    video_out_dir = os.path.join(Videos_dir, "output", "lp_output.mp4")
    cap = cv2.VideoCapture(os.path.join(video_in_dir, "sample_lp.mp4"))

    if not cap.isOpened():
        print("비디오 파일을 열 수 없습니다.")
        return

    fourcc = cv2.VideoWriter_fourcc(*"mp4v")
    fps = int(cap.get(cv2.CAP_PROP_FPS))
    width = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    height = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    out = cv2.VideoWriter(video_out_dir, fourcc, fps, (width, height))

    print("비디오에서 차량 번호판 탐지를 시작합니다. 'q'를 눌러 종료하세요.")
    prev_time = time.time()
    frame_count = 0
    last_lp_results = {"results": [], "counter": 0}

    with concurrent.futures.ThreadPoolExecutor() as executor:
        while True:
            ret, frame = cap.read()
            if not ret:
                print("비디오가 끝났습니다.")
                break

            current_time = time.time()
            fps_display = 1 / (current_time - prev_time)
            prev_time = current_time

            process_this_frame = (frame_count % 5 == 0)

            future = executor.submit(
                process_frame, car_m, lp_m, reader, frame, process_this_frame, last_lp_results, frame_count
            )
            processed_frame = future.result()

            cv2.putText(
                processed_frame,
                f"FPS: {fps_display:.2f}",
                (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX,
                1,
                (255, 0, 0),
                2,
                cv2.LINE_AA,
            )

            out.write(processed_frame[:, :, ::-1])  # RGB에서 BGR로 변환
            cv2.imshow("KOREAN LP Detection", processed_frame[:, :, ::-1])

            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

            frame_count += 1

    cap.release()
    out.release()
    cv2.destroyAllWindows()
    print(f"결과가 저장되었습니다: {video_out_dir}")

if __name__ == "__main__":
    main()
