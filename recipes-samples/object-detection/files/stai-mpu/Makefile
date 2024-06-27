OPENCV_PKGCONFIG?="opencv4"
SYSROOT?=""
ARCHITECTURE?=""
TARGET_BIN = stai_mpu_object_detection

CXXFLAGS += -Wall $(shell pkg-config --cflags gtk+-3.0 $(OPENCV_PKGCONFIG) gstreamer-plugins-base-1.0 gstreamer-wayland-1.0)
CXXFLAGS += -std=c++17 -O3
CXXFLAGS += -I$(SYSROOT)/usr/include/stai_mpu/
CXXFLAGS += -I$(SYSROOT)/usr/include/rapidjson

LDFLAGS  = $(shell pkg-config --libs gtk+-3.0 gstreamer-plugins-base-1.0 gstreamer-wayland-1.0)
LDFLAGS += -lpthread -lopencv_core -lopencv_imgproc -lopencv_imgcodecs
LDFLAGS += -lstai_mpu -ldl

SRCS = stai_mpu_object_detection.cc
OBJS = $(SRCS:.cc=.o)

all: $(TARGET_BIN)

$(TARGET_BIN): $(OBJS)
	$(CXX)  -o $@ $^ $(LDFLAGS)

$(OBJS): $(SRCS)
	$(CXX) $(CXXFLAGS) -c $^

clean:
	rm -rf $(OBJS) $(TARGET_BIN)
