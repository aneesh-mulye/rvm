all: librvm.a
	rm -rf rvm.o

tests: librvm.a abort.c truncate.c basic.c multi.c multi-abort.c
	g++ -ggdb abort.c librvm.a -o abort.test
	g++ -ggdb truncate.c librvm.a -o truncate.test
	g++ -ggdb basic.c librvm.a -o basic.test
	g++ -ggdb multi.c librvm.a -o multi.test
	g++ -ggdb multi-abort.c librvm.a -o multi-abort.test

librvm.a: rvm.o
	ar rvs librvm.a rvm.o

rvm.o: rvm.cpp rvm.h
	rm -rf rvm.o
	g++ -c -ggdb rvm.cpp
