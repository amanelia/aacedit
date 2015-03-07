CC = gcc
CFLAGS = -Wall
LDFLAGS = -Wall
OBJS = aacedit.o

all: aacedit

aacedit: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f aacedit $(OBJS)
