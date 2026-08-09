import cv2 as cv
import numpy as np

def calibrate_from_full_on(frame):
    # 1. Threshold for red/orange LEDs
    hsv = cv.cvtColor(frame, cv.COLOR_BGR2HSV)
    mask1 = cv.inRange(hsv, (0, 120, 160), (10, 255, 255))
    mask2 = cv.inRange(hsv, (170,120,160), (179,255,255))
    mask = cv.bitwise_or(mask1, mask2)

    # 2. Find the biggest red contour (the LED arc)
    contours, _ = cv.findContours(mask, cv.RETR_EXTERNAL, cv.CHAIN_APPROX_SIMPLE)
    cnt = max(contours, key=cv.contourArea)

    # 3. Fit a minimum enclosing circle for center/radius
    (cx, cy), radius = cv.minEnclosingCircle(cnt)
    cx, cy = int(cx), int(cy)
    radius = int(radius)

    # 4. Get angles of all LED pixels to find start/end
    ys, xs = np.nonzero(mask)
    thetas = np.arctan2(ys - cy, xs - cx)
    theta_min, theta_max = np.percentile(thetas, [1, 99])  # discard outliers

    # 5. Optionally compute inner/outer radii via distance histogram
    radii = np.sqrt((xs - cx)**2 + (ys - cy)**2)
    r1, r2 = np.percentile(radii, [5, 95])

    calib = {
        "cx": cx, "cy": cy,
        "r1": float(r1), "r2": float(r2),
        "theta0": float(theta_min),
        "theta1": float(theta_max)
    }
    return calib, mask
