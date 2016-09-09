AM_CFLAGS = \
	@SQLITE_CFLAGS@

AM_CPPFLAGS = \
	-DSQLITE_THREADSAFE=0 \
	-DSQLITE_OMIT_LOAD_EXTENSION \
	-I$(top_srcdir)/include \
	@BOOST_CPPFLAGS@

AM_LDFLAGS = \
	-L$(top_srcdir)/ircd \
	-L$(top_srcdir)/rb \
	@BOOST_LDFLAGS@


bin_PROGRAMS = bandb

bandb_DEPENDENCIES = \
	@SQLITE_OBJ@

bandb_LDADD = \
	-lircd \
	-lrb \
	@SQLITE_LDFLAGS@ \
	@SQLITE_OBJ@ \
	@SQLITE_LIBS@ \
	@BOOST_LIBS@

EXTRA_bandb_SOURCES = \
	sqlite3.c

bandb_SOURCES = \
	bandb.cc \
	rsdb_sqlite3.cc \
	rsdb_snprintf.c


bin_PROGRAMS += bantool

bantool_DEPENDENCIES = \
	@SQLITE_OBJ@

bantool_LDADD = \
	-lircd \
	-lrb \
	@SQLITE_LDFLAGS@ \
	@SQLITE_LIBS@ \
	@SQLITE_OBJ@ \
	@BOOST_LIBS@

EXTRA_bantool_SOURCES = \
	sqlite3.c

bantool_SOURCES = \
	bantool.cc \
	rsdb_sqlite3.cc \
	rsdb_snprintf.c


sqlite3.o:
	echo -e "\033[1;36m***\033[0m Compiling sqlite3 may take a moment..."
	$(COMPILE) -w -c -o sqlite3.o sqlite3.c
