SYSROOT?=""
TARGET_BIN = tflite_edgetpu_benchmark

CXXFLAGS += -Wall -I${SYSROOT}/usr/include/tensorflow/lite/tools/make/downloads/flatbuffers/include -L${SYSROOT}/usr/lib
LDFLAGS = -Wl,--whole-archive -ltensorflow-lite  -Wl,--no-whole-archive
FLPTHREAD = -lpthread
FLIBEDGETPU = -l:libedgetpu-max.so.1.0

SRCS = tflite_edgetpu_benchmark.cc
OBJS = $(SRCS:.cc=.o)

all: ${TARGET_BIN}

$(TARGET_BIN): $(OBJS)
	$(CXX) ${FLPTHREAD} ${FLIBEDGETPU} ${LDFLAGS} -o $@ $^

$(OBJS): $(SRCS)
	$(CXX) $(CXXFLAGS) -c $^

clean:
	rm -rf $(OBJS) ${TARGET_BIN}
