MAKEFILE = makefile
SOURCE = worm.cpp
BIN = worm

worm: worm.cpp
	g++ -std=c++11 -pthread -o $(BIN) $(SOURCE) -lncurses

clean:
	\rm -rf $(BIN) *~ *.tar

tar:
	make clean
	mkdir worm-0.5
	cp worm.cpp worm-0.5
	cp makefile worm-0.5
	cp configure worm-0.5
	cp ChangeLog worm-0.5
	cp README worm-0.5
	cp LICENSE worm-0.5
	tar cf worm-0.5.tar ./worm-0.5/
	rm -rf worm-0.5

install:
	install $(BIN) $(DESTDIR)/usr/bin
