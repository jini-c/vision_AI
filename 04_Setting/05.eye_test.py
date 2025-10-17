from imutils import face_utils
from datetime import datetime
import numpy as np
import argparse
import imutils
import time
import dlib
import cv2
import os
from PIL import Image, ImageFont, ImageDraw
import pygame
import threading
import time
#from class_naver_sms_service import naver_sms_sender

#=================================================================
#초기값 설정
#=================================================================
#실행 경로 설정 
# 경로 설정
script_dir = os.path.dirname(os.path.abspath(__file__))
os.chdir(script_dir)
Models_dir = os.path.abspath(os.path.join(script_dir, "../00_Models"))
Videos_dir = os.path.abspath(os.path.join(script_dir, "../00_Sample_Video"))
Dataset_dir = os.path.abspath(os.path.join(script_dir, "../00_Dataset"))
video_file = os.path.join(Videos_dir, "input","driver.mp4" )
dataset_file = os.path.join(Dataset_dir, "shape_predictor_68_face_landmarks.dat" )


#=================================================================
#얼굴 감지, 눈동자 감지 처리 
#=================================================================
#face detecor pre trained NN 
detector = dlib.get_frontal_face_detector()

#dlib's facial landmark NN 초기화
predictor = dlib.shape_predictor(dataset_file)

# 오른쪽, 왼쪽 눈 좌표 인덱스 
(lStart, lEnd) = face_utils.FACIAL_LANDMARKS_IDXS["left_eye"]
(rStart, rEnd) = face_utils.FACIAL_LANDMARKS_IDXS["right_eye"]

cap = cv2.VideoCapture(0)
#cap = cv2.VideoCapture(video_file)

if not cap.isOpened():
	print("Error: 비디오 파일을 열 수 없습니다.")
	exit()
time.sleep(2.0)

while True:
	#웹캠 영상 읽기
	ret, frame = cap.read()
	if not ret:
		print("비디오가 끝났거나 에러가 발생했습니다.")
		break

	frame = imutils.resize(frame, width=720)
	frame = cv2.flip(frame, 1)

	#입력영상 graysale 처리 
	gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

	# 얼굴 Detection 
	rects = detector(gray)	
   
	for rect in rects:
		x, y = rect.left(), rect.top()
		w, h = rect.right() - x, rect.bottom() - y
		
		#print( w, h)
		#얼굴 크기가 110이상일때만 눈동자 Detection
		#if (w > 110 ):
		
		#=================================================================
		#68개 점 찾기
		#=================================================================
		
		#얼굴 영역 bounding box 그리기 (선택사항)
		# cv2.rectangle(frame, (x, y), (x + w, y + h), (255, 0, 0), 2)

		#얼굴 68개 랜드마크 Detection
		shape = predictor(gray, rect)
		shape = face_utils.shape_to_np(shape)
		
		# 68개 랜드마크 점들을 연결해서 얼굴 형태 그리기
		
		# 얼굴 윤곽선 (0-16)
		jaw_points = shape[0:17]
		cv2.polylines(frame, [jaw_points], False, (255, 255, 0), 2)
		
		# 오른쪽 눈썹 (17-21)
		right_eyebrow = shape[17:22]
		cv2.polylines(frame, [right_eyebrow], False, (255, 0, 255), 2)
		
		# 왼쪽 눈썹 (22-26)
		left_eyebrow = shape[22:27]
		cv2.polylines(frame, [left_eyebrow], False, (255, 0, 255), 2)
		
		# 코 중앙선 (27-30)
		nose_bridge = shape[27:31]
		cv2.polylines(frame, [nose_bridge], False, (0, 255, 255), 2)
		
		# 코 하단 (31-35)
		nose_tip = shape[31:36]
		cv2.polylines(frame, [nose_tip], True, (0, 255, 255), 2)
		
		# 오른쪽 눈 (36-41)
		right_eye = shape[36:42]
		cv2.polylines(frame, [right_eye], True, (0, 255, 0), 2)
		
		# 왼쪽 눈 (42-47)
		left_eye = shape[42:48]
		cv2.polylines(frame, [left_eye], True, (0, 255, 0), 2)
		
		# 입술 외곽선 (48-59)
		outer_lip = shape[48:60]
		cv2.polylines(frame, [outer_lip], True, (0, 0, 255), 2)
		
		# 입술 내곽선 (60-67)
		inner_lip = shape[60:68]
		cv2.polylines(frame, [inner_lip], True, (255, 0, 0), 2)
		
		# 68개 랜드마크 점도 함께 표시 (선택사항)
		for i, (x_point, y_point) in enumerate(shape):
			cv2.circle(frame, (x_point, y_point), 1, (255, 255, 255), -1)
		
		# 기존 눈동자 관련 코드들 (주석처리)
		#왼쪽 및 오른쪽 눈 좌표를 추출한 다음 좌표를 사용하여 양쪽 눈의 눈 종횡비를 계산
		# leftEye = shape[lStart:lEnd]
		# rightEye = shape[rStart:rEnd]
		
		#print( ear_avg )
		# 양쪽 눈동자 외각선 찾기
		# leftEyeHull = cv2.convexHull(leftEye)
		# rightEyeHull = cv2.convexHull(rightEye)

		#양쪽 눈동자 녹색 외각선 그리기
		# cv2.drawContours(frame, [leftEyeHull], 0, (0, 0, 255), 1)
		# cv2.drawContours(frame, [rightEyeHull], 0, (0, 0, 255), 1)

		#양쪽 눈동자 녹색 점 그리기
		# cv2.circle(frame, tuple(rightEye[[0,0][0]]), 2, (0,255,0), -1)
		# cv2.circle(frame, tuple(rightEye[[1,0][0]]), 2, (0,255,0), -1)
		# cv2.circle(frame, tuple(rightEye[[2,0][0]]), 2, (0,0,0), -1)
		# cv2.circle(frame, tuple(rightEye[[3,0][0]]), 2, (0,0,0), -1)
		# cv2.circle(frame, tuple(rightEye[[4,0][0]]), 2, (0,0,0), -1)
		# cv2.circle(frame, tuple(rightEye[[5,0][0]]), 2, (0,0,0), -1)   
		
		# cv2.circle(frame, tuple(leftEye[[0,0][0]]), 2, (0,255,0), -1)
		# cv2.circle(frame, tuple(leftEye[[1,0][0]]), 2, (0,255,0), -1)
		# cv2.circle(frame, tuple(leftEye[[2,0][0]]), 2, (0,0,0), -1)
		# cv2.circle(frame, tuple(leftEye[[3,0][0]]), 2, (0,0,0), -1)
		# cv2.circle(frame, tuple(leftEye[[4,0][0]]), 2, (0,0,0), -1)
		# cv2.circle(frame, tuple(leftEye[[5,0][0]]), 2, (0,0,0), -1)
	
	cv2.imshow("Frame", frame)
	key = cv2.waitKey(1) & 0xFF
	if key == ord("q"):
		break

cap.release()
cv2.destroyAllWindows()