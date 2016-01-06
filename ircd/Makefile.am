AUTOMAKE_OPTIONS = foreign

prefix		= @prefix@
libircddir	= @libdir@

AM_CPPFLAGS = @LTDLINCL@ -I$(top_srcdir)/include -I$(top_srcdir)/libratbox/include
AM_LDFLAGS  = -L$(top_srcdir)/libratbox/src -export-dynamic -avoid-version -no-undefined
AM_YFLAGS = -d
LDADD = @LIBLTDL@ -lratbox

BUILT_SOURCES = version.c

version.c: version.c.SH ../CREDITS ../include/patchlevel.h ../include/serno.h
	$(SHELL) version.c.SH
	$(CP) version.c version.c.last
	touch version.c.SH

libircd_la_SOURCES =                  \
  bandbi.c                      \
  blacklist.c                   \
  cache.c                       \
  capability.c			\
  channel.c                     \
  chmode.c                      \
  class.c                       \
  client.c                      \
  extban.c                      \
  getopt.c                      \
  hash.c                        \
  hook.c                        \
  hostmask.c                    \
  ipv4_from_ipv6.c		\
  irc_dictionary.c              \
  ircd.c                        \
  ircd_parser.y                 \
  ircd_lexer.l                  \
  ircd_signal.c                 \
  listener.c                    \
  logger.c                      \
  match.c                       \
  modules.c                     \
  monitor.c                     \
  newconf.c                     \
  operhash.c                    \
  packet.c                      \
  parse.c                       \
  privilege.c                   \
  ratelimit.c			\
  res.c                         \
  reslib.c                      \
  reject.c                      \
  restart.c                     \
  s_auth.c                      \
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
  whowas.c
libircd_la_LDFLAGS = $(EXTRA_FLAGS) -avoid-version
libircd_LTLIBRARIES = libircd.la

bin_PROGRAMS = charybdis
charybdis_SOURCES = main.c
charybdis_LDADD = libircd.la

install-exec-hook: install-libircdLTLIBRARIES
	$(RM) -f version.c
