CC := gcc
CFLAGS := -g -Wall -DSPELL_CHECKER
SRCS := main.c
LDSRCS := auto_complete.c spell_check.c
OBJS := $(SRCS:%.c=%.o)
LDOBJS := $(LDSRCS:%.c=%.o)
LDLIBS := -lrt -L./. -lautocomplete
TARGET := auto_complete
AR := ar
RANLIB := ranlib
LIB_AUTOCOMPLETE := libautocomplete.a

all :$(LIB_AUTOCOMPLETE) $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^  $(LDLIBS)

$(LIB_AUTOCOMPLETE): $(LDOBJS)
	@ (\
	$(AR) cr  $@ $^;\
	$(RANLIB) $@ ;\
	)

%.o:%.c
	$(CC) -c $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) *~ $(OBJS) $(LDOBJS) $(LIB_AUTOCOMPLETE)