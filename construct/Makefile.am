prefix = @prefix@

AM_CXXFLAGS = \
	###

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@ROCKSDB_CPPFLAGS@ \
	@JS_CPPFLAGS@ \
	@BOOST_CPPFLAGS@ \
	@SODIUM_CPPFLAGS@ \
	###

AM_LDFLAGS = \
	-dlopen self \
	-Wl,-fuse-ld=gold \
	-Wl,--no-gnu-unique \
	-Wl,--no-allow-shlib-undefined \
	-Wl,--no-undefined \
	-Wl,--gc-sections \
	-Wl,--icf=all \
	-Wl,-z -Wl,now \
	-L$(top_srcdir)/ircd \
	-L$(top_srcdir)/modules \
	$(PLATFORM_LDFLAGS) \
	###

bin_PROGRAMS = construct

construct_LDFLAGS = \
	$(AM_LDFLAGS) \
	@ROCKSDB_LDFLAGS@ \
	@JS_LDFLAGS@ \
	@BOOST_LDFLAGS@ \
	@SODIUM_LDFLAGS@ \
	###

construct_LDADD = \
	-lircd \
	@ROCKSDB_LIBS@ \
	@JS_LIBS@ \
	@BOOST_LIBS@ \
	@SODIUM_LIBS@ \
	-lcrypto \
	-lssl \
	-lz \
	###

construct_SOURCES = \
	construct.cc    \
	console.cc      \
	lgetopt.cc      \
	###
