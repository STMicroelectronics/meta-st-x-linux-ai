SYSROOT?=""
TARGET_BIN = FaceRecognition

CXXFLAGS += -Wall $(shell pkg-config --cflags gtk+-3.0 opencv4 cairo gstreamer-plugins-base-1.0 glesv2) -I${SYSROOT}/usr/include/tensorflow/lite/tools/make/downloads/flatbuffers/include -I${SYSROOT}/usr/include/tensorflow/lite/tools/make/downloads/absl
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 opencv4 cairo gstreamer-plugins-base-1.0 glesv2) -lpthread -Wl,--whole-archive -ltensorflow-lite  -Wl,--no-whole-archive

SRCS = face_recognition.cpp face_detect_helpers.cpp
OBJS = $(SRCS:.cpp=.o)

all: ${TARGET_BIN}

$(TARGET_BIN): $(OBJS)
	$(CXX) ${LDFLAGS} -o $@ $^

$(OBJS): $(SRCS)
	$(CXX) $(CXXFLAGS) -c $^

clean:
	rm -rf $(OBJS) ${TARGET_BIN}
