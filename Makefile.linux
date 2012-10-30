prefix=/usr/local

CC=gcc
CXX=g++
RM=rm -f
CPPFLAGS=-O
LDFLAGS=-O
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

install: teapotnet
	install -d $(DESTDIR)$(prefix)/bin
	install -d $(DESTDIR)$(prefix)/share/teapotnet
	install -d /etc/teapotnet
	install -m 0755 teapotnet $(DESTDIR)$(prefix)/bin
	cp -r static $(DESTDIR)$(prefix)/share/teapotnet
	echo "static_dir=$(DESTDIR)$(prefix)/share/teapotnet/static" > /etc/teapotnet/config.conf

uninstall:
	rm -f $(DESTDIR)$(prefix)/bin/teaponet
	rm -rf $(DESTDIR)$(prefix)/share/teapotnet

