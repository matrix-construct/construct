AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS =   rb     \
            ircd

if BUILD_LTDL
	SUBDIRS += libltdl
endif

SUBDIRS += tools       \
           help        \
           doc         \
           ssld        \
           wsockd      \
           authd       \
           bandb       \
           modules     \
           extensions  \
           charybdis

logdir = @prefix@/var/log

install-data-hook:
	test -d ${DESTDIR}${logdir} || mkdir -p ${DESTDIR}${logdir}

mrproper:
	rm -f include/rb/config.h
	rm -f include/rb/config.h.in

clean-local:
	rm -f ircd/ircd_lexer.c
	rm -f ircd/ircd_parser.c
	rm -f ircd/ircd_parser.h
