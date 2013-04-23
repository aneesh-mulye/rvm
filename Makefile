all: rvm.a
	rm -rf rvm.o

tests: rvm.a abort.c truncate.c basic.c multi.c
	g++ -ggdb abort.c rvm.a -o abort.test
	g++ -ggdb truncate.c rvm.a -o truncate.test
	g++ -ggdb basic.c rvm.a -o basic.test
	g++ -ggdb multi.c rvm.a -o multi.test
	g++ -ggdb multi-abort.c rvm.a -o multi-abort.test

rvm.a: rvm.o
	ar rvs rvm.a rvm.o

rvm.o: rvm.cpp rvm.h
	rm -rf rvm.o
	g++ -c -ggdb rvm.cpp

rvm.cpp:

rvm.h:
