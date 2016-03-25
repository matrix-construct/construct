AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = librb

if BUILD_LTDL
SUBDIRS += libltdl
endif

SUBDIRS += ircd \
           ssld \
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

clean-local:
	rm -f include/serno.h
