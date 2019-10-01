AUTOMAKE_OPTIONS = foreign

DEFS += \
	-DIRCD_UNIT \
	###

AM_CXXFLAGS = \
	-ftls-model=initial-exec \
	@EXTRA_CXXFLAGS@ \
	###

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-include ircd/ircd.pic.h \
	@EXTRA_CPPFLAGS@ \
	###

AM_LDFLAGS = \
	-version-info 3:2:0 \
	-Wl,--no-undefined-version \
	-Wl,--unresolved-symbols=report-all \
	-Wl,--weak-unresolved-symbols \
	-Wl,-z,nodelete \
	-Wl,-z,nodlopen \
	-Wl,-z,now \
	$(PLATFORM_LDFLAGS) \
	@EXTRA_LDFLAGS@ \
	###

if DEBUG
if GCC
AM_CXXFLAGS += -fmax-errors=2
endif
endif

if MINGW
AM_LDFLAGS += -Wl,--enable-runtime-pseudo-reloc
AM_LDFLAGS += -export-symbols-regex '*'
endif

#
# libircd
#

libircddir = @libdir@
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
	@MALLOC_LDFLAGS@ \
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
	@MALLOC_LIBS@ \
	@EXTRA_LIBS@ \
	###

#
# Unit list
#
# Since this is a GNU C++ project we assume the non-standard respect for
# the order of this sources list. Static initialization will occur in
# the same order as this list, and destruction in the reverse order.
# Add the most dependent units at the end and the least dependent units
# at the beginning.
#

libircd_la_SOURCES =#
libircd_la_SOURCES += assert.cc
libircd_la_SOURCES += info.cc
libircd_la_SOURCES += allocator.cc
libircd_la_SOURCES += allocator_gnu.cc
libircd_la_SOURCES += allocator_je.cc
libircd_la_SOURCES += vg.cc
libircd_la_SOURCES += exception.cc
libircd_la_SOURCES += util.cc
libircd_la_SOURCES += demangle.cc
libircd_la_SOURCES += backtrace.cc
libircd_la_SOURCES += locale.cc
libircd_la_SOURCES += timedate.cc
libircd_la_SOURCES += lex_cast.cc
libircd_la_SOURCES += stringops.cc
libircd_la_SOURCES += globular.cc
libircd_la_SOURCES += tokens.cc
libircd_la_SOURCES += parse.cc
libircd_la_SOURCES += rand.cc
libircd_la_SOURCES += base.cc
libircd_la_SOURCES += crh.cc
libircd_la_SOURCES += fmt.cc
libircd_la_SOURCES += json.cc
libircd_la_SOURCES += conf.cc
libircd_la_SOURCES += stats.cc
libircd_la_SOURCES += logger.cc
if MAGIC
libircd_la_SOURCES += magic.cc
endif
if SODIUM
libircd_la_SOURCES += sodium.cc
endif
libircd_la_SOURCES += openssl.cc
libircd_la_SOURCES += rfc1459.cc
libircd_la_SOURCES += rfc3986.cc
libircd_la_SOURCES += rfc1035.cc
libircd_la_SOURCES += http.cc
libircd_la_SOURCES += http2.cc
libircd_la_SOURCES += prof.cc
if LINUX
libircd_la_SOURCES += prof_linux.cc
endif
libircd_la_SOURCES += fs.cc
libircd_la_SOURCES += ios.cc
libircd_la_SOURCES += ctx.cc
libircd_la_SOURCES += ctx_eh.cc
libircd_la_SOURCES += ctx_ole.cc
if AIO
libircd_la_SOURCES += fs_aio.cc
endif
if IOU
libircd_la_SOURCES += fs_iou.cc
endif
libircd_la_SOURCES += mods.cc
if LINUX
libircd_la_SOURCES += mods_ldso.cc
endif
libircd_la_SOURCES += db_write_thread.cc
libircd_la_SOURCES += db_port.cc
libircd_la_SOURCES += db_env.cc
libircd_la_SOURCES += db.cc
libircd_la_SOURCES += net.cc
libircd_la_SOURCES += server.cc
libircd_la_SOURCES += client.cc
libircd_la_SOURCES += resource.cc
if JS
libircd_la_SOURCES += js.cc
endif
libircd_la_SOURCES += ircd.cc

#
# Specific unit configurations
#

base.lo:              AM_CPPFLAGS := @BOOST_CPPFLAGS@ ${AM_CPPFLAGS}
client.lo:            AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
ctx.lo:               AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
ctx_ole.lo:           AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
ctx_eh.lo:            AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
db.lo:                AM_CPPFLAGS := ${ROCKSDB_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
db_env.lo:            AM_CPPFLAGS := ${ROCKSDB_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
db_port.lo:           AM_CPPFLAGS := ${ROCKSDB_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
db_write_thread.lo:   AM_CPPFLAGS := ${ROCKSDB_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
db_write_thread.lo:   AM_CPPFLAGS += -I$(top_srcdir)/deps/rocksdb/include
db_write_thread.lo:   AM_CPPFLAGS += -I$(top_srcdir)/deps/rocksdb
fmt.lo:               AM_CPPFLAGS := ${SPIRIT_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
fmt.lo:               AM_CXXFLAGS := ${SPIRIT_UNIT_CXXFLAGS} ${AM_CXXFLAGS}
fs.lo:                AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
if AIO
fs_aio.lo:            AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
endif
if IOU
fs_iou.lo:            AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
endif
http.lo:              AM_CPPFLAGS := ${SPIRIT_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
http.lo:              AM_CXXFLAGS := ${SPIRIT_UNIT_CXXFLAGS} ${AM_CXXFLAGS}
ios.lo:               AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
if JS
js.lo:                AM_CPPFLAGS := @JS_CPPFLAGS@ ${AM_CPPFLAGS}
endif
json.lo:              AM_CPPFLAGS := ${SPIRIT_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
json.lo:              AM_CXXFLAGS := ${SPIRIT_UNIT_CXXFLAGS} ${AM_CXXFLAGS}
lex_cast.lo:          AM_CPPFLAGS := @BOOST_CPPFLAGS@ ${AM_CPPFLAGS}
if MAGIC
magic.lo:             AM_CPPFLAGS := @MAGIC_CPPFLAGS@ ${AM_CPPFLAGS}
endif
mods.lo:              AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
net.lo:               AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
openssl.lo:           AM_CPPFLAGS := @SSL_CPPFLAGS@ @CRYPTO_CPPFLAGS@ ${AM_CPPFLAGS}
parse.lo:             AM_CPPFLAGS := ${SPIRIT_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
parse.lo:             AM_CXXFLAGS := ${SPIRIT_UNIT_CXXFLAGS} ${AM_CXXFLAGS}
rfc1459.lo:           AM_CPPFLAGS := ${SPIRIT_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
rfc1459.lo:           AM_CXXFLAGS := ${SPIRIT_UNIT_CXXFLAGS} ${AM_CXXFLAGS}
rfc3986.lo:           AM_CPPFLAGS := ${SPIRIT_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
rfc3986.lo:           AM_CXXFLAGS := ${SPIRIT_UNIT_CXXFLAGS} ${AM_CXXFLAGS}
server.lo:            AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
if SODIUM
sodium.lo:            AM_CPPFLAGS := @SODIUM_CPPFLAGS@ ${AM_CPPFLAGS}
endif
