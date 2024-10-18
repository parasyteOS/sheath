.PHONY: all

SRCS = $(shell find . -name '*.c')

OBJS = $(SRCS:%.c=%.o)

all: sheath

sheath: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

install: sheath
	install -m0755 -D sheath $(PREFIX)/bin/sheath
