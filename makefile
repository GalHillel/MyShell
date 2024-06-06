CC=gcc

SHELL_EXEC=myshell

SRCS=shell.c
OBJS=$(SRCS:.c=.o)

.PHONY: all clean run

all: $(SHELL_EXEC)

$(SHELL_EXEC): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) -o $(SHELL_EXEC)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(SHELL_EXEC)

run: $(SHELL_EXEC)
	./$(SHELL_EXEC)
