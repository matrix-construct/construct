AUTOMAKE_OPTIONS = foreign

prefix		= @prefix@
libircddir	= @libdir@

AM_CPPFLAGS = @LTDLINCL@ -I$(top_srcdir)/include -I$(top_srcdir)/librb/include
AM_LDFLAGS  = -L$(top_srcdir)/librb/src -export-dynamic -avoid-version -no-undefined
AM_YFLAGS = -d

BUILT_SOURCES = ircd_parser.h version.c

version.c: version.c.SH ../CREDITS ../include/patchlevel.h ../include/serno.h
	$(SHELL) version.c.SH
	$(CP) version.c version.c.last
	touch version.c.SH

if MINGW
EXTRA_FLAGS = -Wl,--enable-runtime-pseudo-reloc -export-symbols-regex '*'
endif

libircd_la_SOURCES =                  \
  authproc.c			\
  bandbi.c                      \
  cache.c                       \
  capability.c			\
  channel.c                     \
  chmode.c                      \
  class.c                       \
  client.c                      \
  dns.c				\
  extban.c                      \
  getopt.c                      \
  hash.c                        \
  hook.c                        \
  hostmask.c                    \
  ircd.c                        \
  ircd_parser.y                 \
  ircd_lexer.l                  \
  ircd_signal.c                 \
  listener.c                    \
  logger.c                      \
  match.c                       \
  modules.c                     \
  monitor.c                     \
  msgbuf.c			\
  newconf.c                     \
  operhash.c                    \
  packet.c                      \
  parse.c                       \
  privilege.c                   \
  ratelimit.c			\
  reject.c                      \
  restart.c                     \
  s_conf.c                      \
  s_newconf.c                   \
  s_serv.c                      \
  s_user.c                      \
  scache.c                      \
  send.c                        \
  snomask.c                     \
  sslproc.c                     \
  substitution.c                \
  supported.c                   \
  tgchange.c                    \
  version.c                     \
  whowas.c			\
  wsproc.c
libircd_la_LDFLAGS = $(EXTRA_FLAGS) -avoid-version -no-undefined
libircd_la_LIBADD = @LIBLTDL@ -L$(top_srcdir)/librb/src -lrb
libircd_LTLIBRARIES = libircd.la

bin_PROGRAMS = charybdis
charybdis_SOURCES = main.c
charybdis_LDFLAGS = $(EXTRA_FLAGS) -dlopen self
charybdis_LDADD = libircd.la -L$(top_srcdir)/librb/src -lrb

install-exec-hook: install-libircdLTLIBRARIES
	$(RM) -f version.c
