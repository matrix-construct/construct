AUTOMAKE_OPTIONS = foreign

BUILT_SOURCES =#
CLEANFILES =#

DEFS += \
	-DIRCD_UNIT \
	###

CXXFLAGS += \
	$(EXTRA_CXXFLAGS) \
	###

AM_CXXFLAGS = \
	-ftls-model=initial-exec \
	-include ircd/ircd.pic.h \
	###

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	$(EXTRA_CPPFLAGS) \
	###

AM_LDFLAGS = \
	-version-info 3:2:0 \
	-Wl,--no-undefined-version \
	-Wl,--weak-unresolved-symbols \
	-Wl,--unresolved-symbols=ignore-in-shared-libs \
	-Wl,--wrap=pthread_create \
	-Wl,--wrap=pthread_join \
	-Wl,--wrap=pthread_tryjoin_np \
	-Wl,--wrap=pthread_timedjoin_np \
	-Wl,--wrap=pthread_clockjoin_np \
	-Wl,--wrap=pthread_self \
	-Wl,--wrap=pthread_setname_np \
	-Wl,-z,nodelete \
	-Wl,-z,nodlopen \
	-Wl,-z,lazy \
	$(PLATFORM_LDFLAGS) \
	$(EXTRA_LDFLAGS) \
	###

if CLANG
AM_CXXFLAGS += -fno-c++-static-destructors
endif

if LTO
AM_CXXFLAGS += -flto=thin
endif

if LTO
if CLANG
AM_CXXFLAGS += -fstrict-vtable-pointers
AM_CXXFLAGS += -fwhole-program-vtables
endif
endif

if !PROFILE
if CLANG
AM_CXXFLAGS += -fprofile-use
BUILT_SOURCES += default.profdata
CLEANFILES += default.profdata
endif
endif

if PROFILE
if CLANG
AM_CXXFLAGS += -fprofile-generate
AM_LDFLAGS += -Wl,-fprofile-generate
#TODO: XXX
#AM_CXXFLAGS += -fprofile-instr-generate
#AM_CXXFLAGS += -fcoverage-mapping
#AM_LDFLAGS += -Wl,-fprofile-instr-generate
#AM_LDFLAGS += -Wl,-fcoverage-mapping
endif
endif

if MINGW
AM_LDFLAGS += -Wl,--enable-runtime-pseudo-reloc
AM_LDFLAGS += -export-symbols-regex '*'
endif

ROCKSDB_SRC_CPPFLAGS =#
ROCKSDB_SRC_CPPFLAGS += -isystem $(top_srcdir)/deps/rocksdb/include
ROCKSDB_SRC_CPPFLAGS += -isystem $(top_srcdir)/deps/rocksdb

GPT_FP_CXXFLAGS =#
GPT_FP_CXXFLAGS += -fno-math-errno
GPT_FP_CXXFLAGS += -fno-trapping-math
GPT_FP_CXXFLAGS += -ffinite-math-only
GPT_FP_CXXFLAGS += -fno-signed-zeros
GPT_FP_CXXFLAGS += -fassociative-math
GPT_FP_CXXFLAGS += -ffp-contract=fast
GPT_FP_CXXFLAGS += -freciprocal-math
if CLANG
GPT_FP_CXXFLAGS += -fdenormal-fp-math=positive-zero
endif

libircddir = @libdir@
libircd_LTLIBRARIES = libircd.la

libircd_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	@ROCKSDB_LDFLAGS@ \
	@JS_LDFLAGS@ \
	@ICU_LDFLAGS@ \
	@BOOST_LDFLAGS@ \
	@OPENCL_LDFLAGS@ \
	@SSL_LDFLAGS@ \
	@PBC_LDFLAGS@ \
	@CRYPTO_LDFLAGS@ \
	@SODIUM_LDFLAGS@ \
	@MAGIC_LDFLAGS@ \
	@IMAGEMAGICK_LDFLAGS@ \
	@PNG_LDFLAGS@ \
	@ZSTD_LDFLAGS@ \
	@SNAPPY_LDFLAGS@ \
	@LZ4_LDFLAGS@ \
	@Z_LDFLAGS@ \
	@MALLOC_LDFLAGS@ \
	###

libircd_la_LIBADD = \
	@ROCKSDB_LIBS@ \
	@JS_LIBS@ \
	@ICU_LIBS@ \
	@BOOST_LIBS@ \
	@PBC_LIBS@ \
	@SSL_LIBS@ \
	@CRYPTO_LIBS@ \
	@SODIUM_LIBS@ \
	@MAGIC_LIBS@ \
	@IMAGEMAGICK_LIBS@ \
	@ZSTD_LIBS@ \
	@SNAPPY_LIBS@ \
	@LZ4_LIBS@ \
	@Z_LIBS@ \
	@MALLOC_LIBS@ \
	$(EXTRA_LIBS) \
	###

