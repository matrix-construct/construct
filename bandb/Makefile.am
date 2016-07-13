bin_PROGRAMS = bandb bantool

AM_CPPFLAGS = $(WARNFLAGS)
AM_CPPFLAGS += -I$(top_srcdir)/include
AM_CPPFLAGS += -isystem $(top_srcdir)/boost/include
AM_CPPFLAGS += @SQLITE_INCLUDES@
AM_CPPFLAGS += -DSQLITE_THREADSAFE=0 -DSQLITE_OMIT_LOAD_EXTENSION

AM_LDFLAGS = -L$(top_srcdir)/rb
AM_LDFLAGS += -L$(top_srcdir)/ircd
AM_LDFLAGS += -L$(top_srcdir)/boost/lib

bandb_SOURCES = bandb.cc rsdb_sqlite3.c rsdb_snprintf.c
EXTRA_bandb_SOURCES = sqlite3.c
bandb_LDADD = -lrb @SQLITE_LD@ @SQLITE_OBJ@ -lboost_system
bandb_DEPENDENCIES = @SQLITE_OBJ@

bantool_SOURCES = bantool.cc rsdb_sqlite3.c rsdb_snprintf.c
EXTRA_bantool_SOURCES = sqlite3.c
bantool_LDADD = -lrb @SQLITE_LD@ @SQLITE_OBJ@ -lboost_system
bantool_DEPENDENCIES = @SQLITE_OBJ@
