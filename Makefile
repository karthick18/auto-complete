CC := gcc
CFLAGS := -g -Wall -DSPELL_CHECKER
SRCS := auto_complete.c main.c
OBJS := $(SRCS:%.c=%.o)
LIBS := -lrt
TARGET := auto_complete
AR := ar
RANLIB := ranlib
LIB_AUTOCOMPLETE := libautocomplete.a

all :$(TARGET) $(LIB_AUTOCOMPLETE)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LIBS)

$(LIB_AUTOCOMPLETE): $(OBJS)
	@ (\
	$(AR) cr  $@ $^;\
	$(RANLIB) $@ ;\
	)

%.o:%.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) *~ $(OBJS) $(LIB_AUTOCOMPLETE)