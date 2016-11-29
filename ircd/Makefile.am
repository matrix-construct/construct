AUTOMAKE_OPTIONS = foreign

libircddir	= @libdir@

AM_CPPFLAGS = \
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
	$(PLATFORM_LDFLAGS)

libircd_LTLIBRARIES = libircd.la

libircd_la_SOURCES =   \
	ircd.cc            \
	info.cc            \
	exception.cc       \
	rfc1459.cc         \
	stringops.cc       \
	logger.cc          \
	fs.cc              \
	mods.cc            \
	resource.cc        \
	ctx.cc             \
	socket.cc          \
	client.cc          \
	vm.cc              \
	fmt.cc             \
	db.cc              \
	locale.cc          \
	json.cc            \
	http.cc            \
	matrix.cc          \
	parse.cc

if JS
libircd_la_SOURCES +=  \
	js.cc              \
	xdr.cc
endif

install-data-local:
	test -d $(prefix)/@logdir@ || mkdir -p $(prefix)/@logdir@
