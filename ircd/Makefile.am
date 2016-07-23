AUTOMAKE_OPTIONS = foreign

libircddir	= @libdir@

AM_YFLAGS = -d # --language=c++
AM_LFLAGS = # --c++

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@LTDLINCL@ \
	@BOOST_CPPFLAGS@

if MINGW
PLATFORM_LDFLAGS = -Wl,--enable-runtime-pseudo-reloc -export-symbols-regex '*'
endif

AM_LDFLAGS = \
	-export-dynamic \
	-avoid-version \
	-no-undefined \
	$(PLATFORM_LDFLAGS)

AM_LDFLAGS += \
	-L$(top_srcdir)/rb \
	@BOOST_LDFLAGS@

BUILT_SOURCES = ircd_parser.h


libircd_LTLIBRARIES = libircd.la

libircd_la_LIBADD = \
	-lrb \
	@LIBLTDL@ \
	@BOOST_LIBS@

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


install-data-local:
	test -d $(prefix)/@logdir@ || mkdir -p $(prefix)/@logdir@

clean-local:
	rm -f ircd_lexer.c
	rm -f ircd_parser.c
	rm -f ircd_parser.h
