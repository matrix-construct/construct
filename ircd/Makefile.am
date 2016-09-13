AUTOMAKE_OPTIONS = foreign

libircddir	= @libdir@

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
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

libircd_LTLIBRARIES = libircd.la

libircd_la_LIBADD = \
	-lrb \
	@BOOST_LIBS@

libircd_la_SOURCES =   \
	ircd.cc            \
	exception.cc       \
	rfc1459.cc         \
	stringops.cc       \
	match.cc           \
	getopt.cc          \
	logger.cc          \
	fs.cc              \
	conf.cc            \
	newconf.cc         \
	modules.cc         \
	snomask.cc         \
	info.cc            \
	cmds.cc            \
	ctx.cc             \
	client.cc          \
	vm.cc

#authproc.cc			\
#bandbi.cc                      \
#cache.cc                       \
#cache_serv.cc                       \
#capability.cc			\
#channel.cc                     \
#client.cc \
#client_mode.cc \
#chmode.cc                      \
#class.cc                       \
#dns.cc				\
#extban.cc                      \
#hash.cc                        \
#hook.cc                        \
#mask.cc                        \
#ircd.cc                       \
#listener.cc                   \
#monitor.cc                     \
#msgbuf.cc			\
#operhash.cc                    \
#packet.cc                      \
#parse.cc                       \
#privilege.cc                   \
#ratelimit.cc			\
#reject.cc                      \
#restart.cc                     \
#s_conf.cc                      \
#s_newconf.cc                   \
#s_serv.cc                      \
#s_user.cc                      \
#sslproc.cc                     \
#substitution.cc                \
#supported.cc                   \
#tgchange.cc                    \
#whowas.cc			\
#wsproc.cc

install-data-local:
	test -d $(prefix)/@logdir@ || mkdir -p $(prefix)/@logdir@

clean-local:
	rm -f ircd_lexer.c ircd_lexer.cc
	rm -f ircd_parser.c ircd_parser.cc
	rm -f ircd_parser.h ircd_parser.hh
	rm -f stack.hh
