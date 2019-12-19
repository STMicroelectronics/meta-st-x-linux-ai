SYSROOT?=""
TARGET_BIN_1 = label_tfl_gst_gtk
TARGET_BIN_2 = objdetect_tfl_gst_gtk

CXXFLAGS += -Wall $(shell pkg-config --cflags gtk+-3.0 opencv gstreamer-plugins-base-1.0 glesv2) -I${SYSROOT}/usr/include/tensorflow/lite/tools/make/downloads/flatbuffers/include
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 opencv gstreamer-plugins-base-1.0 glesv2) -lpthread -Wl,--whole-archive -ltensorflow-lite  -Wl,--no-whole-archive

SRCS_1 = label_tfl_gst_gtk.cc wrapper_tfl.cc
OBJS_1 = $(SRCS_1:.cc=.o)

SRCS_2 = objdetect_tfl_gst_gtk.cc wrapper_tfl.cc
OBJS_2 = $(SRCS_2:.cc=.o)

all: ${TARGET_BIN_1} ${TARGET_BIN_2}

$(TARGET_BIN_1): $(OBJS_1)
	$(CXX) ${LDFLAGS} -o $@ $^

$(OBJS_1): $(SRCS_1)
	$(CXX) $(CXXFLAGS) -c $^

$(TARGET_BIN_2): $(OBJS_2)
	$(CXX) ${LDFLAGS} -o $@ $^

$(OBJS_2): $(SRCS_2)
	$(CXX) $(CXXFLAGS) -c $^


clean:
	rm -rf $(OBJS_1) ${TARGET_BIN_1}
	rm -rf $(OBJS_2) ${TARGET_BIN_2}
