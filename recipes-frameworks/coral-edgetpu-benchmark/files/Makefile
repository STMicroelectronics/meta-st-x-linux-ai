SYSROOT?=""
TARGET_BIN = coral_edgetpu_benchmark

CXXFLAGS += -Wall -I$(SYSROOT)/usr/include/tensorflow/lite/flatbuffers/include -L${SYSROOT}/${libdir}

LDFLAGS  = -lpthread -ldl -ltensorflow-lite -ledgetpu

SRCS = coral_edgetpu_benchmark.cc
OBJS = $(SRCS:.cc=.o)

all: ${TARGET_BIN}

$(TARGET_BIN): $(OBJS)
	$(CXX) ${LDFLAGS} -o $@ $^

$(OBJS): $(SRCS)
	$(CXX) $(CXXFLAGS) -c $^

clean:
	rm -rf $(OBJS) ${TARGET_BIN}
