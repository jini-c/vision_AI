# VISION_AI 프로젝트 모음

이 저장소는 다양한 컴퓨터 비전 AI 프로젝트와 실험 결과물을 모아놓은 곳입니다.

## 포함된 프로젝트

-   **`04_Setting`**: 기본적인 OpenCV 및 dlib 예제 (졸음 감지 등)
-   **`05_Open_Vocabulary_Object_Detection`**: YOLO-World와 ResNet을 사용한 개방형 어휘 객체 탐지
-   **`06_PoseEstimation`**: 2D 자세 추정 및 행동 인식
-   **`08_RoadSegmentation`**: 도로 분할 프로젝트 (학습 후 양자화(PTQ), 효율성 측정 등)
-   **`09_YOLO_Depth`**: YOLO를 사용한 깊이 추정
-   **`10_Age_Gender Recognition`**: 나이 및 성별 인식
-   **`12_3D object Detection`**: 3D 객체 탐지
-   **`13_Korean_LP_Recognition`**: 한국어 번호판 인식
-   **`14_YOLOv12`**: YOLOv12 관련 프로젝트
-   **`15_YOLOE(app)`**: YOLO-E 기반 애플리케이션
-   **`my_yolo_app`**: YOLO 기반 자세 분류 웹 애플리케이션 (Flask)

## 설정 및 사용법

각 프로젝트는 해당 디렉토리 안에 독립적으로 구성되어 있습니다. 개별 프로젝트의 구체적인 설정 및 사용법은 각 폴더 내의 `README.md` 파일을 참고해주세요.

`my_yolo_app`의 경우, 아래 명령어로 필요한 라이브러리를 설치할 수 있습니다.
```bash
pip install -r my_yolo_app/requirements.txt
```

## Git LFS 사용 안내

이 저장소는 `00_Sample_video` 디렉토리의 대용량 비디오 파일을 관리하기 위해 [Git LFS (Large File Storage)](https://git-lfs.github.com/)를 사용합니다. 비디오 파일을 올바르게 다운로드하려면 Git LFS를 먼저 설치해야 합니다.

```bash
git lfs install
```
