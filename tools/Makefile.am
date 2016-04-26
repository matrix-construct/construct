bin_PROGRAMS = charybdis-mkpasswd charybdis-mkfingerprint
AM_CFLAGS=$(WARNFLAGS)
AM_CPPFLAGS = $(DEFAULT_INCLUDES) -I../librb/include -I.

charybdis_mkpasswd_SOURCES = mkpasswd.c
charybdis_mkpasswd_LDADD = ../librb/src/librb.la

charybdis_mkfingerprint_SOURCES = mkfingerprint.c
charybdis_mkfingerprint_LDADD = ../librb/src/librb.la
