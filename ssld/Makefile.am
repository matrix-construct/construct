AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@BOOST_CPPFLAGS@

AM_LDFLAGS = \
	-L$(top_srcdir)/ircd \
	-L$(top_srcdir)/rb \
	@BOOST_LDFLAGS@


bin_PROGRAMS = ssld

ssld_LDADD = \
	-lircd \
	-lrb \
	@ZLIB_LD@ \
	@BOOST_LIBS@

ssld_SOURCES = \
	ssld.cc
