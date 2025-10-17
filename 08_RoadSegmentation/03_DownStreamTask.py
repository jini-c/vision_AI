import cv2
import numpy as np
from openvino.runtime import Core
from ultralytics import YOLO
from pathlib import Path
import pandas as pd
from sklearn.metrics import accuracy_score, precision_score, recall_score, f1_score
import os
import psutil
import threading
import time

# ----------------- !!! 경로 설정 !!! -----------------
script_dir = os.path.dirname(os.path.abspath(__file__))
base_dir = os.path.abspath(os.path.join(script_dir, os.pardir))
image_base_dir = os.path.join(base_dir, "00_Sample_Video", "input", "paper", "dataset")
IMAGE_DIR = Path(image_base_dir)
LABEL_CSV = IMAGE_DIR / "labels.csv"
models_base_dir = os.path.join(base_dir, "00_Models")
YOLO_MODEL_PATH = os.path.join(models_base_dir, "yolov8n.pt")
seg_models_dir = os.path.join(models_base_dir, "road-segmentation-adas-0001")
SEG_MODELS = {
    "FP32": Path(os.path.join(seg_models_dir, "FP32", "road-segmentation-adas-0001.xml")),
    "FP16": Path(os.path.join(seg_models_dir, "FP16", "road-segmentation-adas-0001.xml")),
    "INT8": Path(os.path.join(seg_models_dir, "FP16-INT8", "road-segmentation-adas-0001.xml")),
}
ROAD_CLASS_ID, PERSON_CLASS_ID = 1, 0
# -----------------------------------------------------------------------------

class ResourceMonitor:
    def __init__(self, pid, cpu_cores, interval=0.1):
        self.process = psutil.Process(pid)
        self.cpu_cores = cpu_cores
        self.interval = interval
        self.is_running = False
        self.thread = None
        self.cpu_usage = []
        self.mem_usage_mb = []

    def _monitor_loop(self):
        while self.is_running:
            try:
                raw_cpu_percent = self.process.cpu_percent()
                normalized_cpu_percent = raw_cpu_percent / self.cpu_cores
                self.cpu_usage.append(normalized_cpu_percent)
                self.mem_usage_mb.append(self.process.memory_info().rss / (1024 * 1024))
            except psutil.NoSuchProcess:
                break
            time.sleep(self.interval)

    def start(self):
        self.reset()
        self.is_running = True
        self.process.cpu_percent() 
        self.thread = threading.Thread(target=self._monitor_loop, daemon=True)
        self.thread.start()

    def stop(self):
        self.is_running = False
        if self.thread:
            self.thread.join()

    # <<< 수정: peak_mem 대신 avg_mem 계산 >>>
    def get_results(self):
        if not self.cpu_usage or not self.mem_usage_mb:
            return 0, 0, 0
        avg_cpu = sum(self.cpu_usage) / len(self.cpu_usage)
        peak_cpu = max(self.cpu_usage)
        # 최대 메모리 대신 평균 메모리 계산
        avg_mem = sum(self.mem_usage_mb) / len(self.mem_usage_mb)
        return avg_cpu, peak_cpu, avg_mem
    
    def reset(self):
        self.cpu_usage = []
        self.mem_usage_mb = []

# (SegmentationModel, assess_risk, evaluate_system, calculate_metrics 함수는 이전과 동일)
class SegmentationModel:
    def __init__(self, model_xml_path):
        core = Core()
        self.compiled_model = core.compile_model(model=str(model_xml_path), device_name="CPU")
        self.input_layer, self.output_layer = self.compiled_model.input(0), self.compiled_model.output(0)
        _, _, self.H, self.W = self.input_layer.shape
    def predict(self, image):
        resized_image = cv2.resize(image, (self.W, self.H))
        input_data = np.expand_dims(resized_image.transpose(2, 0, 1), axis=0)
        predictions = self.compiled_model([input_data])[self.output_layer].squeeze()
        result_mask = predictions.argmax(axis=0).astype('uint8')
        orig_h, orig_w = image.shape[:2]
        return cv2.resize(result_mask, (orig_w, orig_h), interpolation=cv2.INTER_NEAREST)

