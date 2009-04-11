CFLAGS = -Wall -O2 -fomit-frame-pointer

BIN = debrick
OBJS = debrick.o

all: $(BIN)

$(BIN): $(OBJS)
	gcc $(CFLAGS) -o $@ $(OBJS)

clean:
	rm -rf *.o

distclean: clean
	rm -rf $(BIN)
