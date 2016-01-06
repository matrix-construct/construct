pkglibexec_PROGRAMS = bandb 
bin_PROGRAMS = bantool
AM_CFLAGS=$(WARNFLAGS)

AM_CPPFLAGS = -I../include -I../libratbox/include @SQLITE_INCLUDES@

bandb_SOURCES = bandb.c rsdb_sqlite3.c rsdb_snprintf.c
EXTRA_bandb_SOURCES = sqlite3.c
bandb_LDADD = ../libratbox/src/libratbox.la @SQLITE_LD@ @SQLITE_OBJ@
bandb_DEPENDENCIES = @SQLITE_OBJ@

bantool_SOURCES = bantool.c rsdb_sqlite3.c rsdb_snprintf.c
EXTRA_bantool_SOURCES = sqlite3.c
bantool_LDADD = ../libratbox/src/libratbox.la @SQLITE_LD@ @SQLITE_OBJ@
bantool_DEPENDENCIES = @SQLITE_OBJ@
