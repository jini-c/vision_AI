import cv2
import time
from openvino.runtime import Core
import numpy as np
import os
from pathlib import Path

#----------------------------------------------------------------
#TABLE 1


# ----------------- !!! 경로 설정 (수정된 부분) !!! -----------------
# 현재 스크립트 파일의 디렉토리를 기준으로 경로 설정
script_dir = os.path.dirname(os.path.abspath(__file__))
base_dir = os.path.abspath(os.path.join(script_dir, os.pardir)) # 상위 폴더

# 비디오와 모델이 저장된 디렉토리 경로
videos_dir = os.path.abspath(os.path.join(script_dir, "../00_Sample_Video/input")) # <<< 수정된 부분
models_dir = os.path.join(base_dir, "00_Models", "road-segmentation-adas-0001")

# 사용할 비디오 파일 경로
VIDEO_SOURCE = os.path.join(videos_dir, "paper", "test_video.mp4") # <<< 수정된 부분

# 사용할 모델 파일 경로 딕셔너리
MODELS = {
    "FP32": Path(os.path.join(models_dir, "FP32", "road-segmentation-adas-0001.xml")),
    "FP16": Path(os.path.join(models_dir, "FP16", "road-segmentation-adas-0001.xml")),
    "INT8": Path(os.path.join(models_dir, "FP16-INT8", "road-segmentation-adas-0001.xml")),
}

NUM_FRAMES_TO_MEASURE = 500 # FPS 측정을 위해 처리할 총 프레임 수
WARMUP_FRAMES = 50          # 성능 안정화를 위한 웜업 프레임 수
# -----------------------------------------------------------------------------


def measure_model_size(model_xml_path):
    """Measures the total size (xml + bin) of the model in MB."""
    try:
        # 입력이 Path 객체가 아닐 수 있으므로 변환
        model_xml_path = Path(model_xml_path)
        model_bin_path = model_xml_path.with_suffix(".bin")
        size_xml = os.path.getsize(model_xml_path)
        size_bin = os.path.getsize(model_bin_path)
        total_size_mb = (size_xml + size_bin) / (1024 * 1024)
        return total_size_mb
    except FileNotFoundError:
        return 0

def measure_fps(compiled_model, video_source):
    """Measures the average FPS for a compiled model."""
    input_layer = compiled_model.input(0)
    N, C, H, W = input_layer.shape
    
    cap = cv2.VideoCapture(video_source)
    if not cap.isOpened():
        print(f"Error: Could not open video source at '{video_source}'")
        return 0

    total_time = 0
    frame_count = 0

    print(f"Measuring FPS for {NUM_FRAMES_TO_MEASURE} frames after {WARMUP_FRAMES} warmup frames...")
    while frame_count < NUM_FRAMES_TO_MEASURE + WARMUP_FRAMES:
        ret, frame = cap.read()
        if not ret:
            # 비디오가 짧으면 처음부터 다시 시작하여 프레임 수 확보
            print("Video ended, restarting from the beginning.")
            cap.set(cv2.CAP_PROP_POS_FRAMES, 0)
            continue
            
        # Preprocessing
        resized_image = cv2.resize(frame, (W, H))
        input_data = np.expand_dims(resized_image.transpose(2, 0, 1), axis=0)
        
        # Inference
        start_time = time.perf_counter()
        compiled_model([input_data])
        end_time = time.perf_counter()
        
        # 웜업 이후 시간만 측정
        if frame_count >= WARMUP_FRAMES:
            total_time += (end_time - start_time)
        
        frame_count += 1

    cap.release()
    
    avg_fps = NUM_FRAMES_TO_MEASURE / total_time if total_time > 0 else 0
    return avg_fps

# 메인 실행 블록
if __name__ == "__main__":
    core = Core()
    results = {}

    for name, path in MODELS.items():
        print(f"\n--- Evaluating {name} Model ---")
        
        # 파일 존재 여부 확인
        if not path.exists():
            print(f"Skipping {name}: Model file not found at '{path}'")
            continue
            
        size = measure_model_size(path)
        
        # 모델 컴파일 (CPU)
        try:
            compiled_model = core.compile_model(model=str(path), device_name="CPU")
            fps = measure_fps(compiled_model, VIDEO_SOURCE)
            results[name] = {"FPS": fps, "Size": size}
            print(f"Result -> Size: {size:.2f} MB, FPS: {fps:.2f}")
        except Exception as e:
            print(f"Error evaluating model {name}: {e}")

    # 결과가 있는 경우에만 요약 출력
    if not results:
        print("\n--- No models were evaluated. Please check file paths. ---")
    else:
        # 결과 출력 (Table 1 데이터)
        print("\n--- Summary (Table 1 Data) ---")
        print(f"{'Precision':<15} | {'FPS (CPU)':>10} | {'Speed Gain (%)':>15} | {'Size (MB)':>10} | {'Reduction (%)':>15}")
        print("-" * 75)
        
        baseline_fps = results.get("FP32", {}).get("FPS", 0)
        baseline_size = results.get("FP32", {}).get("Size", 1) # 0으로 나누는 것 방지

        for name, data in results.items():
            fps = data["FPS"]
            size = data["Size"]
            speed_gain = ((fps - baseline_fps) / baseline_fps * 100) if baseline_fps > 0 else 0
            reduction = ((baseline_size - size) / baseline_size * 100) if baseline_size > 0 else 0
            
            # 출력 포맷팅
            gain_str = f"{speed_gain:>15.2f}%" if name != "FP32" else f"{'-':>15}"
            reduct_str = f"{reduction:>15.2f}%" if name != "FP32" else f"{'-':>15}"

            print(f"{name:<15} | {fps:>10.2f} | {gain_str} | {size:>10.2f} | {reduct_str}")