CC=gcc
CXX=g++
RM=rm -f
CPPFLAGS=-g
LDFLAGS=-g
LDLIBS=-lpthread

SRCS=$(shell printf "%s " src/*.cpp)
OBJS=$(subst .cpp,.o,$(SRCS))

all: arcanet

arcanet: $(OBJS)
	$(CXX) $(LDFLAGS) -o arcanet $(OBJS) $(LDLIBS) 

depend: .depend

.depend: $(SRCS)
	$(CXX) $(CPPFLAGS) -MM $^ > ./.depend

clean:
	$(RM) $(OBJS)

dist-clean: clean
	$(RM) src/*~ ./.depend

include .depend

