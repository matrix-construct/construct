pkglibexec_PROGRAMS = bandb
bin_PROGRAMS = bantool
AM_CFLAGS=$(WARNFLAGS)

AM_CPPFLAGS = -I../include -I../librb/include @SQLITE_INCLUDES@
AM_CPPFLAGS += -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION

bandb_SOURCES = bandb.c rsdb_sqlite3.c rsdb_snprintf.c
EXTRA_bandb_SOURCES = sqlite3.c
bandb_LDADD = ../librb/src/librb.la @SQLITE_LD@ @SQLITE_OBJ@
bandb_DEPENDENCIES = @SQLITE_OBJ@

bantool_SOURCES = bantool.c rsdb_sqlite3.c rsdb_snprintf.c
EXTRA_bantool_SOURCES = sqlite3.c
bantool_LDADD = ../librb/src/librb.la @SQLITE_LD@ @SQLITE_OBJ@
bantool_DEPENDENCIES = @SQLITE_OBJ@
