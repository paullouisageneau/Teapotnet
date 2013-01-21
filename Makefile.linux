prefix=/usr/local

DESTDIR=
TPROOT=/var/lib/teapotnet

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
	$(RM) teapotnet
	$(RM) src/*~ ./.depend

include .depend

install: teapotnet teapotnet.service
	install -d $(DESTDIR)$(prefix)/bin
	install -d $(DESTDIR)$(prefix)/share/teapotnet
	install -d $(DESTDIR)/etc/teapotnet
	install -m 0755 teapotnet $(DESTDIR)$(prefix)/bin
	cp -r static $(DESTDIR)$(prefix)/share/teapotnet
	echo "static_dir=$(prefix)/share/teapotnet/static" > $(DESTDIR)/etc/teapotnet/config.conf
	@if [ -z "$(DESTDIR)" ]; then bash -c "./daemon.sh install $(prefix)"; fi

uninstall:
	rm -f $(DESTDIR)$(prefix)/bin/teaponet
	rm -rf $(DESTDIR)$(prefix)/share/teapotnet
	rm -f $(DESTDIR)/etc/teapotnet/config.conf
	@if [ -z "$(DESTDIR)" ]; then bash -c "./daemon.sh uninstall $(prefix)"; fi

