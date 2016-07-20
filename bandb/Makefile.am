AM_CFLAGS = \
	@SQLITE_CFLAGS@

AM_CPPFLAGS = \
	-DSQLITE_THREADSAFE=0 \
	-DSQLITE_OMIT_LOAD_EXTENSION \
	-I$(top_srcdir)/include \
	@BOOST_CPPFLAGS@

AM_LDFLAGS = \
	-L$(top_srcdir)/rb \
	@BOOST_LDFLAGS@


bin_PROGRAMS = bandb

bandb_DEPENDENCIES = \
	@SQLITE_OBJ@

bandb_LDADD = \
	-lrb \
	@SQLITE_LDFLAGS@ \
	@SQLITE_OBJ@ \
	@SQLITE_LIBS@ \
	@BOOST_LIBS@

EXTRA_bandb_SOURCES = \
	sqlite3.c

bandb_SOURCES = \
	bandb.cc \
	rsdb_sqlite3.c \
	rsdb_snprintf.c


bin_PROGRAMS += bantool

bantool_DEPENDENCIES = \
	@SQLITE_OBJ@

bantool_LDADD = \
	-lrb \
	@SQLITE_LDFLAGS@ \
	@SQLITE_LIBS@ \
	@SQLITE_OBJ@ \
	@BOOST_LIBS@

EXTRA_bantool_SOURCES = \
	sqlite3.c

bantool_SOURCES = \
	bantool.cc \
	rsdb_sqlite3.c \
	rsdb_snprintf.c
