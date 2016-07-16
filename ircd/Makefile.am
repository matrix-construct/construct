AUTOMAKE_OPTIONS = foreign
libircddir	= @libdir@

AM_CPPFLAGS = @LTDLINCL@
AM_CPPFLAGS += -I$(top_srcdir)/include
AM_CPPFLAGS += -isystem $(top_srcdir)/boost/include

AM_LDFLAGS = -export-dynamic -avoid-version -no-undefined
AM_LDFLAGS += -L$(top_srcdir)/rb
AM_LDFLAGS += -L$(top_srcdir)/boost/lib

AM_YFLAGS = -d # --language=c++
AM_LFLAGS = # --c++

if MINGW
EXTRA_FLAGS = -Wl,--enable-runtime-pseudo-reloc -export-symbols-regex '*'
endif

BUILT_SOURCES = ircd_parser.h

libircd_la_SOURCES =                  \
  authproc.cc			\
  bandbi.cc                      \
  cache.cc                       \
  capability.cc			\
  channel.cc                     \
  chmode.cc                      \
  class.cc                       \
  client.cc                     \
  dns.cc				\
  extban.cc                      \
  getopt.cc                      \
  hash.cc                        \
  hook.cc                        \
  hostmask.cc                    \
  ircd.cc                       \
  ircd_parser.y                 \
  ircd_lexer.l                   \
  ircd_signal.cc                 \
  listener.cc                   \
  logger.cc                     \
  match.cc                       \
  modules.cc                    \
  monitor.cc                     \
  msgbuf.cc			\
  newconf.cc                     \
  operhash.cc                    \
  packet.cc                      \
  parse.cc                       \
  privilege.cc                   \
  ratelimit.cc			\
  reject.cc                      \
  restart.cc                     \
  s_conf.cc                      \
  s_newconf.cc                   \
  s_serv.cc                      \
  s_user.cc                      \
  scache.cc                      \
  send.cc                        \
  snomask.cc                     \
  sslproc.cc                     \
  substitution.cc                \
  supported.cc                   \
  tgchange.cc                    \
  version.cc                     \
  whowas.cc			\
  wsproc.cc
libircd_la_LDFLAGS = $(EXTRA_FLAGS) -avoid-version -no-undefined

libircd_la_LIBADD = @LIBLTDL@
libircd_LTLIBRARIES = libircd.la

install-data-local:
	test -d $(prefix)/@logdir@ || mkdir -p $(prefix)/@logdir@

clean-local:
	rm -f ircd_lexer.c
	rm -f ircd_parser.c
	rm -f ircd_parser.h
