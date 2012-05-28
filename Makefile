CFLAGS += -Wall -O2

BRJTAGOBJS = brjtag.o

all: brjtag

brjtag: $(BRJTAGOBJS)
	gcc $(CFLAGS) -o $@ $(BRJTAGOBJS)

clean:
	rm -rf *.o brjtag
