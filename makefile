MAKEFILE = makefile
SOURCE = worm.cpp
BIN = worm

worm: worm.cpp
	g++ -std=c++11 -pthread -o $(BIN) $(SOURCE) -lncurses

#.PHONY: clean
clean:
	\rm -rf $(BIN) *~

tar:
	tar cf worm.tar ../worm/
	#tar cf worm.tar $(SOURCE) $(MAKEFILE)

install:
	sudo install $(BIN) /usr/bin
