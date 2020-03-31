# Generated automatically from Makefile.in by configure.
#NMAGIC = -n
NMAGIC =

VERSION=3.1.1

# Compiler
CC = gcc
srcdir = .
LIBS = -lcrypt 
DEFS = -DHAVE_CONFIG_H -DVERSION='"$(VERSION)"'
#STATIC = -static
#CFLAGS = ${DEFS} -O -static 
#CFLAGS = ${DEFS} -O -g
CFLAGS = ${DEFS} -g -O2 ${STATIC}

TARFILE = yapp_bbs$(VERSION).tar

# Executable file
BINFILE  = bbs
BINDIR   = /usr/local/bin
WWWDIR   = /usr/home/thaler/yapp/www

# List of build files
OBJS = driver.o misc.o joq.o rfp.o conf.o change.o item.o edbuf.o arch.o \
       system.o help.o macro.o sep.o sum.o range.o dates.o lib.o files.o \
       xalloc.o news.o stats.o user.o license.o www.o sysop.o security.o \
       edit.o log.o

SRC1 = driver.c misc.c joq.c rfp.c conf.c change.c item.c edbuf.c arch.c
SRC2 = system.c help.c macro.c sep.c sum.c range.c dates.c lib.c files.c 
SRC3 = xalloc.c news.c stats.c user.c license.c www.c security.c log.c
SRCS = $(SRC1) $(SRC2) $(SRC3)

all: bbs html_pager html_check html_filter receive_post webuser make_license

html_pager: html_pager.o
	$(CC) $(CFLAGS) -o $@ html_pager.o

html_filter: html_filter.o
	$(CC) $(CFLAGS) -o $@ html_filter.o

html_check: html_check.o
	$(CC) $(CFLAGS) -o $@ html_check.o

receive_post: receive_post.o util.o
	$(CC) ${CFLAGS} -o $@ receive_post.o util.o

webuser: webuser.o
	$(CC) -g -O2 ${STATIC} -o $@ webuser.o $(LIBS)

make_license: make_license.o 
	$(CC) $(CFLAGS) -o $@ make_license.o $(LIBS)

$(BINFILE): $(OBJS) main.o
	$(CC) $(CFLAGS) $(NMAGIC) -o $(BINFILE) $(OBJS) main.o $(LIBS)
#	chmod 4755 $(BINFILE)

where: where.o xalloc.o files.o
	$(CC) $(CFLAGS) $(NMAGIC) -o where where.o xalloc.o files.o

depend:
	makedepend -- $(CFLAGS) -- $(SRCS)

clean:
	/bin/rm -f - $(OBJS) main.o html_check.o html_pager.o receive_post.o webuser.o html_filter.o make_license.o util.o

install: 
	cp $(BINFILE) $(BINDIR)/bbs
	chmod 4711 $(BINDIR)/bbs
	cp cfcreate cfdelete /usr/bbs
	chmod 700 /usr/bbs/bin/cfcreate /usr/bbs/bin/cfdelete
	cp receive_post html_check html_filter html_pager $(WWWDIR)
	cp webuser $(BINDIR)
	chmod 4711 $(BINDIR)/webuser

install2:
	cp $(BINFILE) /usr/local/bin/bbs2
	chmod 4711 /usr/local/bin/bbs2

tar:
	tar -cvf $(TARFILE) `cat MANIFEST.source`
	gzip $(TARFILE)

#	tar -cvf $(TARFILE) yapp/IAFA-PACKAGE yapp/README yapp/LICENSE yapp/INFO yapp/Changes yapp/Makefile yapp/*.h yapp/*.c yapp/help.tar yapp/data.tar
#	compress $(TARFILE)

shar:
	makekit -nyapp -s90k README LICENSE INFO Changes Makefile *.h *.c help.tar.Z.uu data.tar.Z.uu

helptar:
	cd bbs
	tar -cvf help.tar help
	compress help.tar

