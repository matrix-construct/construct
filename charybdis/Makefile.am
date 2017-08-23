prefix = @prefix@

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@ROCKSDB_CPPFLAGS@ \
	@JS_CPPFLAGS@ \
	@BOOST_CPPFLAGS@

AM_LDFLAGS = \
	-dlopen self \
	-Wl,-fuse-ld=gold \
	-Wl,--no-gnu-unique \
	-Wl,--no-allow-shlib-undefined \
	-Wl,--no-undefined \
	-Wl,-z -Wl,now \
	-L$(top_srcdir)/ircd \
	-L$(top_srcdir)/modules \
	$(PLATFORM_LDFLAGS)

bin_PROGRAMS = charybdis

charybdis_LDFLAGS = \
	$(AM_LDFLAGS) \
	@ROCKSDB_LDFLAGS@ \
	@JS_LDFLAGS@ \
	@BOOST_LDFLAGS@

charybdis_LDADD = \
	-lircd \
	@ROCKSDB_LIBS@ \
	@JS_LIBS@ \
	@BOOST_LIBS@ \
	-lcrypto \
	-lssl \
	-lz

charybdis_SOURCES = \
	lgetopt.cc      \
	console.cc      \
	charybdis.cc
