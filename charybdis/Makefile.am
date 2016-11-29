prefix = @prefix@

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@ROCKSDB_CPPFLAGS@ \
	@JS_CPPFLAGS@ \
	@BOOST_CPPFLAGS@

AM_LDFLAGS = \
	-dlopen self \
	-Wl,-fuse-ld=gold \
	-Wl,--no-undefined \
	-Wl,--no-gnu-unique \
	$(PLATFORM_LDFLAGS)

AM_LDFLAGS += \
	-L$(top_srcdir)/ircd \
	@ROCKSDB_LDFLAGS@ \
	@JS_LDFLAGS@ \
	@BOOST_LDFLAGS@


bin_PROGRAMS = charybdis

charybdis_SOURCES = \
	lgetopt.cc      \
	charybdis.cc

charybdis_LDADD = \
	-lircd \
	@ROCKSDB_LIBS@ \
	@JS_LIBS@ \
	@BOOST_LIBS@ \
	-lcrypto \
	-lssl
