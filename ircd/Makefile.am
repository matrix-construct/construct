AUTOMAKE_OPTIONS = foreign

libircddir	= @libdir@

AM_CXXFLAGS = \
	-ftls-model=initial-exec \
	@EXTRA_CXXFLAGS@ \
	###

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@ROCKSDB_CPPFLAGS@ \
	@JS_CPPFLAGS@ \
	@BOOST_CPPFLAGS@ \
	@SSL_CPPFLAGS@ \
	@CRYPTO_CPPFLAGS@ \
	@SODIUM_CPPFLAGS@ \
	@MAGIC_CPPFLAGS@ \
	@SNAPPY_CPPFLAGS@ \
	@LZ4_CPPFLAGS@ \
	@Z_CPPFLAGS@ \
	-include ircd/ircd.pic.h \
	@EXTRA_CPPFLAGS@ \
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
	-Wl,--warn-common \
	-Wl,--warn-execstack \
	-Wl,--detect-odr-violations \
	-Wl,--unresolved-symbols=report-all \
	-Wl,--no-undefined-version \
	-Wl,--no-undefined \
	-Wl,--no-gnu-unique \
	-Wl,--export-dynamic \
	-Wl,--gc-sections \
	-Wl,--icf=all \
	-Wl,--rosegment \
	-Wl,--whole-archive \
	-Wl,-z -Wl,initfirst \
	-Wl,-z -Wl,nodelete \
	-Wl,-z -Wl,nodlopen \
	-Wl,-z -Wl,noexecstack \
	-Wl,-z -Wl,now \
	-Wl,-z -Wl,combreloc \
	-Wl,-z -Wl,nocopyreloc \
	$(PLATFORM_LDFLAGS) \
	@EXTRA_LDFLAGS@ \
	###

libircd_LTLIBRARIES = libircd.la

libircd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	@ROCKSDB_LDFLAGS@ \
	@JS_LDFLAGS@ \
	@BOOST_LDFLAGS@ \
	@SSL_LDFLAGS@ \
	@CRYPTO_LDFLAGS@ \
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
	@SSL_LIBS@ \
	@CRYPTO_LIBS@ \
	@SODIUM_LIBS@ \
	@MAGIC_LIBS@ \
	@SNAPPY_LIBS@ \
	@LZ4_LIBS@ \
	@Z_LIBS@ \
	@EXTRA_LIBS@ \
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
	info.cc            \
	allocator.cc       \
	exception.cc       \
	util.cc            \
	ios.cc             \
	lexical.cc         \
	tokens.cc          \
	json.cc            \
	locale.cc          \
	conf.cc            \
	logger.cc          \
	sodium.cc          \
	rfc1459.cc         \
	rand.cc            \
	crh.cc             \
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
	db_port.cc         \
	db_env.cc          \
	db.cc              \
	net.cc             \
	http.cc            \
	server.cc          \
	client.cc          \
	resource.cc        \
	m_name.cc          \
	m_id.cc            \
	m_event.cc         \
	m_dbs.cc           \
	m_state.cc         \
	m_room.cc          \
	m_v1.cc            \
	m.cc               \
	ircd.cc            \
	###

if AIO
libircd_la_SOURCES +=  \
	fs_aio.cc          \
	###
endif

if JS
libircd_la_SOURCES +=  \
	js.cc              \
	###
endif
