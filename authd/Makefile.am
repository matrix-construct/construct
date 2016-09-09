AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@BOOST_CPPFLAGS@

AM_LDFLAGS = \
	-L$(top_srcdir)/ircd \
	-L$(top_srcdir)/rb \
	@BOOST_LDFLAGS@


bin_PROGRAMS = authd

authd_LDADD = \
	$(top_srcdir)/ircd/libircd.la \
	$(top_srcdir)/rb/librb.la \
	@BOOST_LIBS@

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


mrproper-local:
	rm -rf providers/.deps
	rm -f providers/.dirstamp
