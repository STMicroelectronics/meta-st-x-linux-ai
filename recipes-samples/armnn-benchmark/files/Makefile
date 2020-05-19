TARGET_BIN = armnn_tfl_benchmark

CXXFLAGS += -Wall
LDFLAGS = -lpthread -larmnn -larmnnTfLiteParser

SRCS = armnn_tfl_benchmark.cc
OBJS = $(SRCS:.cc=.o)

all: ${TARGET_BIN}

$(TARGET_BIN): $(OBJS)
	$(CXX) ${LDFLAGS} -o $@ $^

$(OBJS): $(SRCS)
	$(CXX) $(CXXFLAGS) -c $^

clean:
	rm -rf $(OBJS) ${TARGET_BIN}
