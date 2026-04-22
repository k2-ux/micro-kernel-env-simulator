CC     = gcc
CFLAGS = -Wall -Wextra -Wpedantic -std=c11

SRCS   = main.c sensor.c buffer.c status.c commands.c
OBJS   = $(SRCS:.c=.o)

.PHONY: all debug clean

all: simulator

simulator: $(OBJS)
	$(CC) $(CFLAGS) -o simulator $(OBJS)

debug: $(SRCS)
	$(CC) $(CFLAGS) -DDEBUG -o simulator_debug $(SRCS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f simulator simulator_debug *.o
