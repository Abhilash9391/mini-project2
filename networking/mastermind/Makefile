CC = gcc
CFLAGS = -std=c2x -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=700 \
          -Wall -Wextra -Werror -Wno-unused-parameter -g
LDLIBS =

SRCS = main.c mastermind.c codebreaker.c discovery.c utils.c
OBJS = $(SRCS:.c=.o)

mastermind: $(OBJS)
	$(CC) $(CFLAGS) -o mastermind $(OBJS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f mastermind $(OBJS)

.PHONY: clean
