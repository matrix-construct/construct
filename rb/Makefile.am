AUTOMAKE_OPTIONS = foreign

pkgconfigdir = $(libdir)/pkgconfig
#pkgconfig_DATA = librb.pc

AM_CPPFLAGS = -I$(top_srcdir)/include @SSL_CFLAGS@ @GNUTLS_CFLAGS@ @MBEDTLS_CFLAGS@

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
	rb.c				\
	rb_memory.c			\
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
	version.c

librb_la_LDFLAGS = @SSL_LIBS@ @GNUTLS_LIBS@ @MBEDTLS_LIBS@ -avoid-version -no-undefined -export-symbols export-syms.txt
librb_la_LIBADD = @SSL_LIBS@ @GNUTLS_LIBS@ @MBEDTLS_LIBS@
lib_LTLIBRARIES = librb.la


mrproper-local:
	rm -f $(top_srcdir)/include/rb/config.h
	rm -f $(top_srcdir)/include/rb/config.h.in
	rm -f $(top_srcdir)/include/rb/stamp-h1
