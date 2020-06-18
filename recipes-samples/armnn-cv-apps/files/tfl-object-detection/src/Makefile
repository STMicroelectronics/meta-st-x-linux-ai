TARGET_BIN = objdetect_armnn_tfl_gst_gtk

CXXFLAGS += -Wall $(shell pkg-config --cflags gtk+-3.0 opencv cairo gstreamer-plugins-base-1.0)
LDFLAGS = $(shell pkg-config --libs gtk+-3.0 opencv cairo gstreamer-plugins-base-1.0) -lpthread -larmnn -larmnnTfLiteParser

SRCS = objdetect_armnn_tfl_gst_gtk.cc
OBJS = $(SRCS:.cc=.o)

all: ${TARGET_BIN}

$(TARGET_BIN): $(OBJS)
	$(CXX) ${LDFLAGS} -o $@ $^

$(OBJS): $(SRCS)
	$(CXX) $(CXXFLAGS) -c $^

clean:
	rm -rf $(OBJS) ${TARGET_BIN}
