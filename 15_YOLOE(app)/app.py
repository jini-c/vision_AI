import torch
import numpy as np
import gradio as gr
import supervision as sv
from scipy.ndimage import binary_fill_holes
from ultralytics import YOLOE
from ultralytics.utils.torch_utils import smart_inference_mode
from ultralytics.models.yolo.yoloe.predict_vp import YOLOEVPSegPredictor
from gradio_image_prompter import ImagePrompter
from huggingface_hub import hf_hub_download


# YOLOE 모델을 초기화하는 함수
def init_model(model_id, is_pf=False):
    # 프롬프트 없는 모드 여부에 따라 파일 이름 결정
    filename = f"{model_id}-seg.pt" if not is_pf else f"{model_id}-seg-pf.pt"
    # Hugging Face Hub에서 모델 가중치 다운로드
    path = hf_hub_download(repo_id="jameslahm/yoloe", filename=filename)
    # 다운로드한 경로에서 YOLOE 모델 로드
    model = YOLOE(path)
    # 모델을 평가 모드로 설정
    model.eval()
    # GPU가 사용 가능하면 GPU로, 아니면 CPU로 이동
    model.to("cuda" if torch.cuda.is_available() else "cpu")
    return model

# 최적화를 위한 스마트 추론 모드 데코레이터가 적용된 추론 함수
@smart_inference_mode()
def yoloe_inference(image, prompts, target_image, model_id, image_size, conf_thresh, iou_thresh, prompt_type):
    # 제공된 모델 ID로 모델 초기화
    model = init_model(model_id)
    # 모델에 전달할 추가 인자를 담을 딕셔너리
    kwargs = {}
    
    # 프롬프트 유형에 따라 처리
    if prompt_type == "Text":
        # 텍스트 프롬프트 추출
        texts = prompts["texts"]
        # 텍스트 프롬프트 기반으로 탐지 클래스 설정
        model.set_classes(texts, model.get_text_pe(texts))
    elif prompt_type == "Visual":
        # 시각 프롬프트를 위한 kwargs 설정
        kwargs = dict(
            prompts=prompts,
            predictor=YOLOEVPSegPredictor
        )
        if target_image:
            # 시각 프롬프트 임베딩(VPE)으로 예측 수행 후 클래스 업데이트
            model.predict(source=image, imgsz=image_size, conf=conf_thresh, iou=iou_thresh, return_vpe=True, **kwargs)
            model.set_classes(["object0"], model.predictor.vpe)
            model.predictor = None  # 예측기 초기화
            image = target_image  # 다음 처리를 위해 타겟 이미지로 전환
            kwargs = {}  # 다음 예측을 위해 kwargs 비우기
    elif prompt_type == "Prompt-free":
        # 프롬프트 없는 모드를 위한 어휘 추출
        vocab = model.get_vocab(prompts["texts"])
        # 프롬프트 없는 모드로 모델 재초기화
        model = init_model(model_id, is_pf=True)
        # 어휘와 클래스 이름 설정
        model.set_vocab(vocab, names=prompts["texts"])
        # 프롬프트 없는 탐지를 위한 모델 설정 조정
        model.model.model[-1].is_fused = True
        model.model.model[-1].conf = 0.001  # 신뢰도 임계값 낮춤
        model.model.model[-1].max_det = 1000  # 최대 탐지 수 증가

    # 객체 탐지 및 분할 수행
    results = model.predict(source=image, imgsz=image_size, conf=conf_thresh, iou=iou_thresh, **kwargs)
    # 결과를 supervision Detections 형식으로 변환
    detections = sv.Detections.from_ultralytics(results[0])

    # 이미지 해상도에 따라 최적의 선 굵기와 텍스트 크기 계산
    resolution_wh = image.size
    thickness = sv.calculate_optimal_line_thickness(resolution_wh=resolution_wh)
    text_scale = sv.calculate_optimal_text_scale(resolution_wh=resolution_wh)

    # 클래스 이름과 신뢰도 점수로 레이블 생성
    labels = [
        f"{class_name} {confidence:.2f}"
        for class_name, confidence
        in zip(detections['class_name'], detections.confidence)
    ]

    # 이미지에 마스크, 박스, 레이블 주석 추가
    annotated_image = image.copy()
    annotated_image = sv.MaskAnnotator(
        color_lookup=sv.ColorLookup.INDEX,
        opacity=0.4
    ).annotate(scene=annotated_image, detections=detections)
    annotated_image = sv.BoxAnnotator(
        color_lookup=sv.ColorLookup.INDEX,
        thickness=thickness
    ).annotate(scene=annotated_image, detections=detections)
    annotated_image = sv.LabelAnnotator(
        color_lookup=sv.ColorLookup.INDEX,
        text_scale=text_scale,
        smart_position=True
    ).annotate(scene=annotated_image, detections=detections, labels=labels)

    return annotated_image

