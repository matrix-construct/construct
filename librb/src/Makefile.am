# $Id: Makefile.am 26046 2008-09-09 16:37:30Z androsyn $

AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = -I. -I../include @SSL_CFLAGS@ @GNUTLS_CFLAGS@ @MBEDTLS_CFLAGS@

BUILT_SOURCES = version.c

version.c: version.c.SH ../CREDITS
	$(SHELL) ./version.c.SH
	$(CP) version.c version.c.last

version.lo: version.c ../include/serno.h
	$(LIBTOOL) --mode=compile $(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

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
	rb_lib.c			\
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
	arc4random.c			\
	version.c

librb_la_LDFLAGS = @SSL_LIBS@ @GNUTLS_LIBS@ @MBEDTLS_LIBS@ -avoid-version -no-undefined -export-symbols export-syms.txt
librb_la_LIBADD = @SSL_LIBS@ @GNUTLS_LIBS@ @MBEDTLS_LIBS@
lib_LTLIBRARIES = librb.la
