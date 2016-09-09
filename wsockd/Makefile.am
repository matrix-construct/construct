AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@BOOST_CPPFLAGS@

AM_LDFLAGS = \
	-L$(top_srcdir)/ircd \
	-L$(top_srcdir)/rb \
	@BOOST_LDFLAGS@


bin_PROGRAMS = wsockd

wsockd_LDADD = \
	-lircd \
	-lrb \
	@BOOST_LIBS@

wsockd_SOURCES = \
	wsockd.cc \
	sha1.c
