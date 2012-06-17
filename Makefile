CXX=g++
LD=g++

#CXXFLAGS=-Wall -Wextra -Werror -D_GLIBXX_DEBUG -D_GLIBXX_DEBUG_PEDANTIC -I/usr/pkg/include -g -O0
CXXFLAGS=-Wall -Wextra -Werror -I/usr/pkg/include -O2 -g
LDFLAGS=-L/usr/pkg/lib -lboost_filesystem -lboost_system -lboost_thread

all: dup_ident

dup_ident: dup_ident.o
	$(LD) -o dup_ident $(LDFLAGS) dup_ident.o

dup_ident.o: dup_ident.cpp
	$(CXX) -c -o dup_ident.o $(CXXFLAGS) dup_ident.cpp

clean:
	rm -f *.o dup_ident
