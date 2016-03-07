AUTOMAKE_OPTIONS = foreign

SUBDIRS = src

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = librb.pc

BUILT_SOURCES = include/serno.h

include/serno.h:
	@if [ -d .git ] ; then \
		revh=`git log -1 --date=format:%Y%m%d --pretty=format:%cd-%h`;\
		[ -z "$$revh" ] || echo '#define SERNO "'$$revh'"' >include/serno.h ; \
	elif [ -d .hg ] ; then \
		revh=`hg parents --template '{date|shortdate}_{node|short}' 2>/dev/null | sed -e s/-//g -e s/_/-/`;\
		[ -z "$$revh" ] || echo '#define SERNO "'$$revh'"' >include/serno.h ; \
	fi
	@[ -f include/serno.h ] || echo '#define SERNO "unknown"' >include/serno.h
