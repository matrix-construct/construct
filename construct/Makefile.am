prefix = @prefix@

AM_CXXFLAGS = \
	@EXTRA_CXXFLAGS@ \
	###

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@ROCKSDB_CPPFLAGS@ \
	@JS_CPPFLAGS@ \
	@BOOST_CPPFLAGS@ \
	@SSL_CPPFLAGS@ \
	@CRYPTO_CPPFLAGS@ \
	@SODIUM_CPPFLAGS@ \
	@MAGIC_CPPFLAGS@ \
	@SNAPPY_CPPFLAGS@ \
	@LZ4_CPPFLAGS@ \
	@Z_CPPFLAGS@ \
	@EXTRA_CPPFLAGS@ \
	###

AM_LDFLAGS = \
	-dlopen self \
	-Wl,-fuse-ld=gold \
	-Wl,--warn-execstack \
	-Wl,--warn-common \
	-Wl,--detect-odr-violations \
	-Wl,--no-allow-shlib-undefined \
	-Wl,--no-gnu-unique \
	-Wl,--no-undefined \
	-Wl,--gc-sections \
	-Wl,--dynamic-list-data \
	-Wl,--dynamic-list-cpp-new\
	-Wl,--dynamic-list-cpp-typeinfo \
	-Wl,--icf=all \
	-Wl,--rosegment \
	-Wl,-z -Wl,combreloc \
	-Wl,-z -Wl,now \
	-L$(top_srcdir)/ircd \
	-L$(top_srcdir)/modules \
	$(PLATFORM_LDFLAGS) \
	@EXTRA_LDFLAGS@ \
	###

bin_PROGRAMS = construct

construct_LDFLAGS = \
	$(AM_LDFLAGS) \
	@ROCKSDB_LDFLAGS@ \
	@JS_LDFLAGS@ \
	@BOOST_LDFLAGS@ \
	@SSL_LDFLAGS@ \
	@CRYPTO_LDFLAGS@ \
	@SODIUM_LDFLAGS@ \
	@MAGIC_LDFLAGS@ \
	@SNAPPY_LDFLAGS@ \
	@LZ4_LDFLAGS@ \
	@Z_LDFLAGS@ \
	###

construct_LDADD = \
	-lircd \
	@ROCKSDB_LIBS@ \
	@JS_LIBS@ \
	@BOOST_LIBS@ \
	@SSL_LIBS@ \
	@CRYPTO_LIBS@ \
	@SODIUM_LIBS@ \
	@MAGIC_LIBS@ \
	@SNAPPY_LIBS@ \
	@LZ4_LIBS@ \
	@Z_LIBS@ \
	@EXTRA_LIBS@ \
	###

construct_SOURCES = \
	construct.cc    \
	signals.cc      \
	console.cc      \
	lgetopt.cc      \
	###
