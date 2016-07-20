prefix = @prefix@

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@LTDLINCL@ \
	@BOOST_CPPFLAGS@

AM_LDFLAGS = \
	-dlopen self

AM_LDFLAGS += \
	-L$(top_srcdir)/ircd \
	-L$(top_srcdir)/rb \
	@BOOST_LDFLAGS@


bin_PROGRAMS = charybdis

charybdis_SOURCES = \
	charybdis.cc

charybdis_LDADD = \
	-lircd \
	-lrb \
	@LIBLTDL@ \
	@BOOST_LIBS@
