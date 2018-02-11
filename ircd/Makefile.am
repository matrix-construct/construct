AUTOMAKE_OPTIONS = foreign

libircddir	= @libdir@

AM_CXXFLAGS = \
	###

AM_CPPFLAGS = \
	-ffriend-injection \
	-I$(top_srcdir)/include \
	@ROCKSDB_CPPFLAGS@ \
	@JS_CPPFLAGS@ \
	-DBOOST_COROUTINES_NO_DEPRECATION_WARNING=1 \
	-DBOOST_COROUTINE_NO_DEPRECATION_WARNING=1 \
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
	-version-info 3:1:0 \
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

# Since this is a GNU C++ project we assume the non-standard respect for
# the order of this sources list. Static initialization will occur in
# the same order as this list, and destruction in the reverse order.
#
# Add the most dependent units at the end and the least dependent units
# at the beginning.
#
libircd_la_SOURCES =   \
	exception.cc       \
	locale.cc          \
	logger.cc          \
	info.cc            \
	sodium.cc          \
	rand.cc            \
	hash.cc            \
	lexical.cc         \
	base.cc            \
	parse.cc           \
	rfc1459.cc         \
	json.cc            \
	openssl.cc         \
	http.cc            \
	fmt.cc             \
	fs.cc              \
	ctx.cc             \
	db.cc              \
	mods.cc            \
	rfc3986.cc         \
	rfc1035.cc         \
	net.cc             \
	server.cc          \
	client.cc          \
	resource.cc        \
	m/id.cc            \
	m/event.cc         \
	m/dbs.cc           \
	m/state.cc         \
	m/room.cc          \
	m/user.cc          \
	m/filter.cc        \
	m/request.cc       \
	m/v1.cc            \
	m/vm.cc            \
	m/keys.cc          \
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
