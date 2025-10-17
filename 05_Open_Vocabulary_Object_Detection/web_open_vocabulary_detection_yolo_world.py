from flask import Flask, render_template_string, Response, request, jsonify
from ultralytics import YOLOWorld
import torch
import cv2
import time
import os
import threading

# =================================================================
# 초기값 설정
# =================================================================
script_dir = os.path.dirname(os.path.abspath(__file__))
Models_dir = os.path.abspath(os.path.join(script_dir, "../00_Models"))

# 모델 파일 경로를 전역 변수로 저장
model_file = os.path.join(Models_dir, "yolov8x-worldv2.pt")

app = Flask(__name__)

# GPU가 사용 가능한지 확인
device = "cuda" if torch.cuda.is_available() else "cpu"
print(f"Using device: {device}")


# ★★★ 모델 객체 자체를 교체하기 때문에, 초기 모델 로드는 함수로 만듭니다 ★★★
def load_model_with_classes(classes):
    """지정된 클래스로 새 모델 객체를 로드하고 반환하는 함수"""
    m = YOLOWorld(model_file)
    m.to(device)
    m.set_classes(classes)
    print(f"New model instance loaded for classes: {classes}")
    return m


# 초기 모델 로드
desired_classes = ["__placeholder__"]
model = load_model_with_classes(desired_classes)

# 모델 접근을 제어할 Lock 객체 생성
model_lock = threading.Lock()


def generate_frames():
    """웹캠 프레임을 생성하는 제너레이터 함수"""
    # 이 함수가 실행될 때의 desired_classes를 복사해서 사용
    current_detection_classes = desired_classes[:]
    print(
        f"Video stream started. Detecting: {current_detection_classes if current_detection_classes != ['__placeholder__'] else 'Nothing'}"
    )

    cap = cv2.VideoCapture(0)
    if not cap.isOpened():
        print("웹캠을 열 수 없습니다.")
        return

    prev_time = 0

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        with model_lock:
            # 현재 스레드의 모델을 사용하여 예측
            results = model.predict(frame, device=device, verbose=False)

        annotated_frame = results[0].plot()

        current_time = time.time()
        fps = 1 / (current_time - prev_time) if prev_time != 0 else 0
        prev_time = current_time
        cv2.putText(
            annotated_frame,
            f"FPS: {fps:.2f}",
            (10, 30),
            cv2.FONT_HERSHEY_SIMPLEX,
            1,
            (0, 255, 0),
            2,
        )

        ret, buffer = cv2.imencode(".jpg", annotated_frame)
        if not ret:
            continue

        frame_bytes = buffer.tobytes()
        yield (
            b"--frame\r\n" b"Content-Type: image/jpeg\r\n\r\n" + frame_bytes + b"\r\n"
        )

    cap.release()
    print("Video stream stopped.")


@app.route("/")
def index():
    # HTML 및 JavaScript 부분은 변경할 필요가 없습니다.
    HTML_TEMPLATE = """
    <!DOCTYPE html>
    <html lang="ko">
    <head>
        <meta charset="UTF-8">
        <title>Real Time Open-Vocabulary Object Detection(YOLO-World)</title>
        <style>
            body { font-family: sans-serif; text-align: center; margin: 0; background-color: #f4f4f4; }
            h1 { background-color: #007BFF; color: white; padding: 20px; margin: 0; }
            .container { padding: 20px; }
            form { margin: 20px auto; padding: 20px; background-color: white; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); display: inline-block; }
            input[type="text"] { width: 300px; padding: 10px; border-radius: 4px; border: 1px solid #ddd; }
            input[type="submit"] { padding: 10px 20px; background-color: #28a745; color: white; border: none; border-radius: 4px; cursor: pointer; }
            input[type="submit"]:hover { background-color: #218838; }
            img { margin-top: 20px; border: 2px solid #ddd; border-radius: 8px; max-width: 80%; }
        </style>
        <script src="https://code.jquery.com/jquery-3.6.0.min.js"></script>
        <script type="text/javascript">
            $(document).ready(function(){
                $('#detect-form').on('submit', function(event){
                    event.preventDefault();
                    $.ajax({
                        url: '/update_classes',
                        type: 'POST',
                        data: $(this).serialize(),
                        success: function(data) {
                            console.log("Classes updated to:", data.classes);
                            // 스트림을 재시작하여 새 모델을 사용하는 generate_frames가 호출되게 함
                            $('#video-feed').attr('src', '/video_feed?' + new Date().getTime());
                        },
                        error: function() {
                            alert("Error updating classes.");
                        }
                    });
                });
            });
        </script>
    </head>
    <body>
        <h1>Real Time Open-Vocabulary Object Detection(YOLO-World)</h1>
        <div class="container">
            <form id="detect-form">
                <label for="desired_classes">탐지할 객체 입력 (쉼표로 구분):</label><br><br>
                <input type="text" id="desired_classes" name="desired_classes" placeholder="e.g. person, car, cup" required>
                <input type="submit" value="적용 및 스트림 재시작">
            </form>
            <img id="video-feed" src="{{ url_for('video_feed') }}" alt="Camera Feed">
        </div>
    </body>
    </html>
    """
    return render_template_string(HTML_TEMPLATE)


@app.route("/video_feed")
def video_feed():
    return Response(
        generate_frames(), mimetype="multipart/x-mixed-replace; boundary=frame"
    )


@app.route("/update_classes", methods=["POST"])
def update_classes():
    # ★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
    #               오류 해결을 위한 핵심 수정 로직
    # ★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★★
    global model, desired_classes

    desired_classes_str = request.form.get("desired_classes", "")
    new_classes = [cls.strip() for cls in desired_classes_str.split(",") if cls.strip()]

    if not new_classes:
        target_classes = ["__placeholder__"]
    else:
        target_classes = new_classes

    # 새 클래스로 새 모델 객체를 생성
    new_model = load_model_with_classes(target_classes)

    # Lock을 잡고 전역 모델 객체를 새 모델로 교체
    with model_lock:
        model = new_model
        desired_classes = target_classes

    return jsonify({"status": "success", "classes": desired_classes})


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True, threaded=True)