def assess_risk(segmentation_mask, detection_results):
    H, W = segmentation_mask.shape
    is_risky = 0
    for box in detection_results.boxes:
        if int(box.cls[0]) != PERSON_CLASS_ID: continue
        x1, y1, x2, y2 = map(int, box.xyxy[0])
        p_center_x = max(0, min(int((x1 + x2) / 2), W - 1))
        p_center_y = max(0, min(y2, H - 1))
        if segmentation_mask[p_center_y, p_center_x] == ROAD_CLASS_ID:
            is_risky = 1
            break
    return is_risky

def evaluate_system(seg_model, yolo_model, data_df):
    y_true, y_pred = [], []
    for index, row in data_df.iterrows():
        img_path = IMAGE_DIR / row['filename']
        image = cv2.imread(str(img_path))
        if image is None: continue
        detection_results = yolo_model(image, verbose=False, device='cpu', classes=[PERSON_CLASS_ID])[0]
        if seg_model:
            segmentation_mask = seg_model.predict(image)
            predicted_label = assess_risk(segmentation_mask, detection_results)
        else:
            predicted_label = 1 if len(detection_results.boxes) > 0 else 0
        y_true.append(row['label'])
        y_pred.append(predicted_label)
    return y_true, y_pred

def calculate_metrics(y_true, y_pred):
    return (accuracy_score(y_true, y_pred),
            precision_score(y_true, y_pred, pos_label=1, zero_division=0),
            recall_score(y_true, y_pred, pos_label=1, zero_division=0),
            f1_score(y_true, y_pred, pos_label=1, zero_division=0))

if __name__ == '__main__':
    if not LABEL_CSV.exists():
        print(f"Error: Label CSV not found at {LABEL_CSV}.")
        exit()
    data_df = pd.read_csv(LABEL_CSV)
    yolo_model = YOLO(YOLO_MODEL_PATH)
    
    pid = os.getpid()
    cpu_cores = psutil.cpu_count()
    print(f"Total CPU Cores: {cpu_cores}")
    monitor = ResourceMonitor(pid, cpu_cores)
    
    results = {}

    # 1. Baseline Evaluation
    print("Evaluating Baseline (YOLO Only)...")
    monitor.start()
    y_true, y_pred_baseline = evaluate_system(None, yolo_model, data_df)
    monitor.stop()
    acc_metrics = calculate_metrics(y_true, y_pred_baseline)
    res_metrics = monitor.get_results()
    results["YOLOv8 Only (Baseline)"] = {'acc': acc_metrics, 'res': res_metrics}

    # 2. Integrated Systems
    for name, path in SEG_MODELS.items():
        print(f"\nEvaluating {name} System...")
        if not path.exists():
            print(f"  Skipping {name}: Model file not found at '{path}'")
            continue
        try:
            seg_model = SegmentationModel(path)
            monitor.start()
            y_true_sys, y_pred_sys = evaluate_system(seg_model, yolo_model, data_df)
            monitor.stop()
            min_len = min(len(y_true), len(y_pred_sys))
            acc_metrics_sys = calculate_metrics(y_true[:min_len], y_pred_sys[:min_len])
            res_metrics_sys = monitor.get_results()
            results[f"{name} Seg + YOLOv8"] = {'acc': acc_metrics_sys, 'res': res_metrics_sys}
        except Exception as e:
            print(f"  Error evaluating {name} system: {e}")

    # <<< 수정: 결과 출력 테이블 헤더 및 내용 변경 >>>
    print("\n--- Final Results (Table 2 Data with Resource Usage) ---")
    header = (f"{'System Configuration':<30} | {'Accuracy':>8} | {'Precision':>9} | {'Recall':>6} | {'F1-Score':>8} | "
              f"{'Avg CPU (%)':>12} | {'Peak CPU (%)':>13} | {'Avg Mem (MB)':>14}")
    print(header)
    print("-" * len(header))
    
    for name, data in results.items():
        if data:
            acc, prec, rec, f1 = data['acc']
            avg_cpu, peak_cpu, avg_mem = data['res'] # 변수 이름 변경
            print(f"{name:<30} | {acc:>8.3f} | {prec:>9.3f} | {rec:>6.3f} | {f1:>8.3f} | "
                  f"{avg_cpu:>12.2f} | {peak_cpu:>13.2f} | {avg_mem:>14.2f}")