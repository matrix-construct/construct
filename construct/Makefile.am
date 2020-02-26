prefix = @prefix@

AM_CXXFLAGS = \
	-ftls-model=initial-exec \
	@EXTRA_CXXFLAGS@ \
	###

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@BOOST_CPPFLAGS@ \
	@SSL_CPPFLAGS@ \
	@CRYPTO_CPPFLAGS@ \
	@EXTRA_CPPFLAGS@ \
	###

AM_LDFLAGS = \
	-dlopen self \
	-Wl,--warn-execstack \
	-Wl,--warn-common \
	-Wl,--detect-odr-violations \
	-Wl,--unresolved-symbols=report-all \
	-Wl,--allow-shlib-undefined \
	-Wl,--dynamic-list-data \
	-Wl,--dynamic-list-cpp-new\
	-Wl,--dynamic-list-cpp-typeinfo \
	-Wl,--rosegment \
	-Wl,-z,noexecstack \
	-L$(top_srcdir)/ircd \
	-L$(top_srcdir)/modules \
	$(PLATFORM_LDFLAGS) \
	@EXTRA_LDFLAGS@ \
	###

bin_PROGRAMS = construct

construct_LDFLAGS = \
	$(AM_LDFLAGS) \
	@BOOST_LDFLAGS@ \
	@SSL_LDFLAGS@ \
	@CRYPTO_LDFLAGS@ \
	###

construct_LDADD = \
	-lircd \
	@BOOST_LIBS@ \
	@SSL_LIBS@ \
	@CRYPTO_LIBS@ \
	@EXTRA_LIBS@ \
	###

construct_SOURCES = \
	construct.cc    \
	signals.cc      \
	console.cc      \
	lgetopt.cc      \
	###
