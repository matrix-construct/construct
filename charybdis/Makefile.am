prefix = @prefix@
AM_CPPFLAGS = @LTDLINCL@ -I$(top_srcdir)/include

if MINGW
EXTRA_FLAGS = -Wl,--enable-runtime-pseudo-reloc -export-symbols-regex '*'
endif

bin_PROGRAMS = charybdis
charybdis_SOURCES = charybdis.cc
charybdis_LDFLAGS = $(EXTRA_FLAGS) -dlopen self
charybdis_LDADD = -L$(top_srcdir)/ircd -lircd
charybdis_LDADD += -L$(top_srcdir)/rb -lrb
