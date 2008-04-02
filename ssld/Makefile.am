#
# $Id: Makefile.am 24818 2008-01-02 18:38:26Z androsyn $
#
bin_PROGRAMS = ssld
AM_CFLAGS=$(WARNFLAGS)

INCLUDES = -I../include -I../libratbox/include 


ssld_SOURCES = ssld.c

ssld_LDADD = ../libratbox/src/libratbox.la @ZLIB_LD@


