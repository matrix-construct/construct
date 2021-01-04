AUTOMAKE_OPTIONS = foreign

prefix = @prefix@

CXXFLAGS += \
	$(EXTRA_CXXFLAGS) \
	###

AM_CXXFLAGS = \
	-ftls-model=initial-exec \
	###

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@BOOST_CPPFLAGS@ \
	@SSL_CPPFLAGS@ \
	@CRYPTO_CPPFLAGS@ \
	$(EXTRA_CPPFLAGS) \
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
	-Wl,--wrap=epoll_wait \
	-L$(top_srcdir)/ircd \
	-L$(top_srcdir)/modules \
	$(PLATFORM_LDFLAGS) \
	$(EXTRA_LDFLAGS) \
	###

if PROFILE
if CLANG
AM_CXXFLAGS += -fprofile-generate
endif
endif

bin_PROGRAMS = construct

construct_LDFLAGS = \
	$(AM_LDFLAGS) \
	@BOOST_LDFLAGS@ \
	@SSL_LDFLAGS@ \
	@CRYPTO_LDFLAGS@ \
	@MALLOC_LDFLAGS@ \
	###

construct_LDADD = \
	-lircd \
	@BOOST_LIBS@ \
	@SSL_LIBS@ \
	@CRYPTO_LIBS@ \
	@MALLOC_LIBS@ \
	$(EXTRA_LIBS) \
	###

construct_SOURCES = \
	construct.cc    \
	homeserver.cc   \
	signals.cc      \
	console.cc      \
	lgetopt.cc      \
	###
