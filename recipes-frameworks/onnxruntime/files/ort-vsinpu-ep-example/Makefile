SYSROOT?=""
TARGET_BIN = ort-vsinpu-ep-example

CXXFLAGS += -Wall
CXXFLAGS += -std=c++17
CXXFLAGS += -I$(SYSROOT)/usr/include/rapidjson
CXXFLAGS += -I$(SYSROOT)/usr/include/onnxruntime
CXXFLAGS += -I$(SYSROOT)/usr/include/onnxruntime/core/session

LDFLAGS = -ldl -lonnxruntime

SRCS = ort-vsinpu-ep-example.cc
OBJS = $(SRCS:.cc=.o)

all: $(TARGET_BIN)

$(TARGET_BIN): $(OBJS)
	$(CXX)  -o $@ $^ $(LDFLAGS)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf $(OBJS) $(TARGET_BIN)
