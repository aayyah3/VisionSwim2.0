import cv2
import numpy as np
  
# read image
src = cv2.imread('test-img.jpg', cv2.IMREAD_UNCHANGED)
 
# gaussian blur
blurred = cv2.GaussianBlur(src, (9, 9), cv2.BORDER_DEFAULT)

# convert to grayscale
grayscale = cv2.cvtColor(blurred, cv2.COLOR_BGR2GRAY)

# adjust colors to be black or white depending on average brightness
ret, img = cv2.threshold(grayscale , cv2.mean(grayscale)[0], 255, cv2.THRESH_BINARY)

#save image
cv2.imwrite("output.jpg", img)

cv2.imshow("Output", img)
cv2.waitKey(0)
cv2.destroyAllWindows()