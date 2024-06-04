SYSROOT?=""
TARGET_BIN = x-linux-ai

CXXFLAGS += -std=c++17
LDFLAGS += -Wl,--hash-style=gnu

SRCS = x-linux-ai-tool.cc
OBJS = $(SRCS:.cc=.o)

all: $(TARGET_BIN)

$(TARGET_BIN): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

$(OBJS): $(SRCS)
	$(CXX) $(CXXFLAGS) -c $^

clean:
	rm -rf $(OBJS) $(TARGET_BIN)

