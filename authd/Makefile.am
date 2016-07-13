bin_PROGRAMS = authd

AM_CPPFLAGS = $(WARNFLAGS)
AM_CPPFLAGS += -I$(top_srcdir)/include
AM_CPPFLAGS += -isystem $(top_srcdir)/boost/include
AM_LDFLAGS = -L$(top_srcdir)/boost/lib

authd_SOURCES =	\
	authd.cc	\
	dns.cc \
	getaddrinfo.cc \
	getnameinfo.cc \
	notice.cc \
	provider.cc \
	res.cc \
	reslib.cc \
	reslist.cc \
	providers/blacklist.cc \
	providers/ident.cc \
	providers/rdns.cc \
	providers/opm.cc

authd_LDADD = $(top_srcdir)/rb/librb.la
authd_LDADD += -lboost_system

mrproper-local:
	rm -rf providers/.deps
	rm -f providers/.dirstamp
