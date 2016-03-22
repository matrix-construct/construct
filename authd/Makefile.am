pkglibexec_PROGRAMS = authd
AM_CFLAGS=$(WARNFLAGS)
AM_CPPFLAGS = -I../include -I../librb/include 

authd_SOURCES =	\
	authd.c	\
	res.c \
	reslib.c \
	reslist.c \
	dns.c \
	getaddrinfo.c \
	getnameinfo.c \
	provider.c \
	providers/rdns.c \
	providers/ident.c

authd_LDADD = ../librb/src/librb.la
