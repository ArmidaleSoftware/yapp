# Copyright (c) YAPP contributors
# SPDX-License-Identifier: MIT

# YAPP Makefile.
# This uses GNU make.

VERSION:=	3.1.1
DEFS:=		-DINCLUDE_EXTRA_COMMANDS -DNEWS -DWWW -DVERSION='"$(VERSION)"'
INCLUDE:=	-I/usr/local/include
CXX:=		clang++ -std=c++23
CXXFLAGS:=	-Wall -Werror $(DEFS) -g $(INCLUDE)
LIBS:=		-lcrypt -L/usr/local/lib -lreadline

# Executable file
PREFIX:=	/arbornet/m-net/bbs
BINDIR:=	$(PREFIX)/bin
SBINDIR:=	/suid/bin
WWWDIR:=	/arbornet/m-net/bbs/www

PROG:=		bbs
EXTRA_PROGS:=	html_pager html_check html_filter receive_post webuser
TESTS:= str_test

# List of build files
SRCS:=		arch.cc change.cc conf.cc dates.cc driver.cc edbuf.cc \
		edit.cc files.cc help.cc item.cc joq.cc lib.cc license.cc \
		log.cc macro.cc main.cc mem.cc misc.cc news.cc range.cc \
		rfp.cc security.cc sep.cc stats.cc str.cc sum.cc sysop.cc \
		system.cc user.cc www.cc

OBJS:=		$(SRCS:%.cc=%.o)

all:		$(PROG) $(EXTRA_PROGS) $(TESTS)

$(PROG):	$(OBJS) GNUmakefile
		$(CXX) $(CXXFLAGS) -o $(PROG) $(OBJS) $(LIBS) -lgdbm

html_pager:	html_pager.o str.o
		$(CXX) $(CXXFLAGS) -o $@ $^

html_filter:	html_filter.o
		$(CXX) $(CXXFLAGS) -o $@ html_filter.o

html_check:	html_check.o mem.o str.o
		$(CXX) $(CXXFLAGS) -o $@ $^

receive_post:	receive_post.o util.o mem.o str.o
		$(CXX) $(CXXFLAGS) -o $@ $^

webuser:	webuser.o str.o
		$(CXX) -g -O2 ${STATIC} -o $@ $^ $(LIBS)

where:		where.o files.o
		$(CXX) $(CXXFLAGS) -o where where.o files.o

str_test:	str_test.o str.o
		$(CXX) $(CXXFLAGS) -o $@ $^  -L/usr/local/lib -lgtest -lgtest_main

clean:
		rm -f -- $(OBJS) $(PROG) $(EXTRA_PROGS) $(TESTS) \
		    html_check.o html_pager.o receive_post.o \
		    webuser.o html_filter.o util.s *_test.o $(DEPENDS)

install:	$(PROG)
		install -c -o cfadm -g cfadm -m 4111 $(PROG) $(SBINDIR)/bbs

test:		$(TESTS)
		for t in $(TESTS); do ./$$t; done

tinstall:	$(PROG)
		install -c -o cfadm -g cfadm -m 4551 $(PROG) $(SBINDIR)/bbs.testing

# Old install commands included:
#cp cfcreate cfdelete /arbornet/m-net/bbs/bin
#chmod 700 /usr/bbs/bin/cfcreate /usr/bbs/bin/cfdelete
#cp receive_post html_check html_filter html_pager $(WWWDIR)
#cp webuser $(BINDIR)
#chmod 4711 $(BINDIR)/webuser

# dependencies.
DEPENDS=	$(SRCS:%.cc=%.d)
depend:         $(DEPENDS)

%.d:		%.cc
		CXX="$(CXX)" sh makedep `dirname $<` $(CXXFLAGS) $< > $@

ifneq ($(MAKECMDGOALS), clean)
-include $(DEPENDS)
endif
