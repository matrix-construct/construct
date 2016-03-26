pkglibexec_PROGRAMS = authd
AM_CFLAGS=$(WARNFLAGS)
AM_CPPFLAGS = -I../include -I../librb/include 


authd_SOURCES = authd.c res.c reslib.c reslist.c getnameinfo.c getaddrinfo.c dns.c notice.c
authd_LDADD = ../librb/src/librb.la
