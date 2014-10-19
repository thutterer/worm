MAKEFILE = makefile
SOURCE = src/worm.cpp
BIN = worm
VERSION = 0.6

Release:
	g++ -O2 -std=c++11 -pthread -o $(BIN) $(SOURCE) -lncurses

Debug:
	g++ -g -Wall -std=c++11 -pthread -o $(BIN) $(SOURCE) -lncurses

clean:
	\rm -rf $(BIN) *~ *.tar

tar:
	make clean
	mkdir worm-$(VERSION)
	cp worm.cpp worm-$(VERSION)
	cp makefile worm-$(VERSION)
	cp configure worm-$(VERSION)
	cp ChangeLog worm-$(VERSION)
	cp README worm-$(VERSION)
	cp LICENSE worm-$(VERSION)
	tar cf worm-$(VERSION).tar ./worm-$(VERSION)/
	rm -rf worm-$(VERSION)

install:
	install $(BIN) $(DESTDIR)/usr/bin
