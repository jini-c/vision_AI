import os
import cv2
import numpy as np
import time
from ultralytics import YOLO
from collections import deque
from libs.bbox3d_utils import *  # 3D 바운딩 박스 관련 유틸리티 함수
from libs.Plotting import *  # 3D 시각화를 위한 함수
from Train import *  # 모델 학습 관련 함수
from tensorflow.keras.applications import *
from tensorflow.keras.models import *
from tensorflow.keras.optimizers import *
from tensorflow.keras.layers import *
from tensorflow.keras.callbacks import TensorBoard, EarlyStopping, ModelCheckpoint
from tensorflow.keras.losses import Loss
from tensorflow.keras import backend as K
from tensorflow.keras.utils import get_custom_objects
import matplotlib.pyplot as plt

## select 3D model  and input size
####### select model  ########
# select_3d_model = 'resnet50'
# select_3d_model ='resnet101'
# select_3d_model = 'resnet152'
# select_3d_model = 'vgg11'
# select_3d_model = 'vgg16'
# select_3d_model = 'vgg19'
# select_3d_model = 'efficientnetb0'
# select_3d_model = 'efficientnetb5'
# select_3d_model = 'mobilenetv2'
select_3d_model = "mobilenetv2"  # 사용할 모델 선택

# TensorFlow 로그 레벨 설정 (경고 및 정보 로그를 무시)
os.environ["TF_CPP_MIN_LOG_LEVEL"] = "3"
#=================================================================
#초기값 설정
#=================================================================
#실행 경로 설정 
# 경로 설정
script_dir = os.path.dirname(os.path.abspath(__file__))
os.chdir(script_dir)
Models_dir = os.path.abspath(os.path.join(script_dir, "../00_Models"))
Dataset_dir = os.path.abspath(os.path.join(script_dir, "../00_Dataset"))
Videos_dir = os.path.abspath(os.path.join(script_dir, "../00_Sample_Video"))

# 모델 및 비디오 디렉토리 설정
bbox_2d_model_dir = os.path.join(Models_dir, "yolov8n-seg.pt")
bbox_3d_model_dir = os.path.join(Models_dir, select_3d_model)  # 모델 디렉토리 경로

video_in_dir = os.path.join(Videos_dir, "input")  # 비디오 입력 경로
video_out_dir = os.path.join(Videos_dir, "output")  # 비디오 출력 경로


# 비디오 읽기
video = cv2.VideoCapture(
    os.path.join(video_in_dir, "3d_drive.mp4")
)

# 커스텀 객체 등록 (모델 학습 시 사용한 커스텀 함수들)
get_custom_objects().update(
    {
        "orientation_loss": orientation_loss,  # 모델의 손실 함수 등록
        "l2_normalize": tf.nn.l2_normalize,  # L2 정규화 함수 등록
    }
)

# 3D 모델 로드
weights_file = os.path.join(bbox_3d_model_dir, f"{select_3d_model}_weights.keras")
if os.path.exists(weights_file):
    bbox3d_model = load_model(
        weights_file,
        custom_objects={
            "orientation_loss": orientation_loss,
            "l2_normalize": tf.nn.l2_normalize,
        },
    )
    print(bbox3d_model.summary())  # 모델 요약 출력
    print(f"Loading file {weights_file}...!")
else:
    print(f"The file {weights_file} does not exist.")

# 모델과 관련된 상수 및 설정
bin_size = 6  # 바운딩 박스를 위한 빈 크기 설정
input_shape = (224, 224, 3)  # 모델 입력 크기 설정
trained_classes = ["car", "cyclist", "pedestrian"]  # 훈련된 클래스들

# KITTI 카메라 프로젝션 행렬 (P2)
P2 = np.array(
    [
        [721.5377, 0.0, 609.5593, 44.85728],
        [0.0, 721.5377, 172.8540, 0.2163791],
        [0.0, 0.0, 1.0, 0.002745884],
    ]
)

# 평균 크기 (차량, 보행자 등)
dims_avg = {
    "Car": np.array([1.52131309, 1.64441358, 3.85728004]),
    "Van": np.array([2.18560847, 1.91077601, 5.08042328]),
    "Truck": np.array([3.07044968, 2.62877944, 11.17126338]),
    "Pedestrian": np.array([1.75562272, 0.67027992, 0.87397566]),
    "Cyclist": np.array([1.73456498, 0.58174006, 1.77485499]),
    "Tram": np.array([3.56020305, 2.40172589, 18.60659898]),
}

