bin_PROGRAMS = ssld

AM_CPPFLAGS = $(WARNFLAGS)
AM_CPPFLAGS += -I$(top_srcdir)/include
AM_CPPFLAGS += -I$(top_srcdir)/boost/include

AM_LDFLAGS = -L$(top_srcdir)/ircd
AM_LDFLAGS += -L$(top_srcdir)/rb
AM_LDFLAGS += -L$(top_srcdir)/boost/lib

ssld_SOURCES = ssld.cc
ssld_LDADD = -lrb -lboost_system @ZLIB_LD@
