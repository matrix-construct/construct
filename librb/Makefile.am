AUTOMAKE_OPTIONS = foreign

SUBDIRS = src

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = librb.pc

BUILT_SOURCES = include/serno.h

# NOTE: you'll want to change the below if you are unbundling
include/serno.h:
	@if [ -f ../include/serno.h ]; then \
		cp ../include/serno.h include/; \
	else \
		echo '#define SERNO "unknown"' >include/serno.h; \
		echo '#define DATECODE 0UL' >>include/serno.h; \
	fi

clean-local:
	rm -f include/serno.h
	rm -f src/version.c
	rm -f src/version.c.last