bbox_2d_model = YOLO(bbox_2d_model_dir)  # YOLO 모델 로드
bbox_2d_model.overrides.update(
    {
        "conf": 0.9,  # Confidence threshold
        "iou": 0.55,  # IOU threshold for Non-Maximum Suppression
        "agnostic_nms": False,  # Class-agnostic NMS (False for per-class NMS)
        "max_det": 1000,  # Maximum number of detections per image
        "classes": 2,  # Filtered for specific classes (e.g., pedestrians, cyclists)
    }
)

yolo_classes = [
    "Pedestrian",
    "Cyclist",
    "Car",
    "Motorcycle",
    "Airplane",
    "Van",
    "Train",
    "Truck",
    "Boat",
]

# 비디오 파일의 해상도 가져오기
frame_width = int(video.get(cv2.CAP_PROP_FRAME_WIDTH))
frame_height = int(video.get(cv2.CAP_PROP_FRAME_HEIGHT))

# 비디오 출력 설정
fourcc = cv2.VideoWriter_fourcc(*"mp4v")
# out = cv2.VideoWriter(f'{video_out_dir}/{select_3d_model}_output_video.mp4', fourcc, 15, (frame_width, frame_height))


# 객체 추적을 위한 딕셔너리
tracking_trajectories = {}


# 2D 바운딩 박스를 YOLO 모델로 처리
def process2D(image, track=True, device="cuda"):
    """
    2D 바운딩 박스를 YOLO 모델로 예측하고, 신뢰도가 0.4 이상인 객체들만 선택합니다.
    """
    bboxes = []
    confidences = []

    if track:
        results = bbox_2d_model.track(image, verbose=False, device=device, persist=True)

        # 기존 트래킹된 객체 ID 업데이트
        for id_ in list(tracking_trajectories.keys()):
            if id_ not in [
                int(bbox.id)
                for predictions in results
                if predictions is not None
                for bbox in predictions.boxes
                if bbox.id is not None
            ]:
                del tracking_trajectories[id_]

        # 예측된 바운딩 박스 처리
        for predictions in results:
            if predictions is None or predictions.boxes is None:
                continue

            for bbox in predictions.boxes:
                bbox_coords = bbox.xyxy.cpu().numpy().flatten()
                xmin, ymin, xmax, ymax = map(int, bbox_coords)
                confidence = float(bbox.conf.cpu().numpy().item())

                if confidence < 0.4:  # 신뢰도가 0.4 미만인 객체는 처리하지 않음
                    continue

                class_id = int(bbox.cls.cpu().numpy().item())
                obj_id = (
                    int(bbox.id.cpu().numpy().item()) if bbox.id is not None else None
                )

                bboxes.append([xmin, ymin, xmax, ymax, confidence, class_id, obj_id])
                confidences.append(confidence)

                # 라벨 추가
                label = f"ID: {obj_id} {yolo_classes[class_id]} {confidence * 100:.1f}%"
                cv2.rectangle(image, (xmin, ymin), (xmax, ymax), (0, 0, 255), 2)
                text_size = cv2.getTextSize(label, cv2.FONT_HERSHEY_SIMPLEX, 0.5, 1)[0]
                cv2.rectangle(
                    image,
                    (xmin, ymin - text_size[1] - 10),
                    (xmin + text_size[0], ymin),
                    (30, 30, 30),
                    cv2.FILLED,
                )
                cv2.putText(
                    image,
                    label,
                    (xmin, ymin - 7),
                    cv2.FONT_HERSHEY_SIMPLEX,
                    0.5,
                    (255, 255, 255),
                    1,
                )

                centroid_x = (xmin + xmax) / 2
                centroid_y = (ymin + ymax) / 2
                if obj_id is not None and obj_id not in tracking_trajectories:
                    tracking_trajectories[obj_id] = deque(maxlen=5)
                if obj_id is not None:
                    tracking_trajectories[obj_id].append((centroid_x, centroid_y))

            # 객체 궤적 그리기
            for id_, trajectory in tracking_trajectories.items():
                for i in range(1, len(trajectory)):
                    thickness = int(2 * (i / len(trajectory)) + 1)
                    cv2.line(
                        image,
                        (int(trajectory[i - 1][0]), int(trajectory[i - 1][1])),
                        (int(trajectory[i][0]), int(trajectory[i][1])),
                        (255, 255, 255),
                        thickness,
                    )

    return image, bboxes, confidences