diffs:
	makediffs README LICENSE INFO Changes Makefile *.h *.c

lint:
	lint -abchu $(SRCS)

# autoconf lines
config.h.in: config.h.bot configure.in
	autoheader

Makefile: Makefile.in
	./config.status

config.h: config.h.in config.status
	./config.status

# DO NOT DELETE THIS LINE -- make depend depends on it.

driver.o: config.h yapp.h
driver.o: struct.h driver.h xalloc.h lib.h macro.h joq.h rfp.h change.h
driver.o: item.h sep.h help.h conf.h system.h files.h 
driver.o: sum.h edbuf.h misc.h user.h edit.h
misc.o: config.h yapp.h struct.h globals.h misc.h change.h log.h
misc.o: help.h system.h macro.h range.h item.h sum.h sep.h files.h
misc.o: arch.h driver.h lib.h user.h
joq.o: config.h yapp.h struct.h lib.h globals.h joq.h item.h
joq.o: sum.h xalloc.h files.h stats.h
rfp.o: config.h news.h stats.h edit.h conf.h
rfp.o: yapp.h struct.h globals.h rfp.h item.h macro.h driver.h lib.h sum.h
rfp.o: xalloc.h arch.h system.h files.h range.h sep.h
conf.o: config.h macro.h system.h change.h sep.h xalloc.h driver.h stats.h
conf.o: yapp.h struct.h globals.h conf.h lib.h joq.h sum.h item.h range.h
change.o: config.h yapp.h struct.h sep.h xalloc.h stats.h Makefile
change.o: globals.h conf.h lib.h joq.h sum.h item.h range.h macro.h system.h
item.o: config.h yapp.h struct.h macro.h item.h range.h globals.h lib.h arch.h
item.o: system.h sum.h sep.h conf.h xalloc.h edbuf.h main.h driver.h files.h
item.o: news.h stats.h
edbuf.o: config.h yapp.h struct.h system.h
edbuf.o: globals.h macro.h driver.h lib.h item.h edbuf.h help.h files.h
arch.o: config.h yapp.h struct.h item.h lib.h globals.h arch.h
arch.o: xalloc.h news.h sum.h
system.o: config.h yapp.h struct.h lib.h xalloc.h macro.h globals.h files.h
system.o: driver.h system.h
help.o: config.h yapp.h struct.h lib.h globals.h conf.h
macro.o: config.h yapp.h struct.h globals.h macro.h lib.h Makefile
macro.o: xalloc.h stats.h conf.h user.h
sep.o: config.h yapp.h struct.h item.h range.h globals.h sep.h
sep.o: change.h lib.h sum.h macro.h xalloc.h main.h news.h stats.h
sum.o: config.h yapp.h struct.h globals.h lib.h sum.h item.h
sum.o: xalloc.h files.h macro.h sep.h news.h stats.h
range.o: config.h yapp.h struct.h item.h
range.o: range.h globals.h dates.h macro.h lib.h sum.h stats.h xalloc.h
dates.o: config.h yapp.h
lib.o: config.h yapp.h
lib.o: struct.h xalloc.h globals.h lib.h macro.h system.h main.h files.h
log.o: config.h yapp.h struct.h globals.h log.h macro.h
files.o: config.h yapp.h struct.h globals.h files.h xalloc.h system.h
xalloc.o: config.h yapp.h struct.h xalloc.h globals.h
news.o: config.h yapp.h struct.h news.h arch.h item.h sep.h
news.o: range.h sum.h files.h globals.h xalloc.h lib.h
news.o: stats.h rfp.h macro.h
stats.o: config.h yapp.h struct.h globals.h lib.h stats.h xalloc.h system.h
user.o: config.h yapp.h xalloc.h user.h system.h
user.o: struct.h globals.h macro.h files.h lib.h
security.o: config.h yapp.h struct.h globals.h lib.h
receive_post.o: config.h Makefile
html_check.o: config.h Makefile
html_filter.o: config.h Makefile
