import openvino.runtime as ov
import nncf
import numpy as np
import cv2
from pathlib import Path
import os
import glob

# ----------------- 경로 설정 -----------------
# (경로 설정 부분은 변경 없음)
script_dir = os.path.dirname(os.path.abspath(__file__))
base_dir = os.path.abspath(os.path.join(script_dir, os.pardir)) # 상위 폴더
models_base_dir = os.path.join(base_dir, "00_Models", "road-segmentation-adas-0001")
calibration_data_base_dir = os.path.join(base_dir, "00_Sample_Video", "input", "paper")
FP32_MODEL_XML = Path(os.path.join(models_base_dir, "FP32", "road-segmentation-adas-0001.xml"))
FP16_MODEL_XML = Path(os.path.join(models_base_dir, "FP16", "road-segmentation-adas-0001.xml"))
INT8_MODEL_XML = Path(os.path.join(models_base_dir, "FP16-INT8", "road-segmentation-adas-0001.xml")) # INT8 폴더로 수정
CALIBRATION_DATA_DIR = Path(os.path.join(calibration_data_base_dir, "dataset"))
# -----------------------------------------------------------------------------

# 이미지 전처리 함수
def preprocess_image(image_path, input_shape):
    image = cv2.imread(str(image_path))
    if image is None: return None
    h, w = input_shape[2], input_shape[3]
    resized_image = cv2.resize(image, (w, h))
    input_data = np.expand_dims(resized_image.transpose(2, 0, 1), axis=0)
    return input_data

# 보정 데이터셋 생성 함수
# <<< 수정: 이제 데이터셋과 이미지 개수를 함께 반환함 >>>
def create_calibration_dataset(model, data_dir):
    input_layer = model.input(0)
    image_paths = [p for p in Path(data_dir).glob("**/*") if p.suffix.lower() in ('.jpg', '.jpeg', '.png', '.bmp')]
    if not image_paths:
        raise RuntimeError(f"Error: No images found in '{data_dir}'")
    
    num_images = len(image_paths)
    print(f"Found {num_images} images for INT8 calibration.")
    
    def transform_fn(data_item):
        return preprocess_image(data_item, input_layer.shape)
    
    dataset = nncf.Dataset(image_paths, transform_fn)
    return dataset, num_images

# 메인 실행 블록
if __name__ == "__main__":
    
    # --- 사전 확인 ---
    if not FP32_MODEL_XML.exists():
        print(f"Error: Source FP32 model not found at '{FP32_MODEL_XML}'")
        exit()
    if not CALIBRATION_DATA_DIR.exists():
        print(f"Error: Calibration data directory not found at '{CALIBRATION_DATA_DIR}'")
        exit()

    # --- 1. 원본 FP32 모델 로드 ---
    print(f"STEP 1: Loading source FP32 model from:\n  {FP32_MODEL_XML}")
    core = ov.Core()
    fp32_model = core.read_model(model=FP32_MODEL_XML)

    # --- 2. FP32 -> FP16 모델 생성 및 저장 ---
    print(f"\nSTEP 2: Creating FP16 model via compression...")
    os.makedirs(FP16_MODEL_XML.parent, exist_ok=True)
    ov.save_model(fp32_model, str(FP16_MODEL_XML), compress_to_fp16=True)
    print(f"  ✅ Successfully saved FP16 model to:\n     {FP16_MODEL_XML}")
    
    # --- 3. FP32 -> INT8 모델 생성 및 저장 ---
    print(f"\nSTEP 3: Creating INT8 model via post-training quantization...")
    
    # <<< 수정: 데이터셋과 이미지 개수를 모두 받음 >>>
    calibration_dataset, num_calibration_images = create_calibration_dataset(fp32_model, CALIBRATION_DATA_DIR)

    # 3-2. NNCF 양자화 실행
    print("  Running INT8 quantization (this may take some time)...")
    quantized_model = nncf.quantize(
        model=fp32_model,
        calibration_dataset=calibration_dataset,
        preset=nncf.QuantizationPreset.PERFORMANCE,
        # <<< 수정: len() 대신 미리 구해놓은 이미지 개수를 사용 >>>
        subset_size=min(100, num_calibration_images)
    )

    # 3-3. 양자화된 INT8 모델 저장
    os.makedirs(INT8_MODEL_XML.parent, exist_ok=True)
    ov.save_model(quantized_model, str(INT8_MODEL_XML))
    print(f"  ✅ Successfully saved INT8 model to:\n     {INT8_MODEL_XML}")

    print("\n--- All tasks completed. ---")