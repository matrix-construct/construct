AUTOMAKE_OPTIONS = foreign

SUBDIRS = src

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = librb.pc

BUILT_SOURCES = include/serno.h

# NOTE: you'll want to change the below if you are unbundling
include/serno.h:
	@cp ../include/serno.h include/
