PACKAGE_INSTALL += "\
    tensorflow-lite-tools \
    tflite-edgetpu-benchmark \
    arm-compute-library-tools \
    armnn-tools \
    armnn-tensorflow-lite-examples \
    tflite-cv-apps-image-classification-c++ \
    tflite-cv-apps-image-classification-python \
    tflite-cv-apps-object-detection-c++ \
    tflite-cv-apps-object-detection-python \
    tflite-cv-apps-edgetpu-image-classification-c++ \
    tflite-cv-apps-edgetpu-image-classification-python \
    tflite-cv-apps-edgetpu-object-detection-c++ \
    tflite-cv-apps-edgetpu-object-detection-python \
    armnn-tfl-cv-apps-image-classification-c++ \
    armnn-tfl-cv-apps-object-detection-c++ \
"

#Add face recognition application for internal usage and package generation
#DO NOT DELIVERED OUTSIDE
PACKAGE_INSTALL += "\
    tflite-cv-apps-face-recognition-c++ \
"
