SYSROOT?=""
TARGET_BIN = x-linux-ai

CXXFLAGS += -std=c++17

SRCS = x-linux-ai-tool.cc
OBJS = $(SRCS:.cc=.o)

all: $(TARGET_BIN)

$(TARGET_BIN): $(OBJS)
	$(CXX) -o $@ $^

$(OBJS): $(SRCS)
	$(CXX) $(CXXFLAGS) -c $^

clean:
	rm -rf $(OBJS) $(TARGET_BIN)

