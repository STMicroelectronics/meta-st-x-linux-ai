STM32MPU?=""
TARGET_BIN = x-linux-ai-application

CXXFLAGS += -std=c++17

SRCS = x-linux-ai-application.cc x-linux-ai-application-utils.cc
OBJS = $(SRCS:.cc=.o)

ifneq (,$(findstring STM32MP2,$(STM32MPU)))
	CXXFLAGS += -DSTM32MP2_platform
endif

ifneq (,$(findstring STM32MP1,$(STM32MPU)))
    CXXFLAGS += -DSTM32MP1_platform
endif

all: $(TARGET_BIN)

$(TARGET_BIN): $(OBJS)
	$(CXX) -o $@ $^

$(OBJS): $(SRCS)
	$(CXX) $(CXXFLAGS) -c $^

clean:
	rm -rf $(OBJS) $(TARGET_BIN)