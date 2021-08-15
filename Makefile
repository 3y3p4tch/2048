main: 2048.cpp
	g++ -Ofast -fshort-enums -fexpensive-optimizations -free 2048.cpp -o main -lncursesw

clean:
	rm main

.PHONY: clean