if PROFILE
if CLANG11
#TODO: XXX
libircd_la_LIBADD += /usr/lib/llvm-11/lib/clang/11.0.0/lib/linux/libclang_rt.profile-x86_64.a
endif
endif

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
libircd_la_SOURCES += default.profdata
libircd_la_SOURCES += assert.cc
libircd_la_SOURCES += vg.cc
libircd_la_SOURCES += exception.cc
libircd_la_SOURCES += demangle.cc
libircd_la_SOURCES += backtrace.cc
libircd_la_SOURCES += simd.cc
libircd_la_SOURCES += fpe.cc
libircd_la_SOURCES += parse.cc
libircd_la_SOURCES += lex_cast.cc
libircd_la_SOURCES += info.cc
libircd_la_SOURCES += allocator.cc
libircd_la_SOURCES += allocator_gnu.cc
libircd_la_SOURCES += allocator_je.cc
libircd_la_SOURCES += util.cc
libircd_la_SOURCES += rand.cc
libircd_la_SOURCES += fmt.cc
libircd_la_SOURCES += timedate.cc
if ICU
libircd_la_SOURCES += icu.cc
endif
libircd_la_SOURCES += utf.cc
libircd_la_SOURCES += stringops.cc
libircd_la_SOURCES += globular.cc
libircd_la_SOURCES += tokens.cc
libircd_la_SOURCES += b64.cc
libircd_la_SOURCES += b58.cc
libircd_la_SOURCES += crh.cc
libircd_la_SOURCES += json.cc
libircd_la_SOURCES += cbor.cc
libircd_la_SOURCES += conf.cc
libircd_la_SOURCES += stats.cc
libircd_la_SOURCES += logger.cc
libircd_la_SOURCES += ios.cc
libircd_la_SOURCES += run.cc
libircd_la_SOURCES += prof.cc
libircd_la_SOURCES += prof_psi.cc
if LINUX
libircd_la_SOURCES += prof_linux.cc
endif
libircd_la_SOURCES += ctx_x86_64.S
libircd_la_SOURCES += ctx.cc
libircd_la_SOURCES += ctx_eh.cc
libircd_la_SOURCES += ctx_ole.cc
libircd_la_SOURCES += ctx_posix.cc
libircd_la_SOURCES += fs.cc
libircd_la_SOURCES += fs_path.cc
libircd_la_SOURCES += fs_dev.cc
if AIO
libircd_la_SOURCES += fs_aio.cc
endif
if IOU
libircd_la_SOURCES += fs_iou.cc
endif
libircd_la_SOURCES += sys.cc
libircd_la_SOURCES += mods.cc
if LINUX
libircd_la_SOURCES += mods_ldso.cc
endif
libircd_la_SOURCES += exec.cc
if MAGIC
libircd_la_SOURCES += magic.cc
endif
if SODIUM
libircd_la_SOURCES += sodium.cc
endif
if PBC
libircd_la_SOURCES += pbc.cc
endif
if IMAGEMAGICK
libircd_la_SOURCES += magick.cc
endif
libircd_la_SOURCES += png.cc
if OPENCL
libircd_la_SOURCES += cl.cc
endif
libircd_la_SOURCES += gpt.cc
libircd_la_SOURCES += gpt_pipe.cc
libircd_la_SOURCES += gpt_model.cc
libircd_la_SOURCES += gpt_vocab.cc
if OPENCL
if CLANG
BUILT_SOURCES += gpt_cl.o
endif
endif
libircd_la_SOURCES += openssl.cc
libircd_la_SOURCES += rfc1459.cc
libircd_la_SOURCES += rfc3986.cc
libircd_la_SOURCES += rfc1035.cc
libircd_la_SOURCES += http.cc
libircd_la_SOURCES += http2.cc
libircd_la_SOURCES += db_fixes.cc
libircd_la_SOURCES += db_port.cc
libircd_la_SOURCES += db_allocator.cc
libircd_la_SOURCES += db_env.cc
libircd_la_SOURCES += db_database.cc
libircd_la_SOURCES += db.cc
libircd_la_SOURCES += net.cc
libircd_la_SOURCES += net_addrs.cc
libircd_la_SOURCES += net_dns.cc
libircd_la_SOURCES += net_dns_netdb.cc
libircd_la_SOURCES += net_dns_cache.cc
libircd_la_SOURCES += net_dns_resolver.cc
libircd_la_SOURCES += net_listener.cc
libircd_la_SOURCES += net_listener_udp.cc
libircd_la_SOURCES += server.cc
libircd_la_SOURCES += client.cc
libircd_la_SOURCES += resource.cc
if JS
libircd_la_SOURCES += js.cc
endif
libircd_la_SOURCES += ircd.cc

#
# Specific unit option composition
#

