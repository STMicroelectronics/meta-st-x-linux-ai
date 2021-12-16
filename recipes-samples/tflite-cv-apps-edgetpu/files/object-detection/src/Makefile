OPENCV_PKGCONFIG?="opencv4"
SYSROOT?=""
TARGET_BIN = objdetect_tfl_edgetpu_gst_gtk

CXXFLAGS += -Wall $(shell pkg-config --cflags gtk+-3.0 $(OPENCV_PKGCONFIG) gstreamer-plugins-base-1.0 gstreamer-wayland-1.0)
CXXFLAGS += -std=c++17
CXXFLAGS += -I$(SYSROOT)/usr/include/tensorflow/lite/tools/make/downloads/flatbuffers/include
CXXFLAGS += -I$(SYSROOT)/usr/include/rapidjson

LDFLAGS  = $(shell pkg-config --libs gtk+-3.0 gstreamer-plugins-base-1.0 gstreamer-wayland-1.0)
LDFLAGS += -lpthread -ldl -lopencv_core -lopencv_imgproc -lopencv_imgcodecs
LDFLAGS += -l:libedgetpu.so -Wl,--whole-archive -ltensorflow-lite  -Wl,--no-whole-archive

SRCS = objdetect_tfl_edgetpu_gst_gtk.cc
OBJS = $(SRCS:.cc=.o)

all: $(TARGET_BIN)

$(TARGET_BIN): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

$(OBJS): $(SRCS)
	$(CXX) $(CXXFLAGS) -c $^

clean:
	rm -rf $(OBJS) $(TARGET_BIN)