# Gradio 애플리케이션 인터페이스 정의
def app():
    with gr.Blocks():
        with gr.Row():
            with gr.Column():
                # 이미지와 매개변수 입력 섹션
                with gr.Row():
                    # 원본 이미지 입력
                    raw_image = gr.Image(type="pil", label="이미지", visible=True, interactive=True)
                    # 시각 프롬프트를 위한 박스 그리기 입력
                    box_image = ImagePrompter(type="pil", label="박스 그리기", visible=False, interactive=True)
                    # 시각 프롬프트를 위한 마스크 그리기 입력
                    mask_image = gr.ImageEditor(type="pil", label="마스크 그리기", visible=False, interactive=True, layers=False, canvas_size=(640, 640))
                    # 교차 이미지 프롬프트를 위한 타겟 이미지 입력
                    target_image = gr.Image(type="pil", label="타겟 이미지", visible=False, interactive=True)

                # 추론을 실행하는 버튼
                yoloe_infer = gr.Button(value="객체 탐지 및 분할")
                # 프롬프트 유형을 추적하는 숨겨진 텍스트 박스
                prompt_type = gr.Textbox(value="Text", visible=False)

                # 텍스트 기반 프롬프트 탭
                with gr.Tab("Text") as text_tab:
                    texts = gr.Textbox(label="입력 텍스트", value='person,bus', placeholder='person,bus', visible=True, interactive=True)

                # 시각 프롬프트 탭
                with gr.Tab("Visual") as visual_tab:
                    with gr.Row():
                        # 시각 프롬프트 유형 선택 드롭다운 (박스 또는 마스크)
                        visual_prompt_type = gr.Dropdown(choices=["bboxes", "masks"], value="bboxes", label="시각 유형", interactive=True)
                        # 이미지 내/교차 이미지 모드 선택 라디오 버튼
                        visual_usage_type = gr.Radio(choices=["Intra-Image", "Cross-Image"], value="Intra-Image", label="이미지 내/교차 이미지", interactive=True)

                # 프롬프트 없는 모드 탭
                with gr.Tab("Prompt-Free") as prompt_free_tab:
                    gr.HTML(
                        """
                        <p style='text-align: center'>
                        <b>프롬프트 없는 모드가 켜져 있습니다</b>
                        </p>
                    """, show_label=False)

                # 모델 선택 드롭다운
                model_id = gr.Dropdown(
                    label="모델",
                    choices=[
                        "yoloe-v8s", "yoloe-v8m", "yoloe-v8l",
                        "yoloe-11s", "yoloe-11m", "yoloe-11l",
                    ],
                    value="yoloe-v8l",
                )
                # 이미지 크기 슬라이더
                image_size = gr.Slider(label="이미지 크기", minimum=320, maximum=1280, step=32, value=640)
                # 신뢰도 임계값 슬라이더
                conf_thresh = gr.Slider(label="신뢰도 임계값", minimum=0.0, maximum=1.0, step=0.05, value=0.25)
                # IoU 임계값 슬라이더
                iou_thresh = gr.Slider(label="IoU 임계값", minimum=0.0, maximum=1.0, step=0.05, value=0.70)

            with gr.Column():
                # 주석이 추가된 출력 이미지 섹션
                output_image = gr.Image(type="numpy", label="주석이 추가된 이미지", visible=True)

        # 선택된 탭에 따라 가시성 업데이트 함수
        def update_text_image_visibility():
            return gr.update(value="Text"), gr.update(visible=True), gr.update(visible=False), gr.update(visible=False), gr.update(visible=False)

        def update_visual_image_visiblity(visual_prompt_type, visual_usage_type):
            if visual_prompt_type == "bboxes":
                return gr.update(value="Visual"), gr.update(visible=False), gr.update(visible=True), gr.update(visible=False), gr.update(visible=(visual_usage_type == "Cross-Image"))
            elif visual_prompt_type == "masks":
                return gr.update(value="Visual"), gr.update(visible=False), gr.update(visible=False), gr.update(visible=True), gr.update(visible=(visual_usage_type == "Cross-Image"))

        def update_pf_image_visibility():
            return gr.update(value="Prompt-free"), gr.update(visible=True), gr.update(visible=False), gr.update(visible=False), gr.update(visible=False)

        # 탭 선택에 가시성 업데이트 함수 연결
        text_tab.select(fn=update_text_image_visibility, inputs=None, outputs=[prompt_type, raw_image, box_image, mask_image, target_image])
        visual_tab.select(fn=update_visual_image_visiblity, inputs=[visual_prompt_type, visual_usage_type], outputs=[prompt_type, raw_image, box_image, mask_image, target_image])
        prompt_free_tab.select(fn=update_pf_image_visibility, inputs=None, outputs=[prompt_type, raw_image, box_image, mask_image, target_image])

        # 시각 프롬프트 설정에 따라 가시성 업데이트 함수
        def update_visual_prompt_type(visual_prompt_type):
            if visual_prompt_type == "bboxes":
                return gr.update(visible=True), gr.update(visible=False)
            if visual_prompt_type == "masks":
                return gr.update(visible=False), gr.update(visible=True)
            return gr.update(visible=False), gr.update(visible=False)

        def update_visual_usage_type(visual_usage_type):
            if visual_usage_type == "Intra-Image":
                return gr.update(visible=False)
            if visual_usage_type == "Cross-Image":
                return gr.update(visible=True)
            return gr.update(visible=False)

        # 드롭다운 및 라디오 변경에 가시성 업데이트 연결
        visual_prompt_type.change(fn=update_visual_prompt_type, inputs=[visual_prompt_type], outputs=[box_image, mask_image])
        visual_usage_type.change(fn=update_visual_usage_type, inputs=[visual_usage_type], outputs=[target_image])

        # 입력을 처리하고 출력을 생성하는 주요 추론 함수
        def run_inference(raw_image, box_image, mask_image, target_image, texts, model_id, image_size, conf_thresh, iou_thresh, prompt_type, visual_prompt_type, visual_usage_type):
            # 텍스트 또는 프롬프트 없는 프롬프트 처리
            if prompt_type == "Text" or prompt_type == "Prompt-free":
                target_image = None
                image = raw_image
                if prompt_type == "Prompt-free":
                    # 프롬프트 없는 모드를 위한 기본 태그 로드
                    with open('tools/ram_tag_list.txt', 'r') as f:
                        texts = [x.strip() for x in f.readlines()]
                else:
                    # 쉼표로 구분된 텍스트 입력 파싱
                    texts = [text.strip() for text in texts.split(',')]
                prompts = {"texts": texts}
            # 시각 프롬프트 처리
            elif prompt_type == "Visual":
                if visual_usage_type != "Cross-Image":
                    target_image = None
                if visual_prompt_type == "bboxes":
                    image, points = box_image["image"], box_image["points"]
                    points = np.array(points)
                    if len(points) == 0:
                        gr.Warning("박스가 제공되지 않았습니다. 이미지 출력이 없습니다.", visible=True)
                        return gr.update(value=None)
                    # 바운딩 박스 좌표 추출
                    bboxes = np.array([p[[0, 1, 3, 4]] for p in points if p[2] == 2])
                    prompts = {"bboxes": bboxes, "cls": np.array([0] * len(bboxes))}
                elif visual_prompt_type == "masks":
                    image, masks = mask_image["background"], mask_image["layers"][0]
                    masks = np.array(masks.convert("L"))
                    # 마스크의 구멍 채우기 및 이진화
                    masks = binary_fill_holes(masks).astype(np.uint8)
                    masks[masks > 0] = 1
                    if masks.sum() == 0:
                        gr.Warning("마스크가 제공되지 않았습니다. 이미지 출력이 없습니다.", visible=True)
                        return gr.update(value=None)
                    prompts = {"masks": masks[None], "cls": np.array([0])}
            # 준비된 입력으로 추론 실행
            return yoloe_inference(image, prompts, target_image, model_id, image_size, conf_thresh, iou_thresh, prompt_type)

        # 버튼 클릭에 추론 함수 연결
        yoloe_infer.click(
            fn=run_inference,
            inputs=[raw_image, box_image, mask_image, target_image, texts, model_id, image_size, conf_thresh, iou_thresh, prompt_type, visual_prompt_type, visual_usage_type],
            outputs=[output_image],
        )

# Gradio 앱 생성 및 설정
gradio_app = gr.Blocks()
with gradio_app:
    # 로고와 제목이 포함된 헤더 추가
    gr.HTML(
        """
    <h1 style='text-align: center'>
    
    YOLOE(ye): Real-Time Seeing Anything
    </h1>
    """)
    # arXiv와 GitHub 링크 추가
    gr.HTML(
        """
        <h3 style='text-align: center'>
        <a href='https://arxiv.org/abs/2503.07465' target='_blank'>arXiv</a> | <a href='https://github.com/THU-MIG/yoloe' target='_blank'>github</a>
        </h3>
        """)
    
    with gr.Row():
        with gr.Column():
            app()  # 앱 인터페이스 삽입

# 메인 스크립트로 실행 시 Gradio 앱 실행
if __name__ == '__main__':
    gradio_app.launch(allowed_paths=["figures"])