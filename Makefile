CFLAGS += -Wall -O2

WRT54GMEMOBJS = wrt54g.o

SWITCHOBJS = switchend.o

all: debrick switchend

debrick: $(WRT54GMEMOBJS)
	gcc $(CFLAGS) -o $@ $(WRT54GMEMOBJS)

switchend: $(SWITCHOBJS)
	gcc $(CFLAGS) -o $@ $(SWITCHOBJS)

clean:
	rm -rf *.o debrick switchend
