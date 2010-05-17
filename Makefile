CC := gcc
CFLAGS := -g -Wall
SRCS := auto_complete.c main.c
OBJS := $(SRCS:%.c=%.o)
LIBS := -lrt
TARGET := auto_complete

all :$(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

%.o:%.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) *~ $(OBJS)