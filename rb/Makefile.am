AUTOMAKE_OPTIONS = foreign

pkgconfigdir = $(libdir)/pkgconfig
#pkgconfig_DATA = librb.pc
librbdir = @libdir@

# This is temp since all remaining C is circling the drain.
AM_CFLAGS = -std=gnu99

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@SSL_CFLAGS@ \
	@GNUTLS_CFLAGS@ \
	@MBEDTLS_CFLAGS@ \
	@BOOST_CPPFLAGS@

if MINGW
PLATFORM_LDFLAGS = -Wl,--enable-runtime-pseudo-reloc
endif

AM_LDFLAGS = \
	-avoid-version \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

AM_LDFLAGS += \
	-L$(top_srcdir)/rb \
	@BOOST_LDFLAGS@


lib_LTLIBRARIES = librb.la

librb_la_LIBADD = \
	@SSL_LIBS@ \
	@GNUTLS_LIBS@ \
	@MBEDTLS_LIBS@ \
	@BOOST_LIBS@

librb_la_SOURCES =			\
	unix.c				\
	win32.c				\
	crypt.c				\
	balloc.c			\
	commio.c			\
	mbedtls.c			\
	openssl.c			\
	gnutls.c			\
	nossl.c				\
	event.c				\
	linebuf.c			\
	tools.c				\
	helper.c			\
	devpoll.c			\
	epoll.c				\
	poll.c				\
	ports.c				\
	sigio.c				\
	select.c			\
	kqueue.c			\
	rawbuf.c			\
	patricia.c			\
	dictionary.c			\
	radixtree.c			\
	arc4random.c			\
	version.c			\
	terminate.cc			\
	format.cc			\
	rb.cc
