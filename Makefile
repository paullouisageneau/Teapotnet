CC=gcc
CXX=g++
RM=rm -f
CPPFLAGS=-g -O
LDFLAGS=-g -O
LDLIBS=-lpthread -lsqlite3

SRCS=$(shell printf "%s " src/*.cpp)
OBJS=$(subst .cpp,.o,$(SRCS))

all: teapotnet

teapotnet: $(OBJS)
	$(CXX) $(LDFLAGS) -o teapotnet $(OBJS) $(LDLIBS) 

depend: .depend

.depend: $(SRCS)
	$(CXX) $(CPPFLAGS) -MM $^ > ./.depend

clean:
	$(RM) $(OBJS)

dist-clean: clean
	$(RM) src/*~ ./.depend

include .depend

