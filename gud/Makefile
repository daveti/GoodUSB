#
# File		: Makefile
# Description	: Build file for gud 
# Created	: Mon Feb  9 13:47:28 EST 2015
# By		: Dave Tian
#

#
# Environment Setup
#
LIBDIRS=
DEPFILE=Makefile.dep
CC=gcc
CFLAGS=-c $(INCLUDES) -g -Wall 
LINK=gcc
LINKFLAGS=-g
LIBS=

#
# Setup builds
#
GUDOBJS= gud.o \
	nlm.o \
	usb.o

TARGETS	= gud

#
# Project Builds
#
gud : $(GUDOBJS)
	$(LINK) $(LINKFLAGS) $(GUDOBJS) $(LIBS) -o $@

# Various maintenance stuff
clean : 
	rm -f $(TARGETS) $(GUDOBJS) $(DEPFILE) 2>&1

install:
	install -C $(GUDOBJS) $(TARGETDIR)


# Do dependency generation
depend : $(DEPFILE)

$(DEPFILE) : $(GUDOBJS:.o=.c)
	gcc -MM $(CFLAGS) $(GUDOBJS:.o=.c) > $(DEPFILE)

