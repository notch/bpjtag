CFLAGS = -Wall -O2 -fomit-frame-pointer -std=c99 -D_GNU_SOURCE

BIN = debrick
OBJS = debrick.o

all: $(BIN)

$(BIN): $(OBJS)
	gcc $(CFLAGS) -o $@ $(OBJS)

clean:
	rm -rf *.o *~

distclean: clean
	rm -rf $(BIN)
