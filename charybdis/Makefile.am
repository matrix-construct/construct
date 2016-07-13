prefix = @prefix@
AM_CPPFLAGS = @LTDLINCL@
AM_CPPFLAGS += -I$(top_srcdir)/include
AM_CPPFLAGS += -isystem $(top_srcdir)/boost/include

if MINGW
EXTRA_FLAGS = -Wl,--enable-runtime-pseudo-reloc -export-symbols-regex '*'
endif

bin_PROGRAMS = charybdis
charybdis_SOURCES = charybdis.cc

charybdis_LDFLAGS = $(EXTRA_FLAGS)
charybdis_LDFLAGS += -dlopen self
charybdis_LDFLAGS += -L$(top_srcdir)/ircd
charybdis_LDFLAGS += -L$(top_srcdir)/rb
charybdis_LDFLAGS += -L$(top_srcdir)/boost/lib

charybdis_LDADD = -lircd -lrb -lboost_system
