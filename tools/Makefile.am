bin_PROGRAMS = charybdis-mkpasswd charybdis-mkfingerprint
AM_CFLAGS=$(WARNFLAGS)
AM_CPPFLAGS = $(DEFAULT_INCLUDES) -I$(top_srcdir)/include

charybdis_mkpasswd_SOURCES = mkpasswd.c
charybdis_mkpasswd_LDADD = $(top_srcdir)/rb/librb.la

charybdis_mkfingerprint_SOURCES = mkfingerprint.c
charybdis_mkfingerprint_LDADD = $(top_srcdir)/rb/librb.la
