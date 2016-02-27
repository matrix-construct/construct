AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = libratbox

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
	@if [ -d .git ] ; then \
		revh=`git log -1 --date=short --pretty=format:%cd_%h 2>/dev/null | sed -e s/-//g -e s/_/-/`;\
		[ -z "$$revh" ] || echo '#define SERNO "'$$revh'"' >include/serno.h ; \
	elif [ -d .hg ] ; then \
		revh=`hg parents --template '{date|shortdate}_{node|short}' 2>/dev/null | sed -e s/-//g -e s/_/-/`;\
		[ -z "$$revh" ] || echo '#define SERNO "'$$revh'"' >include/serno.h ; \
	fi
	@[ -f include/serno.h ] || echo '#define SERNO "unknown"' >include/serno.h

install-data-hook:
	test -d ${DESTDIR}${logdir} || mkdir -p ${DESTDIR}${logdir}
