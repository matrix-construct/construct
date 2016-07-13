bin_PROGRAMS = wsockd

AM_CPPFLAGS = $(WARNFLAGS)
AM_CPPFLAGS += -isystem $(top_srcdir)/boost/include
AM_CPPFLAGS += -I$(top_srcdir)/include

AM_LDFLAGS = -L$(top_srcdir)/rb
AM_LDFLAGS += -L$(top_srcdir)/boost/lib

wsockd_SOURCES = wsockd.cc sha1.c
wsockd_LDADD = -lrb -lboost_system
