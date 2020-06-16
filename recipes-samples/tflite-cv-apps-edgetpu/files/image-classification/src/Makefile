SYSROOT?=""
TARGET_BIN = label_tfl_edgetpu_gst_gtk

CXXFLAGS += -Wall $(shell pkg-config --cflags gtk+-3.0 opencv4 cairo gstreamer-plugins-base-1.0) -I${SYSROOT}/usr/include/tensorflow/lite/tools/make/downloads/flatbuffers/include -L${SYSROOT}/usr/lib
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 opencv4 cairo gstreamer-plugins-base-1.0) -lpthread -l:libedgetpu_max.so -Wl,--whole-archive -ltensorflow-lite  -Wl,--no-whole-archive

SRCS = label_tfl_edgetpu_gst_gtk.cc wrapper_tfl_edgetpu.cc
OBJS = $(SRCS:.cc=.o)

all: ${TARGET_BIN}

$(TARGET_BIN): $(OBJS)
	$(CXX) ${LDFLAGS} -o $@ $^

$(OBJS): $(SRCS)
	$(CXX) $(CXXFLAGS) -c $^

clean:
	rm -rf $(OBJS) ${TARGET_BIN}
