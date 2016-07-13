AUTOMAKE_OPTIONS = foreign

pkgconfigdir = $(libdir)/pkgconfig
#pkgconfig_DATA = librb.pc

librbdir = @libdir@
AM_CPPFLAGS = @SSL_CFLAGS@ @GNUTLS_CFLAGS@ @MBEDTLS_CFLAGS@
AM_CPPFLAGS += -isystem $(top_srcdir)/boost/include
AM_CPPFLAGS += -I$(top_srcdir)/include

# This is temp since all remaining C is circling the drain.
AM_CFLAGS = -std=gnu99

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
	terminate.cc \
	rb.cc

librb_la_LDFLAGS = @SSL_LIBS@ @GNUTLS_LIBS@ @MBEDTLS_LIBS@ -avoid-version -no-undefined -export-symbols export-syms.txt
librb_la_LDFLAGS += -L$(top_srcdir)/boost/lib

librb_la_LIBADD = @SSL_LIBS@ @GNUTLS_LIBS@ @MBEDTLS_LIBS@
librb_la_LIBADD += -lboost_system
lib_LTLIBRARIES = librb.la
