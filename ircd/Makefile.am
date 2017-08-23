AUTOMAKE_OPTIONS = foreign

libircddir	= @libdir@

AM_CPPFLAGS = \
	-fmax-errors=2 \
	-ffriend-injection \
	-I$(top_srcdir)/include \
	@ROCKSDB_CPPFLAGS@ \
	@JS_CPPFLAGS@ \
	@BOOST_CPPFLAGS@ \
	-include ircd/ircd.h

if MINGW
PLATFORM_LDFLAGS = -Wl,--enable-runtime-pseudo-reloc -export-symbols-regex '*'
endif

AM_LDFLAGS = \
	-avoid-version \
	-Wl,-fuse-ld=gold \
	-Wl,--no-allow-shlib-undefined \
	-Wl,--export-dynamic \
	-Wl,--no-gnu-unique \
	-Wl,-z -Wl,now \
	$(PLATFORM_LDFLAGS)
#	-Wl,--no-undefined

libircd_LTLIBRARIES = libircd.la

libircd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	@ROCKSDB_LDFLAGS@ \
	@JS_LDFLAGS@ \
	@BOOST_LDFLAGS@

libircd_la_LIBADD = \
	@ROCKSDB_LIBS@ \
	@JS_LIBS@ \
	@BOOST_LIBS@ \
	-lcrypto \
	-lssl \
	-lz

libircd_la_SOURCES =   \
	ircd.cc            \
	info.cc            \
	exception.cc       \
	rfc1459.cc         \
	lexical.cc         \
	logger.cc          \
	fs.cc              \
	mods.cc            \
	resource.cc        \
	ctx.cc             \
	socket.cc          \
	client.cc          \
	fmt.cc             \
	db.cc              \
	locale.cc          \
	http.cc            \
	json.cc            \
	parse.cc           \
	listen.cc          \
	matrix.cc

if JS
libircd_la_SOURCES +=  \
	js.cc              \
	xdr.cc
endif

install-data-local:
	test -d $(prefix)/@logdir@ || mkdir -p $(prefix)/@logdir@
