CXX=g++
LD=g++

#CXXFLAGS=-Wall -Wextra -Werror -D_FILE_OFFSET_BITS=64 -D_GLIBXX_DEBUG -D_GLIBXX_DEBUG_PEDANTIC -I/usr/pkg/include -g -O0
CXXFLAGS=-Wall -Wextra -Werror -D_FILE_OFFSET_BITS=64 -O2 -Iprotos
LDFLAGS=-lboost_filesystem -lboost_system -lboost_thread -lboost_program_options -lcrypto -lprotobuf

all: dup_ident

dup_ident: dup_ident.o protos/dup_ident.o
	$(LD) -o dup_ident protos/dup_ident.o dup_ident.o $(LDFLAGS)

dup_ident.o: dup_ident.cpp protos/dup_ident.pb.h
	$(CXX) -c -o dup_ident.o $(CXXFLAGS) dup_ident.cpp

protos/dup_ident.o: protos/dup_ident.pb.cc protos/dup_ident.pb.h
	$(CXX) -c -o protos/dup_ident.o $(CXXFLAGS) protos/dup_ident.pb.cc

protos/dup_ident.pb.h protos/dup_ident.pb.cc: protos/dup_ident.proto
	( cd protos ; protoc dup_ident.proto --cpp_out . )

clean:
	rm -f *.o dup_ident protos/*.pb.cc protos/*.pb.h protos/*.o
