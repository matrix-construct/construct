pkglibexec_PROGRAMS = authd
AM_CFLAGS=$(WARNFLAGS)
AM_CPPFLAGS = -I../include -I../librb/include 

authd_SOURCES =	\
	authd.c	\
	dns.c \
	getaddrinfo.c \
	getnameinfo.c \
	notice.c \
	provider.c \
	res.c \
	reslib.c \
	reslist.c \
	providers/blacklist.c \
	providers/ident.c \
	providers/rdns.c \
	providers/opm.c

authd_LDADD = ../librb/src/librb.la
