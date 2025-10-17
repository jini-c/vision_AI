import sys
import os
import argparse
import numpy as np
import cv2
import time
from ultralytics import YOLO
from openvino.runtime import Core


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
video_out_file = os.path.join(Videos_dir, "output","road_seg_output_video.mp4" )


fps = ""
framecount = 0
time1 = 0

#### Color palettes
palette = []
for i in range(256):
    palette.extend((i, i, i))
palette[:3*21] = np.array([
    [0, 0, 0], [128, 0, 0], [0, 128, 0], [128, 128, 0], [0, 0, 128], [128, 0, 128], [0, 128, 128],
    [128, 128, 128], [64, 0, 0], [192, 0, 0], [64, 128, 0], [192, 128, 0], [64, 0, 128], [192, 0, 128],
    [64, 128, 128], [192, 128, 128], [0, 64, 0], [128, 64, 0], [0, 192, 0], [128, 192, 0], [0, 64, 128]
], dtype='uint8').flatten()

# OpenVINO 모델 로드 함수
def load_openvino_model(model_path, device_name="CPU"):
    model_xml = model_path
    model_bin = os.path.splitext(model_xml)[0] + ".bin"
    
    core = Core()
    model = core.read_model(model=model_xml, weights=model_bin)
    compiled_model = core.compile_model(model=model, device_name=device_name)
    
    input_layer = next(iter(compiled_model.inputs))
    output_layer = next(iter(compiled_model.outputs))
    
    n, c, h, w = input_layer.shape
    return n, c, h, w, compiled_model, input_layer, output_layer

# Road Segmentation (OpenVINO)
def roadSeg_openvino(color_image, n1, c1, h1, w1, compiled_model1, input_blob1, output_blob1):
    prepimg = cv2.resize(color_image, (w1, h1))
    prepimg = prepimg.transpose((2, 0, 1)).reshape((1, c1, h1, w1))
    predictions = compiled_model1([prepimg])[output_blob1].squeeze()

    result = predictions.argmax(axis=0).astype('uint8')
    result_resized = cv2.resize(result, (640, 640), interpolation=cv2.INTER_NEAREST)

    output_img = np.zeros((result_resized.shape[0], result_resized.shape[1], 3), dtype=np.uint8)
    road_class_id = 1
    
    #도로 색상 처리
    output_img[result_resized == road_class_id] = [0, 0, 255]

    for label in range(21):
        if label != road_class_id:
            output_img[result_resized == label] = palette[label*3:label*3+3]

    overlayed_image = cv2.addWeighted(color_image, 0.7, output_img, 0.3, 0)
    return overlayed_image

# Object Detection (Ultralytics YOLO)
def detect_objects_yolo(yolo_model, color_image, process_frame):
    if process_frame:
        results = yolo_model(color_image)
        annotated_frame = results[0].plot()
        return annotated_frame
    return color_image

if __name__ == '__main__':
    #파라미터 설정
    parser = argparse.ArgumentParser()   
    
    #비디오 파일은 로컬에 맞게 수정 필요
    parser.add_argument("--video", default=video_file, help="Input video file.")
    #DeepLabv3+ model.xml 모델의 경로는 로컬에 맞게 수정 필요
    parser.add_argument("--deep_model1", default= road_seg_model_file, help="Path of the DeepLabv3+ model.xml.")
    
    #yolov8n.pt은 별도로 다운로드 받아 경로를 로컬에 맞게 수정 필요   
    parser.add_argument("--yolo_model", default= yolo_model_file, help="Path of the Ultralytics YOLO model.")
    parser.add_argument('--vidfps', type=int, default=30, help='FPS of the output video.')
    args = parser.parse_args()

    cap = cv2.VideoCapture(args.video)
    cap.set(cv2.CAP_PROP_FPS, args.vidfps)
    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
    out = cv2.VideoWriter(video_out_file, fourcc, args.vidfps, (640, 640))

    n1, c1, h1, w1, compiled_model1, input_blob1, output_blob1 = load_openvino_model(args.deep_model1, device_name="CPU")
    yolo_model = YOLO(args.yolo_model)

    frame_counter = 0

    while True:
        t1 = time.perf_counter()
        ret, color_image = cap.read()
        if not ret:
            break

        color_image = cv2.resize(color_image, (640, 640))

        # Road segmentation
        segmented_image = roadSeg_openvino(color_image, n1, c1, h1, w1, compiled_model1, input_blob1, output_blob1)

        # YOLO detection on every 5th frame
        outputimg = detect_objects_yolo(yolo_model, segmented_image, frame_counter % 1 == 0)
        
        cv2.putText(outputimg, fps, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 0, 0), 2, cv2.LINE_AA)
        cv2.imshow('Road Segmentation & Object Detection Video', outputimg)
        out.write(outputimg)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

        frame_counter += 1

        framecount += 1
        if framecount >= 10:
            fps = "{:.1f} FPS".format(time1 / 10)
            framecount = 0
            time1 = 0

        t2 = time.perf_counter()
        elapsedTime = t2 - t1
        time1 += 1 / elapsedTime

    cap.release()
    out.release()
    cv2.destroyAllWindows()
