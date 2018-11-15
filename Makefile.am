AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I tools/m4

SUBDIRS = include/ircd
SUBDIRS += ircd
SUBDIRS += modules
SUBDIRS += share
SUBDIRS += construct

.PHONY:      subdirs $(SUBDIRS)
$(SUBDIRS):
	$(MAKE) -C $@

mrproper-local:
	rm -f aclocal.m4
	rm -rf autom4te.cache
	rm -f compile
	rm -f config.*
	rm -f configure
	rm -f depcomp
	rm -f ltmain.sh
	rm -f install-sh
	rm -f libtool
	rm -f missing
	rm -f */Makefile
	rm -f */Makefile.in
	rm -f include/*/Makefile
	rm -f include/*/Makefile.in
	rm -rf */.deps
	rm -f Makefile
	rm -f Makefile.in
	rm -f tools/m4/argz.m4
	rm -f tools/m4/libtool.m4
	rm -f tools/m4/ltargz.m4
	rm -f tools/m4/ltoptions.m4
	rm -f tools/m4/ltsugar.m4
	rm -f tools/m4/ltversion.m4
	rm -f tools/m4/lt~obsolete.m4
