# $Id: Makefile.am 24820 2008-01-02 19:47:32Z androsyn $

AUTOMAKE_OPTIONS = foreign

INCLUDES = -I. -I../include @SSL_INCLUDES@

libratbox_la_SOURCES =			\
	unix.c				\
	win32.c				\
	crypt.c				\
	balloc.c			\
	commio.c			\
	openssl.c			\
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
	

libratbox_la_LDFLAGS = -avoid-version -no-undefined -export-symbols export-syms.txt
libratbox_la_LIBADD = @CRYPT_LIB@ @SSL_LIBS@ 
lib_LTLIBRARIES = libratbox.la

