# $Id: Makefile.am 25532 2008-06-11 14:40:44Z androsyn $

AUTOMAKE_OPTIONS = foreign

INCLUDES = -I. -I../include @SSL_CFLAGS@ @GNUTLS_CFLAGS@

libratbox_la_SOURCES =			\
	unix.c				\
	win32.c				\
	crypt.c				\
	balloc.c			\
	commio.c			\
	openssl.c			\
	gnutls.c			\
	nossl.c				\
	event.c				\
	ratbox_lib.c			\
	rb_memory.c			\
	linebuf.c			\
	snprintf.c			\
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
	patricia.c
	

libratbox_la_LDFLAGS = -version-info 3:0:0 -no-undefined -export-symbols export-syms.txt
libratbox_la_LIBADD = @CRYPT_LIB@ @SSL_LIBS@ @GNUTLS_LIBS@
lib_LTLIBRARIES = libratbox.la

