AM_CPPFLAGS = $(WARNFLAGS)
AM_CPPFLAGS += @LTDLINCL@
AM_CPPFLAGS += -I$(top_srcdir)/include
AM_CPPFLAGS += -isystem $(top_srcdir)/boost/include

AM_LDFLAGS = -L$(top_srcdir)/rb
AM_LDFLAGS += -L$(top_srcdir)/boost/lib

bin_PROGRAMS = charybdis-mkpasswd charybdis-mkfingerprint

charybdis_mkpasswd_SOURCES = mkpasswd.cc
charybdis_mkpasswd_LDADD = -lrb -lboost_system

charybdis_mkfingerprint_SOURCES = mkfingerprint.cc
charybdis_mkfingerprint_LDADD = -lrb -lboost_system

mrproper-local:
	rm -f genssl
