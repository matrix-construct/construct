AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = include/ircd
SUBDIRS += ircd
SUBDIRS += modules
SUBDIRS += construct
SUBDIRS += tools
SUBDIRS += doc

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
	rm -f ylwrap
	rm -f */Makefile
	rm -f */Makefile.in
	rm -f include/*/Makefile
	rm -f include/*/Makefile.in
	rm -rf */.deps
	rm -f Makefile
	rm -f Makefile.in
	rm -f m4/argz.m4
	rm -f m4/libtool.m4
	rm -f m4/ltargz.m4
	rm -f m4/ltoptions.m4
	rm -f m4/ltsugar.m4
	rm -f m4/ltversion.m4
	rm -f m4/lt~obsolete.m4
