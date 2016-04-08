AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = librb

if BUILD_LTDL
SUBDIRS += libltdl
endif

SUBDIRS += ircd \
           ssld \
           wsockd \
           authd \
           bandb \
           tools \
           modules \
           extensions \
           help \
           doc

logdir = @prefix@/logs

BUILT_SOURCES = include/serno.h

include/serno.h:
	@if [ -d .git ]; then \
		revh=`git log -1 --date=format:%Y%m%d --pretty=format:%cd-%h`; \
		datecode=`git log -1 --pretty=format:%ct`; \
		if [ -n "$$revh" ]; then \
			echo '#define SERNO "'$$revh'"' >include/serno.h ; \
			echo "#define DATECODE $${datecode}UL" >>include/serno.h; \
		fi \
	fi
	@if [ ! -f include/serno.h ]; then \
		echo '#define SERNO "unknown"' >include/serno.h; \
		echo '#define DATECODE 0UL' >>include/serno.h; \
	fi

install-data-hook:
	test -d ${DESTDIR}${logdir} || mkdir -p ${DESTDIR}${logdir}

install-exec-hook:
	rm -f ${DESTDIR}${moduledir}/*.la
	rm -f ${DESTDIR}${moduledir}/autoload/*.la
	rm -f ${DESTDIR}${moduledir}/extensions/*.la
	rm -f ${DESTDIR}${moduledir}/*.dll.a
	rm -f ${DESTDIR}${moduledir}/autoload/*.dll.a
	rm -f ${DESTDIR}${moduledir}/extensions/*.dll.a

distclean-local:
	rm -f librb/include/librb-config.h

clean-local:
	rm -f include/serno.h
	rm -f ircd/ircd_lexer.c
	rm -f ircd/ircd_parser.c
	rm -f ircd/ircd_parser.h
	rm -f ircd/version.c
	rm -f ircd/version.c.last
