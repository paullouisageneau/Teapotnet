prefix=/usr/local

DESTDIR=
TPROOT=/var/lib/teapotnet

CC=gcc
CXX=g++
RM=rm -f
CPPFLAGS=-g -O2 -I. -DSQLITE_ENABLE_FTS3 -DSQLITE_ENABLE_FTS3_PARENTHESIS
LDFLAGS=-g -O2
LDLIBS=-lpthread -ldl

SRCS=$(shell printf "%s " tpn/*.cpp)
OBJS=$(subst .cpp,.o,$(SRCS))

all: teapotnet

teapotnet: $(OBJS) include/sqlite3.o
	$(CXX) $(LDFLAGS) -o teapotnet $(OBJS) include/sqlite3.o $(LDLIBS) 

depend: .depend

.depend: $(SRCS)
	$(CXX) $(CPPFLAGS) -MM $^ > ./.depend
	
clean:
	$(RM) tpn/*.o include/*.o

dist-clean: clean
	$(RM) teapotnet
	$(RM) tpn/*~ ./.depend

include .depend

install: teapotnet teapotnet.service
	install -d $(DESTDIR)$(prefix)/bin
	install -d $(DESTDIR)$(prefix)/share/teapotnet
	install -d $(DESTDIR)/etc/teapotnet
	install -m 0755 teapotnet $(DESTDIR)$(prefix)/bin
	cp -r static $(DESTDIR)$(prefix)/share/teapotnet
	echo "static_dir=$(prefix)/share/teapotnet/static" > $(DESTDIR)/etc/teapotnet/config.conf
	@if [ -z "$(DESTDIR)" ]; then bash -c "./daemon.sh install $(prefix) $(TPROOT)"; fi

uninstall:
	rm -f $(DESTDIR)$(prefix)/bin/teaponet
	rm -rf $(DESTDIR)$(prefix)/share/teapotnet
	rm -f $(DESTDIR)/etc/teapotnet/config.conf
	@if [ -z "$(DESTDIR)" ]; then bash -c "./daemon.sh uninstall $(prefix) $(TPROOT)"; fi