# 3D 바운딩 박스 계산
def process3D(img, bboxes2d):
    """
    2D 바운딩 박스를 사용하여 3D 바운딩 박스를 계산합니다.
    """
    DIMS = []  # 평균 크기 저장
    bboxes = []  # 최종 3D 바운딩 박스 결과 저장
    previous_location = None  # 이전 프레임의 위치

    for item in bboxes2d:
        if len(item) < 7:
            print(f"Unexpected item format: {item}")
            continue

        bbox_coords = item[:4]  # [xmin, ymin, xmax, ymax]
        bbox_coords = np.array(bbox_coords, dtype=int)  # numpy 배열로 변환
        xmin, ymin, xmax, ymax = bbox_coords

        # 2D 바운딩 박스 좌표 보정
        xmin, ymin = max(0, xmin), max(0, ymin)
        xmax, ymax = min(img.shape[1], xmax), min(img.shape[0], ymax)

        confidence = item[4]  # Confidence 값
        classes = item[5]  # Class ID
        obj_id = item[6]  # Object ID

        # Crop 이미지 처리
        crop = img[ymin:ymax, xmin:xmax]
        patch = tf.convert_to_tensor(crop, dtype=tf.float32) / 255.0
        patch = tf.image.resize(patch, (224, 224))
        patch = tf.expand_dims(patch, axis=0)

        # 3D 모델 예측
        prediction = bbox3d_model.predict(patch, verbose=0)
        dim = prediction[0][0]
        bin_anchor = prediction[1][0]
        bin_confidence = prediction[2][0]

        # 평균 크기 보정
        try:
            dim += dims_avg[yolo_classes[int(classes)]]  # 평균 크기 보정
            DIMS.append(dim)
        except KeyError:
            dim = DIMS[-1]

        bbox_ = [xmin, ymin, xmax, ymax]
        theta_ray = calc_theta_ray(img, bbox_, P2)
        alpha = recover_angle(bin_anchor, bin_confidence, bin_size)
        final_alpha = (alpha - theta_ray + np.pi) % (2 * np.pi) - np.pi

        # 회전 보정 (여기에서 alpha와 theta_ray 보정)
        alpha -= 20.5  # 필요에 따라 값을 실험적으로 조정하세요.
        theta_ray -= 0.1  # theta_ray에 대한 보정값도 조정 가능

        # 3D 위치 계산
        location, _ = calc_location(
            dimension=dim,
            proj_matrix=P2,
            box_2d=bbox_,
            alpha=final_alpha,
            theta_ray=theta_ray,
        )
        location = np.array(location)  # numpy 배열로 변환

        # 위치 보정 및 스무딩
        if previous_location is not None:
            location = 0.8 * previous_location + 0.2 * location  # numpy 연산
        previous_location = location

        # **X 좌표를 보정하여 후면을 왼쪽으로 이동**
        location[0] += 50.5  # tx 값에 음수를 더하여 왼쪽으로 이동시킴

        # 디버깅 출력
        # print(f"Object ID: {obj_id}, Location: {location}, Final Alpha: {final_alpha}")

        bboxes.append(
            [
                bbox_,
                dim,
                final_alpha,
                theta_ray,
                bin_anchor,
                bin_confidence,
                classes,
                location,
                obj_id,
            ]
        )

    return bboxes


# 궤적 데이터 저장
tracking_trajectories = {}

frameId, start_time = 0, time.time()
BEV_plot, TracK = True, True
fps = "FPS: 0.00"
Write_Flag = 0

