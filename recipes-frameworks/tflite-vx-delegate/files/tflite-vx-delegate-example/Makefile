SYSROOT?=""
TARGET_BIN = tflite-vx-delegate-example

CXXFLAGS += -Wall
CXXFLAGS += -std=c++17
CXXFLAGS += -I$(SYSROOT)/usr/include/tensorflow/lite/flatbuffers/include
CXXFLAGS += -I$(SYSROOT)/usr/include/tensorflow/lite/abseil-cpp

LDFLAGS = -ldl -ltensorflow-lite -lvx_custom_op

SRCS = tflite-vx-delegate-example.cc
OBJS = $(SRCS:.cc=.o)

all: $(TARGET_BIN)

$(TARGET_BIN): $(OBJS)
	$(CXX)  -o $@ $^ $(LDFLAGS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS) $(TARGET_BIN)
