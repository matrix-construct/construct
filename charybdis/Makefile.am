prefix = @prefix@

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@ROCKSDB_CPPFLAGS@ \
	@JS_CPPFLAGS@ \
	@BOOST_CPPFLAGS@

AM_LDFLAGS = \
	-dlopen self

AM_LDFLAGS += \
	-L$(top_srcdir)/ircd \
	-L$(top_srcdir)/rb \
	@ROCKSDB_LDFLAGS@ \
	@JS_LDFLAGS@ \
	@BOOST_LDFLAGS@


bin_PROGRAMS = charybdis

charybdis_SOURCES = \
	lgetopt.cc      \
	charybdis.cc

charybdis_LDADD = \
	-lircd \
	-lrb \
	@ROCKSDB_LIBS@ \
	@JS_LIBS@ \
	@BOOST_LIBS@
