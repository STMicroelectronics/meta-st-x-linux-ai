SYSROOT?=""
TARGET_BIN = objdetect_tfl_edgetpu_gst_gtk

CXXFLAGS += -Wall $(shell pkg-config --cflags gtk+-3.0 opencv gstreamer-plugins-base-1.0) -I${SYSROOT}/usr/include/tensorflow/lite/tools/make/downloads/flatbuffers/include -L${SYSROOT}/usr/lib
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 opencv cairo gstreamer-plugins-base-1.0) -lpthread -l:libedgetpu.so -Wl,--whole-archive -ltensorflow-lite -Wl,--no-whole-archive

SRCS = objdetect_tfl_edgetpu_gst_gtk.cc wrapper_tfl_edgetpu.cc
OBJS = $(SRCS:.cc=.o)

all: ${TARGET_BIN}

$(TARGET_BIN): $(OBJS)
	$(CXX) ${LDFLAGS} -o $@ $^

$(OBJS): $(SRCS)
	$(CXX) $(CXXFLAGS) -c $^

clean:
	rm -rf $(OBJS) ${TARGET_BIN}
