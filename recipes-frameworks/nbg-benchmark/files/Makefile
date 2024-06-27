# COPYRIGHT (C) 2023, STMICROELECTRONICS - All Rights Reserved
SYSROOT?=""

CFLAGS += -Wall -g

##############################################################################
# Supply necessary libraries.

LDFLAGS = -Wl,--no-as-needed
LDFLAGS += -L$(SYSROOT)/usr/lib -ljpeg -lovxlib -lOpenVX -lOpenVXU
LDFLAGS += -lpthread -ldl -lstdc++

TARGET_NAME = nbg_benchmark
CUR_SOURCE = vnn_utils.cc nbg_benchmark.cc
OBJECTS =  $(CUR_SOURCE:.cc=.o)

#############################################################################
# Objects.

all: $(TARGET_NAME)

$(TARGET_NAME): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJECTS): $(CUR_SOURCE)
	$(CC) $(CFLAGS) -c $^
clean:
	rm -rf $(OBJECTS) $(TARGET_NAME)
