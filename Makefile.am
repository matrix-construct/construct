AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

SUBDIRS = libratbox

if BUILD_LTDL
SUBDIRS += libltdl
endif

SUBDIRS += ircd \
           ssld \
           bandb \
           tools \
           modules \
           extensions \
           help \
           doc

logdir = @prefix@/logs

install-data-hook:
	test -d ${logdir} || mkdir -p ${logdir}