client.lo:            AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
ctx_x86_64.lo:        AM_CPPFLAGS := -I$(top_srcdir)/include
ctx.lo:               AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
ctx_ole.lo:           AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
ctx_eh.lo:            AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
db.lo:                AM_CPPFLAGS := ${ROCKSDB_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
db_allocator.lo:      AM_CPPFLAGS := ${ROCKSDB_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
db_database.lo:       AM_CPPFLAGS := ${ROCKSDB_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
db_env.lo:            AM_CPPFLAGS := ${ROCKSDB_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
db_fixes.lo:          AM_CPPFLAGS := ${ROCKSDB_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
db_fixes.lo:          AM_CPPFLAGS += ${ROCKSDB_SRC_CPPFLAGS}
db_port.lo:           AM_CPPFLAGS := ${ROCKSDB_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
exception.lo:         AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
exec.lo:              AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
fmt.lo:               AM_CPPFLAGS := ${SPIRIT_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
fmt.lo:               AM_CXXFLAGS := ${SPIRIT_UNIT_CXXFLAGS} ${AM_CXXFLAGS}
fs.lo:                AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
fs_path.lo:           AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
if AIO
fs_aio.lo:            AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
endif
if IOU
fs_iou.lo:            AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
endif
gpt.lo:               AM_CXXFLAGS := ${AM_CXXFLAGS} ${GPT_FP_CXXFLAGS}
http.lo:              AM_CPPFLAGS := ${SPIRIT_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
http.lo:              AM_CXXFLAGS := ${SPIRIT_UNIT_CXXFLAGS} ${AM_CXXFLAGS}
ios.lo:               AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
if JS
js.lo:                AM_CPPFLAGS := @JS_CPPFLAGS@ ${AM_CPPFLAGS}
endif
json.lo:              AM_CPPFLAGS := ${SPIRIT_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
json.lo:              AM_CXXFLAGS := ${SPIRIT_UNIT_CXXFLAGS} ${AM_CXXFLAGS}
lex_cast.lo:          AM_CPPFLAGS := ${SPIRIT_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
lex_cast.lo:          AM_CXXFLAGS := ${SPIRIT_UNIT_CXXFLAGS} ${AM_CXXFLAGS}
if OPENCL
cl.lo:                AM_CPPFLAGS := @OPENCL_CPPFLAGS@ ${AM_CPPFLAGS}
endif
tokens.lo:            AM_CPPFLAGS := @BOOST_CPPFLAGS@ ${AM_CPPFLAGS}
prof.lo:              AM_CPPFLAGS := @BOOST_CPPFLAGS@ ${AM_CPPFLAGS}
if MAGIC
magic.lo:             AM_CPPFLAGS := @MAGIC_CPPFLAGS@ ${AM_CPPFLAGS}
endif
if IMAGEMAGICK
magick.lo:            AM_CPPFLAGS := @IMAGEMAGICK_CPPFLAGS@ ${AM_CPPFLAGS}
endif IMAGEMAGICK
mods.lo:              AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
net.lo:               AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
net_addrs.lo:         AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
net_dns.lo:           AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
net_dns_resolver.lo:  AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
net_listener.lo:      AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
net_listener_udp.lo:  AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
openssl.lo:           AM_CPPFLAGS := @SSL_CPPFLAGS@ @CRYPTO_CPPFLAGS@ ${AM_CPPFLAGS}
parse.lo:             AM_CPPFLAGS := ${SPIRIT_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
parse.lo:             AM_CXXFLAGS := ${SPIRIT_UNIT_CXXFLAGS} ${AM_CXXFLAGS}
if PBC
pbc.lo:               AM_CPPFLAGS := @PBC_CPPFLAGS@ ${AM_CPPFLAGS}
endif
if PNG
png.o:                AM_CPPFLAGS := @PNG_CPPFLAGS@ ${AM_CPPFLAGS}
endif PNG
rfc1459.lo:           AM_CPPFLAGS := ${SPIRIT_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
rfc1459.lo:           AM_CXXFLAGS := ${SPIRIT_UNIT_CXXFLAGS} ${AM_CXXFLAGS}
rfc3986.lo:           AM_CPPFLAGS := ${SPIRIT_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
rfc3986.lo:           AM_CXXFLAGS := ${SPIRIT_UNIT_CXXFLAGS} ${AM_CXXFLAGS}
server.lo:            AM_CPPFLAGS := ${ASIO_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
if SODIUM
sodium.lo:            AM_CPPFLAGS := @SODIUM_CPPFLAGS@ ${AM_CPPFLAGS}
endif

#
# Other recipes
#

# LLVM PGO text to binary for -fprofile-use
default.profdata:
	-$(LLVM_PROFDATA) merge -output=default.profdata default.proftext

if CLANG
gpt_cl.o: gpt_cl.cl
	$(CC) -std=CL1.1 $(AM_CPPFLAGS) $(CPPFLAGS) $(DEFS) -Xclang -finclude-default-header -include "ircd/config.h" -o $@ -c $^
endif
