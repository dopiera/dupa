CXX=g++
LD=g++

#CXXFLAGS=-Wall -Wextra -Werror -D_FILE_OFFSET_BITS=64 -D_GLIBXX_DEBUG -D_GLIBXX_DEBUG_PEDANTIC -I/usr/pkg/include -g -O0
CXXFLAGS=-Wall -Wextra -Werror -D_FILE_OFFSET_BITS=64 -O2
LDFLAGS=-lboost_filesystem-mt -lboost_system-mt -lboost_thread-mt -lcrypto

all: dup_ident

dup_ident: dup_ident.o
	$(LD) -o dup_ident dup_ident.o $(LDFLAGS)

dup_ident.o: dup_ident.cpp
	$(CXX) -c -o dup_ident.o $(CXXFLAGS) dup_ident.cpp

clean:
	rm -f *.o dup_ident
