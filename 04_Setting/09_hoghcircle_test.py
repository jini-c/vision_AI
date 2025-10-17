import cv2
import numpy as np
import os
import math

# ============================================
# 1) 경로 설정
# ============================================
try:
    script_dir = os.path.dirname(os.path.abspath(__file__))
except NameError:
    script_dir = os.getcwd()

os.chdir(script_dir)
Videos_dir = os.path.abspath(os.path.join(script_dir, "../00_Sample_Video"))
image_path = os.path.join(Videos_dir, "input", "apple.png")

# ============================================
# 2) 이미지 로드 & 리사이즈
# ============================================
img = cv2.imread(image_path)
if img is None:
    raise FileNotFoundError(f"이미지를 불러올 수 없습니다: {image_path}")

h, w = img.shape[:2]
max_side = max(h, w)
scale = 1200 / max_side if max_side > 1400 else 1.0
if scale < 1.0:
    img = cv2.resize(img, (int(w*scale), int(h*scale)), interpolation=cv2.INTER_AREA)
    h, w = img.shape[:2]

out = img.copy()

# ============================================
# 3) 빨강 색상 마스크 (이전과 동일)
# ============================================
hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)

lower_red1 = np.array([0,   60,  80])
upper_red1 = np.array([10, 255, 255])
lower_red2 = np.array([170, 60,  80])
upper_red2 = np.array([180, 255, 255])

mask_r1 = cv2.inRange(hsv, lower_red1, upper_red1)
mask_r2 = cv2.inRange(hsv, lower_red2, upper_red2)
mask_red = cv2.bitwise_or(mask_r1, mask_r2)

kernel = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5,5))
mask_red = cv2.morphologyEx(mask_red, cv2.MORPH_OPEN, kernel, iterations=2)
mask_red = cv2.morphologyEx(mask_red, cv2.MORPH_CLOSE, kernel, iterations=2)

# 작은 컴포넌트 제거
contours, _ = cv2.findContours(mask_red, cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)
min_area = (max(h, w) * 0.01) ** 2
mask_filtered = np.zeros_like(mask_red)
for contour in contours:
    if cv2.contourArea(contour) > min_area:
        cv2.fillPoly(mask_filtered, [contour], 255)
mask_red = mask_filtered

# ============================================
# 4) 전처리
# ============================================
roi = cv2.bitwise_and(img, img, mask=mask_red)
gray = cv2.cvtColor(roi, cv2.COLOR_BGR2GRAY)
gray = cv2.GaussianBlur(gray, (5,5), 0)

# ============================================
# 5) 파라미터 설정
# ============================================
max_dim = max(h, w)
minR = max(20, int(max_dim * 0.04))
maxR = int(max_dim * 0.15)
minR = min(minR, maxR-10)
minDist = int(max_dim * 0.06)

print(f"파라미터: minR={minR}, maxR={maxR}, minDist={minDist}")

# ============================================
# 6) 허프 원 검출 (이전 성공한 파라미터)
# ============================================
all_circles = []

for param2_try in [35, 30, 25, 20, 15, 12]:
    circles = cv2.HoughCircles(
        gray,
        cv2.HOUGH_GRADIENT,
        dp=1.0,
        minDist=minDist,
        param1=50,
        param2=param2_try,
        minRadius=minR,
        maxRadius=maxR
    )
    if circles is not None:
        all_circles.extend(circles[0])
        if len(all_circles) >= 15:
            break

print(f"총 {len(all_circles)}개의 후보 원 검출")

# ============================================
# 7) 향상된 품질 평가 및 위치 정제
# ============================================
def refine_circle_position(x, y, r, img, mask_red):
    """원의 위치와 크기를 정제하는 함수"""
    # 주변 영역에서 더 정확한 중심과 반지름 찾기
    search_radius = min(20, r//2)
    best_score = -1
    best_pos = (x, y, r)
    
    # 중심점 주변 탐색
    for dx in range(-search_radius, search_radius+1, 3):
        for dy in range(-search_radius, search_radius+1, 3):
            new_x, new_y = x + dx, y + dy
            
            # 경계 확인
            if new_x - r < 0 or new_y - r < 0 or new_x + r >= w or new_y + r >= h:
                continue
                
            # 반지름도 약간 조정해서 테스트
            for dr in [-3, 0, 3]:
                new_r = max(minR, min(maxR, r + dr))
                
                # 새 위치에서 점수 계산
                yy, xx = np.indices((h, w))
                circle_mask = ((xx - new_x)**2 + (yy - new_y)**2) <= (new_r*new_r)
                
                if np.sum(circle_mask) == 0:
                    continue
                    
                # 빨강 영역과의 일치도
                red_overlap = np.sum(mask_red[circle_mask] > 0) / np.sum(circle_mask)
                
                # 경계 선명도 (사과 가장자리)
                edges = cv2.Canny(img, 50, 150)
                edge_score = np.sum(edges[circle_mask] > 0) / (2 * math.pi * new_r)
                
                # 종합 점수 (엣지 정보 더 중시해서 경계가 뚜렷한 큰 원 선호)
                total_score = red_overlap * 0.5 + min(edge_score, 1.0) * 0.5
                
                if total_score > best_score:
                    best_score = total_score
                    best_pos = (new_x, new_y, new_r)
    
    return best_pos, best_score

candidates = []
if all_circles:
    all_circles = np.round(np.array(all_circles)).astype(int)
    
    for (x, y, r) in all_circles:
        if not (r >= minR and r <= maxR):
            continue
        if not (r <= x < w-r and r <= y < h-r):
            continue
        
        # 위치와 크기 정제
        refined_pos, refined_score = refine_circle_position(x, y, r, img, mask_red)
        refined_x, refined_y, refined_r = refined_pos
        
        # 기본 품질 평가
        yy, xx = np.indices((h, w))
        circle_mask = ((xx - refined_x)**2 + (yy - refined_y)**2) <= (refined_r*refined_r)
        area = np.count_nonzero(circle_mask)
        
        if area == 0:
            continue
            
        # 빨강 비율
        red_inside = np.count_nonzero(mask_red[circle_mask] > 0)
        red_ratio = red_inside / float(area)
        
        # 원형도
        circle_perimeter = 2 * math.pi * refined_r
        edges = cv2.Canny(gray, 50, 150)
        edge_inside = np.count_nonzero(edges[circle_mask] > 0)
        circularity = edge_inside / circle_perimeter if circle_perimeter > 0 else 0
        
        # 색상 일관성
        roi_bgr = img[circle_mask]
        if len(roi_bgr) > 0:
            color_std = np.std(roi_bgr, axis=0).mean()
        else:
            color_std = 255
            
        # 완화된 필터링 조건 (위치 정제로 더 정확해졌으므로)
        red_threshold = 0.12
        circularity_threshold = 0.1
        color_std_threshold = 90
        
        if (red_ratio >= red_threshold and 
            circularity >= circularity_threshold and 
            color_std <= color_std_threshold):
            
            # 정제 점수를 반영한 최종 점수
            score = refined_score * 0.4 + red_ratio * 0.3 + circularity * 0.2 + (1 - color_std/255) * 0.1
            candidates.append((refined_x, refined_y, refined_r, red_ratio, circularity, color_std, score))

print(f"필터링 후 {len(candidates)}개의 후보")

# 점수 순으로 정렬
candidates.sort(key=lambda t: t[6], reverse=True)

# 후보 정보 출력
print(f"\n=== 후보 리스트 (점수순) ===")
for i, (x, y, r, red_ratio, circularity, color_std, score) in enumerate(candidates[:8]):
    print(f"후보 {i+1}: 중심({x},{y}), 반지름={r}, 점수={score:.3f}")

# ============================================
# 8) 개선된 NMS - 거리 기반으로 단순하게
# ============================================
final = []
MIN_DISTANCE = max(40, minR * 1.8)  # 최소 거리 (실제 사과 크기 고려)

print(f"\nNMS 시작: 최소거리={MIN_DISTANCE}")

for i, candidate in enumerate(candidates):
    x, y, r = candidate[0], candidate[1], candidate[2]
    
    # 기존 선택된 원들과 거리 체크
    too_close = False
    for existing in final:
        ex_x, ex_y = existing[0], existing[1]
        distance = math.hypot(x - ex_x, y - ex_y)
        
        if distance < MIN_DISTANCE:
            print(f"후보 {i+1} 중심({x},{y}) - 기존 중심({ex_x},{ex_y}) 거리={distance:.1f} < {MIN_DISTANCE} → 제거")
            too_close = True
            break
    
    if not too_close:
        final.append(candidate)
        print(f"후보 {i+1} 중심({x},{y}) 선택됨 (총 {len(final)}개)")
        
        # 4개까지만
        if len(final) >= 4:
            break

# ============================================
# 9) 결과 그리기
# ============================================
if final:
    colors = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0)]
    
    for i, (x, y, r, red_ratio, circularity, color_std, score) in enumerate(final):
        color = colors[i % len(colors)]
        cv2.circle(out, (x, y), r, color, 3)
        cv2.circle(out, (x, y), 2, (0, 255, 255), -1)
        cv2.putText(out, str(i+1), (x-10, y+5), cv2.FONT_HERSHEY_SIMPLEX, 0.8, color, 2)
    
    print(f"\n=== 최종 결과: {len(final)}개의 사과 검출 ===")
    for i, (x, y, r, red_ratio, circularity, color_std, score) in enumerate(final):
        print(f"사과 {i+1}: 중심({x},{y}), 반지름={r}")
        print(f"  빨강비율={red_ratio:.3f}, 원형도={circularity:.3f}, 점수={score:.3f}")
else:
    print("사과를 찾지 못했습니다.")

# ============================================
# 10) 표시
# ============================================
cv2.imshow("Original", img)
cv2.imshow("Red Mask", mask_red)
cv2.imshow("Detected Apples", out)

print(f"\n이미지 크기: {w}x{h}")
print("키를 누르면 종료됩니다...")

cv2.waitKey(0)
cv2.destroyAllWindows()