# 비디오 프레임별 처리 시작
while True:
    success, frame = video.read()
    if not success:
        break

    img = frame.copy()
    img2 = frame.copy()
    img2D, bboxes2d, confidences2d = process2D(img2, track=TracK)
    img3D = frame.copy()
    plot3dbev = Plot3DBoxBev(P2)

    # BEV 초기화
    bev_image = np.zeros((500, 500, 3), dtype=np.uint8)

    if bboxes2d:
        bboxes3D = process3D(img, bboxes2d)
        for i, bbox in enumerate(bboxes3D):
            (
                bbox_coords,
                dim,
                alpha,
                theta_ray,
                orient,
                conf,
                classes,
                location,
                obj_id,
            ) = bbox
            confidence = confidences2d[i]
            label = (
                yolo_classes[int(classes)]
                if int(classes) < len(yolo_classes)
                else "Unknown"
            )

            # 3D 시각화
            plot3d(
                img3D,
                P2,
                bbox_coords,
                dim,
                alpha,
                theta_ray,
                label=label,
                confidence=confidence,
                obj_id=obj_id,
            )

            # 3D 객체의 위치와 회전 정보를 계산하고, 이를 3D 공간에서 시각화하는 과정
            if BEV_plot:
                obj = KITTIObject()
                obj.name = str(
                    yolo_classes[int(classes)]
                )  # 'cpu().numpy()'를 제거하고 'int(classes)'로 처리

                # 객체가 이미지 프레임 내에서 잘린 정도를 나타내는 변수
                obj.truncation = float(0.00)

                # 객체가 다른 객체나 배경에 의해 가려진 정도를 나타내는 변수
                obj.occlusion = int(-1)

                # 2D 바운딩 박스 좌표를 설정
                obj.xmin, obj.ymin, obj.xmax, obj.ymax = (
                    int(bbox_coords[0]),
                    int(bbox_coords[1]),
                    int(bbox_coords[2]),
                    int(bbox_coords[3]),
                )

                # 회전 각도입니다. orient와 conf는 모델이 예측한 회전과 관련된 정보이며, bin_size는 회전 각도를 세분화하는 데 사용되는 변수.
                obj.alpha = recover_angle(orient, conf, bin_size)

                # 3D 바운딩 박스의 크기를 설정. dim은 [높이, 너비, 길이]로 구성된 3D 객체의 크기를 의미. 이 크기는 객체가 실제 3D 공간에서 차지하는 부피를 정의
                obj.h, obj.w, obj.l = dim[0], dim[1], dim[2]

                # 객체의 회전 행렬을 계산합니다. P2는 카메라의 프로젝션 행렬로, 이를 사용하여 객체가 3D 공간에서 어떻게 회전하는지 계산
                obj.rot_global, rot_local = compute_orientaion(P2, obj)

                # 객체의 위치를 3D 공간에서 계산. 이 함수는 프로젝션 행렬(P2)과 객체의 회전 행렬(rot_local)을 사용하여,
                # 객체가 3D 공간에서 어느 위치에 있는지(tx, ty, tz) 계산
                obj.tx, obj.ty, obj.tz = translation_constraints(P2, obj, rot_local)

                # 객체의 최종 회전을 계산합니다.
                # alpha는 객체의 기본 회전 각도이고,
                # theta_ray는 카메라의 시선에서의 회전 각도를 의미.
                # 두 값을 더해서 객체가 3D 공간에서 얼마나 회전했는지를 계산
                rot_y = alpha + theta_ray

                """3D 시각화
                class_object: 객체의 클래스 (예: 'car', 'pedestrian' 등)
                bbox: 객체의 2D 바운딩 박스 좌표
                dim: 객체의 3D 크기 (h, w, l)
                loc: 객체의 3D 위치 (tx, ty, tz)
                rot_y: 객체의 회전 각도 (y축 기준 회전)
                objId: 객체의 ID (객체 추적을 위한 ID)
                """
                plot3dbev.plot(
                    img=img3D,
                    class_object=obj.name.lower(),
                    bbox=[obj.xmin, obj.ymin, obj.xmax, obj.ymax],
                    dim=[obj.h, obj.w, obj.l],
                    loc=[obj.tx, obj.ty, obj.tz],
                    rot_y=rot_y,
                    objId=[obj_id] if not isinstance(obj_id, list) else obj_id,
                )

    if BEV_plot:
        plot3dbev.plot(img=img3D)
        img3D = plot3dbev.show_result()

    # FPS 계산
    frameId += 1
    if frameId % 10 == 0:
        fps = f"FPS: {frameId / (time.time() - start_time):.2f}"

    # FPS 출력
    cv2.putText(
        img3D, f"{fps}", (10, 50), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 255), 2
    )

    height2, width2, channels = img3D.shape

    # Combined 이미지 출력
    cv2.imshow("img3D", img3D)

    # BEV와 병합된 이미지 출력 (BEV_plot이 True인 경우)
    if Write_Flag == 0:
        out = cv2.VideoWriter(
            f"{video_out_dir}/{select_3d_model}_output_video.mp4",
            fourcc,
            15,
            (width2, height2),
        )
        Write_Flag = 1

    out.write(img3D)

    if cv2.waitKey(1) & 0xFF == ord("q"):
        break

print("End of video")
video.release()
out.release()
cv2.destroyAllWindows()
