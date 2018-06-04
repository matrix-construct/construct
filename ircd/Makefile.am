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
	@MAGIC_CPPFLAGS@ \
	@SNAPPY_CPPFLAGS@ \
	@LZ4_CPPFLAGS@ \
	@Z_CPPFLAGS@ \
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
	-version-info 3:1:0 \
	-Wl,-fuse-ld=gold \
	-Wl,--detect-odr-violations \
	-Wl,--export-dynamic \
	-Wl,--no-undefined \
	-Wl,--no-gnu-unique \
	-Wl,--gc-sections \
	-Wl,--icf=all \
	-Wl,--rosegment \
	-Wl,-z -Wl,initfirst \
	-Wl,-z -Wl,nodelete \
	-Wl,-z -Wl,nodlopen \
	-Wl,-z -Wl,now \
	$(PLATFORM_LDFLAGS) \
	###

libircd_LTLIBRARIES = libircd.la

libircd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	@ROCKSDB_LDFLAGS@ \
	@JS_LDFLAGS@ \
	@BOOST_LDFLAGS@ \
	@SODIUM_LDFLAGS@ \
	@MAGIC_LDFLAGS@ \
	@SNAPPY_LDFLAGS@ \
	@LZ4_LDFLAGS@ \
	@Z_LDFLAGS@ \
	###

libircd_la_LIBADD = \
	@ROCKSDB_LIBS@ \
	@JS_LIBS@ \
	@BOOST_LIBS@ \
	@SODIUM_LIBS@ \
	@MAGIC_LIBS@ \
	-lcrypto \
	-lssl \
	@SNAPPY_LIBS@ \
	@LZ4_LIBS@ \
	@Z_LIBS@ \
	###

# Since this is a GNU C++ project we assume the non-standard respect for
# the order of this sources list. Static initialization will occur in
# the same order as this list, and destruction in the reverse order.
#
# Add the most dependent units at the end and the least dependent units
# at the beginning.
#
# Note: Some exceptions to the order have been made for now to diffuse
# the units that compile spirit grammars otherwise they thrash weaker
# systems.
libircd_la_SOURCES =   \
	allocator.cc       \
	exception.cc       \
	lexical.cc         \
	tokens.cc          \
	json.cc            \
	locale.cc          \
	logger.cc          \
	info.cc            \
	sodium.cc          \
	conf.cc            \
	rfc1459.cc         \
	rand.cc            \
	hash.cc            \
	base.cc            \
	parse.cc           \
	openssl.cc         \
	magic.cc           \
	fs.cc              \
	ctx.cc             \
	rfc3986.cc         \
	rfc1035.cc         \
	demangle.cc        \
	mods.cc            \
	fmt.cc             \
	db.cc              \
	net.cc             \
	http.cc            \
	server.cc          \
	client.cc          \
	resource.cc        \
	m/id.cc            \
	m/event.cc         \
	m/dbs.cc           \
	m/state.cc         \
	m/room.cc          \
	m/request.cc       \
	m/v1.cc            \
	m/m.cc             \
	ircd.cc            \
	###

if AIO
libircd_la_SOURCES +=  \
	aio.cc             \
	###
endif

if JS
libircd_la_SOURCES +=  \
	js.cc              \
	xdr.cc             \
	###
endif

install-data-local:
	test -d $(prefix)/@logdir@ || mkdir -p $(prefix)/@logdir@
