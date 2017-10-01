AUTOMAKE_OPTIONS = foreign

libircddir	= @libdir@

AM_CXXFLAGS = \
	###

AM_CPPFLAGS = \
	-ffriend-injection \
	-I$(top_srcdir)/include \
	@ROCKSDB_CPPFLAGS@ \
	@JS_CPPFLAGS@ \
	@BOOST_CPPFLAGS@ \
	@SODIUM_CPPFLAGS@ \
	-include ircd/ircd.h \
	###

if MINGW
PLATFORM_LDFLAGS = -Wl,--enable-runtime-pseudo-reloc -export-symbols-regex '*'
endif

if DEBUG
if GCC
AM_CXXFLAGS += -fmax-errors=2
endif
endif

AM_LDFLAGS = \
	-avoid-version \
	-Wl,-fuse-ld=gold \
	-Wl,--no-allow-shlib-undefined \
	-Wl,--export-dynamic \
	-Wl,--no-gnu-unique \
	-Wl,-z -Wl,now \
	$(PLATFORM_LDFLAGS) \
	# -Wl,--no-undefined \
	###

libircd_LTLIBRARIES = libircd.la

libircd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	@ROCKSDB_LDFLAGS@ \
	@JS_LDFLAGS@ \
	@BOOST_LDFLAGS@ \
	@SODIUM_LDFLAGS@ \
	###

libircd_la_LIBADD = \
	@ROCKSDB_LIBS@ \
	@JS_LIBS@ \
	@BOOST_LIBS@ \
	@SODIUM_LIBS@ \
	-lcrypto \
	-lssl \
	-lz \
	###

libircd_la_SOURCES =   \
	client.cc          \
	crypto.cc          \
	ctx.cc             \
	db.cc              \
	exception.cc       \
	fmt.cc             \
	fs.cc              \
	http.cc            \
	info.cc            \
	ircd.cc            \
	json.cc            \
	lexical.cc         \
	locale.cc          \
	logger.cc          \
	matrix.cc          \
	mods.cc            \
	net.cc             \
	openssl.cc         \
	parse.cc           \
	resource.cc        \
	rfc1459.cc         \
	sodium.cc          \
	###

if JS
libircd_la_SOURCES +=  \
	js.cc              \
	xdr.cc             \
	###
endif

install-data-local:
	test -d $(prefix)/@logdir@ || mkdir -p $(prefix)/@logdir@
