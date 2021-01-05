SYSROOT?=""
TARGET_BIN = tflite_edgetpu_benchmark

CXXFLAGS += -Wall -I${SYSROOT}/usr/include/tensorflow/lite/tools/make/downloads/flatbuffers/include -L${SYSROOT}/usr/lib

LDFLAGS  = -lpthread -ldl
LDFLAGS += -ledgetpu -Wl,--whole-archive -ltensorflow-lite  -Wl,--no-whole-archive

SRCS = tflite_edgetpu_benchmark.cc
OBJS = $(SRCS:.cc=.o)

all: ${TARGET_BIN}

$(TARGET_BIN): $(OBJS)
	$(CXX) ${LDFLAGS} -o $@ $^

$(OBJS): $(SRCS)
	$(CXX) $(CXXFLAGS) -c $^

clean:
	rm -rf $(OBJS) ${TARGET_BIN}
