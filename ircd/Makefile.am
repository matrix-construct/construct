AUTOMAKE_OPTIONS = foreign

libircddir	= @libdir@

# NOTE: the c++ version has a radically different API
#AM_YFLAGS = -d --language=c++
AM_YFLAGS = -d

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@LTDLINCL@ \
	@BOOST_CPPFLAGS@ \
	-include ircd/ircd.h

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

BUILT_SOURCES = ircd_lexer.cc ircd_parser.cc

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
  exception.cc                   \
  extban.cc                      \
  fs.cc                          \
  getopt.cc                      \
  hash.cc                        \
  hook.cc                        \
  hostmask.cc                    \
  ircd.cc                       \
  ircd_lexer.cc                \
  ircd_parser.cc                \
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

ircd_lexer.cc:
	flex $(AM_LFLAGS) -o ircd_lexer.cc ircd_lexer.ll

ircd_parser.cc:
	$(YACC) $(AM_YFLAGS) -o ircd_parser.cc ircd_parser.yy

install-data-local:
	test -d $(prefix)/@logdir@ || mkdir -p $(prefix)/@logdir@

clean-local:
	rm -f ircd_lexer.c ircd_lexer.cc
	rm -f ircd_parser.c ircd_parser.cc
	rm -f ircd_parser.h ircd_parser.hh
	rm -f stack.hh
