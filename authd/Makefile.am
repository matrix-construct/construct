pkglibexec_PROGRAMS = authd
AM_CFLAGS=$(WARNFLAGS)
AM_CPPFLAGS = -I../include -I../librb/include 


authd_SOURCES = authd.c res.c reslib.c dns.c
authd_LDADD = ../librb/src/librb.la
