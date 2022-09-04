AUTOMAKE_OPTIONS = foreign

BUILT_SOURCES =#
CLEANFILES =#

DEFS += -DIRCD_UNIT

AM_CPPFLAGS =#
AM_CPPFLAGS += -I$(top_srcdir)/include

AM_CXXFLAGS =#
AM_CXXFLAGS += -ftls-model=initial-exec

AM_LDFLAGS =#
AM_LDFLAGS += -version-info 3:2:0
AM_LDFLAGS += -Wl,--no-undefined-version
AM_LDFLAGS += -Wl,--icf=all
AM_LDFLAGS += -Wl,--sort-section=name
AM_LDFLAGS += -Wl,--unresolved-symbols=ignore-in-shared-libs
AM_LDFLAGS += -Wl,--wrap=pthread_create
AM_LDFLAGS += -Wl,--wrap=pthread_join
AM_LDFLAGS += -Wl,--wrap=pthread_tryjoin_np
AM_LDFLAGS += -Wl,--wrap=pthread_timedjoin_np
AM_LDFLAGS += -Wl,--wrap=pthread_clockjoin_np
AM_LDFLAGS += -Wl,--wrap=pthread_self
AM_LDFLAGS += -Wl,--wrap=pthread_setname_np
AM_LDFLAGS += -Wl,--wrap=_dl_signal_exception
AM_LDFLAGS += -Wl,-z,nodelete
AM_LDFLAGS += -Wl,-z,nodlopen
AM_LDFLAGS += -Wl,-z,lazy
AM_LDFLAGS += $(PLATFORM_LDFLAGS)

if CLANG_PCH
AM_CPPFLAGS += -include-pch $(top_srcdir)/include/ircd/ircd.pic.h.gch
else
AM_CPPFLAGS += -include ircd/ircd.pic.h
endif

if CLANG
if !CLANG_PCH
AM_CXXFLAGS += -fno-c++-static-destructors
endif
endif

if LTO
if GCC
AM_CXXFLAGS += -flto=auto
endif
endif

if LTO
if CLANG
AM_CXXFLAGS += -flto=thin
AM_CXXFLAGS += -fstrict-vtable-pointers
AM_CXXFLAGS += -fwhole-program-vtables
#AM_LDFLAGS += -Wl,-plugin-opt,-pass-remarks='.*'
endif
endif

if CLANG
if PROFILE_USE
AM_CXXFLAGS += -fprofile-use
BUILT_SOURCES += default.profdata
CLEANFILES += default.profdata
endif
endif

if CLANG
if PROFILE
AM_CXXFLAGS += -fprofile-generate
AM_LDFLAGS += -Wl,-fprofile-generate
#TODO: XXX
#AM_CXXFLAGS += -fprofile-instr-generate
#AM_CXXFLAGS += -fcoverage-mapping
#AM_LDFLAGS += -Wl,-fprofile-instr-generate
#AM_LDFLAGS += -Wl,-fcoverage-mapping
endif
endif

if GOLD
AM_LDFLAGS += -Wl,--no-text-reorder
AM_LDFLAGS += -Wl,--weak-unresolved-symbols
AM_LDFLAGS += -Wl,--sort-common=descending
endif

if MINGW
AM_LDFLAGS += -Wl,--enable-runtime-pseudo-reloc
AM_LDFLAGS += -export-symbols-regex '*'
endif

AM_CXXFLAGS += $(EXTRA_CXXFLAGS)
AM_CPPFLAGS += $(EXTRA_CPPFLAGS)
AM_LDFLAGS += $(EXTRA_LDFLAGS)

libircddir = @libdir@
libircd_LTLIBRARIES =#
libircd_LTLIBRARIES += libircd.la

libircd_la_LDFLAGS =#
libircd_la_LDFLAGS += $(AM_LDFLAGS)
libircd_la_LDFLAGS += @ROCKSDB_LDFLAGS@
libircd_la_LDFLAGS += @JS_LDFLAGS@
libircd_la_LDFLAGS += @ICU_LDFLAGS@
libircd_la_LDFLAGS += @BOOST_LDFLAGS@
libircd_la_LDFLAGS += @OPENCL_LDFLAGS@
libircd_la_LDFLAGS += @SSL_LDFLAGS@
libircd_la_LDFLAGS += @PBC_LDFLAGS@
libircd_la_LDFLAGS += @CRYPTO_LDFLAGS@
libircd_la_LDFLAGS += @SODIUM_LDFLAGS@
libircd_la_LDFLAGS += @MAGIC_LDFLAGS@
libircd_la_LDFLAGS += @IMAGEMAGICK_LDFLAGS@
libircd_la_LDFLAGS += @PNG_LDFLAGS@
libircd_la_LDFLAGS += @ZSTD_LDFLAGS@
libircd_la_LDFLAGS += @SNAPPY_LDFLAGS@
libircd_la_LDFLAGS += @LZ4_LDFLAGS@
libircd_la_LDFLAGS += @Z_LDFLAGS@
libircd_la_LDFLAGS += @MALLOC_LDFLAGS@

libircd_la_LIBADD =#
libircd_la_LIBADD += @ROCKSDB_LIBS@
libircd_la_LIBADD += @JS_LIBS@
libircd_la_LIBADD += @ICU_LIBS@
libircd_la_LIBADD += @BOOST_LIBS@
libircd_la_LIBADD += @PBC_LIBS@
libircd_la_LIBADD += @SSL_LIBS@
libircd_la_LIBADD += @CRYPTO_LIBS@
libircd_la_LIBADD += @SODIUM_LIBS@
libircd_la_LIBADD += @MAGIC_LIBS@
libircd_la_LIBADD += @IMAGEMAGICK_LIBS@
libircd_la_LIBADD += @PNG_LIBS@
libircd_la_LIBADD += @ZSTD_LIBS@
libircd_la_LIBADD += @SNAPPY_LIBS@
libircd_la_LIBADD += @LZ4_LIBS@
libircd_la_LIBADD += @Z_LIBS@
libircd_la_LIBADD += $(EXTRA_LIBS)

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
libircd_la_SOURCES += versions.cc
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
libircd_la_SOURCES += beep.cc
libircd_la_SOURCES += png.cc
if OPENCL
libircd_la_SOURCES += cl.cc
endif
libircd_la_SOURCES += gpt_vocab.cc
libircd_la_SOURCES += gpt_model.cc
libircd_la_SOURCES += gpt_pipe_code.cc
libircd_la_SOURCES += gpt_pipe.cc
libircd_la_SOURCES += gpt_cpu.cc
libircd_la_SOURCES += gpt.cc
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
if LINUX
libircd_la_SOURCES += net_bpf.cc
endif
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

ROCKSDB_SRC_CPPFLAGS =#
ROCKSDB_SRC_CPPFLAGS += -isystem $(top_srcdir)/deps/rocksdb/include
ROCKSDB_SRC_CPPFLAGS += -isystem $(top_srcdir)/deps/rocksdb

GPT_FP_CXXFLAGS =#
#TODO: XXX
if !CLANG_PCH
GPT_FP_CXXFLAGS += -fno-math-errno
GPT_FP_CXXFLAGS += -fno-trapping-math
GPT_FP_CXXFLAGS += -ffinite-math-only
GPT_FP_CXXFLAGS += -fno-signed-zeros
GPT_FP_CXXFLAGS += -fassociative-math
GPT_FP_CXXFLAGS += -freciprocal-math
GPT_FP_CXXFLAGS += -ffp-contract=fast
if CLANG
GPT_FP_CXXFLAGS += -fdenormal-fp-math=positive-zero
GPT_FP_CXXFLAGS += -ffp-model=fast
endif
endif

if CLANG_PCH
ASIO_UNIT_CPPFLAGS += -include-pch $(top_srcdir)/include/ircd/asio.h.gch
SPIRIT_UNIT_CPPFLAGS += -include-pch $(top_srcdir)/include/ircd/spirit.h.gch
else
ASIO_UNIT_CPPFLAGS += -include ircd/asio.h
SPIRIT_UNIT_CPPFLAGS += -include ircd/spirit.h
endif

client.lo:            AM_CPPFLAGS := ${AM_CPPFLAGS} ${ASIO_UNIT_CPPFLAGS}
ctx_x86_64.lo:        AM_CPPFLAGS := -I$(top_srcdir)/include
ctx.lo:               AM_CPPFLAGS := ${AM_CPPFLAGS} ${ASIO_UNIT_CPPFLAGS}
ctx_ole.lo:           AM_CPPFLAGS := ${AM_CPPFLAGS} ${ASIO_UNIT_CPPFLAGS}
ctx_eh.lo:            AM_CPPFLAGS := ${AM_CPPFLAGS} ${ASIO_UNIT_CPPFLAGS}
db.lo:                AM_CPPFLAGS := ${AM_CPPFLAGS} ${ROCKSDB_UNIT_CPPFLAGS}
db_allocator.lo:      AM_CPPFLAGS := ${AM_CPPFLAGS} ${ROCKSDB_UNIT_CPPFLAGS}
db_database.lo:       AM_CPPFLAGS := ${AM_CPPFLAGS} ${ROCKSDB_UNIT_CPPFLAGS}
db_env.lo:            AM_CPPFLAGS := ${AM_CPPFLAGS} ${ROCKSDB_UNIT_CPPFLAGS}
db_fixes.lo:          AM_CPPFLAGS := ${AM_CPPFLAGS} ${ROCKSDB_UNIT_CPPFLAGS}
db_fixes.lo:          AM_CPPFLAGS += ${ROCKSDB_SRC_CPPFLAGS}
db_port.lo:           AM_CPPFLAGS := ${AM_CPPFLAGS} ${ROCKSDB_UNIT_CPPFLAGS}
exception.lo:         AM_CPPFLAGS := ${AM_CPPFLAGS} ${ASIO_UNIT_CPPFLAGS}
exec.lo:              AM_CPPFLAGS := ${AM_CPPFLAGS} ${ASIO_UNIT_CPPFLAGS}
fmt.lo:               AM_CPPFLAGS := ${AM_CPPFLAGS} ${SPIRIT_UNIT_CPPFLAGS}
fmt.lo:               AM_CXXFLAGS := ${AM_CXXFLAGS} ${SPIRIT_UNIT_CXXFLAGS}
fs.lo:                AM_CPPFLAGS := ${AM_CPPFLAGS} ${ASIO_UNIT_CPPFLAGS}
fs_path.lo:           AM_CPPFLAGS := ${AM_CPPFLAGS} ${ASIO_UNIT_CPPFLAGS}
if AIO
fs_aio.lo:            AM_CPPFLAGS := ${AM_CPPFLAGS} ${ASIO_UNIT_CPPFLAGS}
endif
gpt_cpu.lo:           AM_CXXFLAGS := ${AM_CXXFLAGS} ${GPT_FP_CXXFLAGS}
http.lo:              AM_CPPFLAGS := ${AM_CPPFLAGS} ${SPIRIT_UNIT_CPPFLAGS}
http.lo:              AM_CXXFLAGS := ${AM_CXXFLAGS} ${SPIRIT_UNIT_CXXFLAGS}
ios.lo:               AM_CPPFLAGS := ${AM_CPPFLAGS} ${ASIO_UNIT_CPPFLAGS}
if JS
js.lo:                AM_CPPFLAGS := @JS_CPPFLAGS@ ${AM_CPPFLAGS}
endif
json.lo:              AM_CPPFLAGS := ${AM_CPPFLAGS} ${SPIRIT_UNIT_CPPFLAGS}
json.lo:              AM_CXXFLAGS := ${AM_CXXFLAGS} ${SPIRIT_UNIT_CXXFLAGS}
lex_cast.lo:          AM_CPPFLAGS := ${AM_CPPFLAGS} ${SPIRIT_UNIT_CPPFLAGS}
lex_cast.lo:          AM_CXXFLAGS := ${AM_CXXFLAGS} ${SPIRIT_UNIT_CXXFLAGS}
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
mods.lo:              AM_CPPFLAGS := ${AM_CPPFLAGS} ${ASIO_UNIT_CPPFLAGS}
net.lo:               AM_CPPFLAGS := ${AM_CPPFLAGS} ${ASIO_UNIT_CPPFLAGS}
net_addrs.lo:         AM_CPPFLAGS := ${AM_CPPFLAGS} ${ASIO_UNIT_CPPFLAGS}
net_dns.lo:           AM_CPPFLAGS := ${AM_CPPFLAGS} ${ASIO_UNIT_CPPFLAGS}
net_dns_resolver.lo:  AM_CPPFLAGS := ${AM_CPPFLAGS} ${ASIO_UNIT_CPPFLAGS}
net_listener.lo:      AM_CPPFLAGS := ${AM_CPPFLAGS} ${ASIO_UNIT_CPPFLAGS}
net_listener_udp.lo:  AM_CPPFLAGS := ${AM_CPPFLAGS} ${ASIO_UNIT_CPPFLAGS}
openssl.lo:           AM_CPPFLAGS := @SSL_CPPFLAGS@ @CRYPTO_CPPFLAGS@ ${AM_CPPFLAGS}
parse.lo:             AM_CPPFLAGS := ${AM_CPPFLAGS} ${SPIRIT_UNIT_CPPFLAGS}
parse.lo:             AM_CXXFLAGS := ${AM_CXXFLAGS} ${SPIRIT_UNIT_CXXFLAGS}
if PBC
pbc.lo:               AM_CPPFLAGS := @PBC_CPPFLAGS@ ${AM_CPPFLAGS}
endif
if PNG
png.o:                AM_CPPFLAGS := @PNG_CPPFLAGS@ ${AM_CPPFLAGS}
endif PNG
rfc1459.lo:           AM_CPPFLAGS := ${AM_CPPFLAGS} ${SPIRIT_UNIT_CPPFLAGS}
rfc1459.lo:           AM_CXXFLAGS := ${AM_CXXFLAGS} ${SPIRIT_UNIT_CXXFLAGS}
rfc3986.lo:           AM_CPPFLAGS := ${AM_CPPFLAGS} ${SPIRIT_UNIT_CPPFLAGS}
rfc3986.lo:           AM_CXXFLAGS := ${AM_CXXFLAGS} ${SPIRIT_UNIT_CXXFLAGS}
server.lo:            AM_CPPFLAGS := ${AM_CPPFLAGS} ${ASIO_UNIT_CPPFLAGS}
if SODIUM
sodium.lo:            AM_CPPFLAGS := @SODIUM_CPPFLAGS@ ${AM_CPPFLAGS}
endif

#
# Other recipes
#

# LLVM PGO text to binary for -fprofile-use
if CLANG
default.profdata:
	-$(LLVM_PROFDATA) merge -output=default.profdata default.proftext
endif

###############################################################################
#
# Hardware Acceleration / sub-targets
#

if CLANG
if OPENCL

GPU_CPPFLAGS =#
GPU_CPPFLAGS += -D__OPENCL_VERSION__=110
GPU_CPPFLAGS += -Dcl_clang_storage_class_specifiers
GPU_CPPFLAGS += -DNOFP64
GPU_CPPFLAGS += -I$(top_srcdir)/include
GPU_CPPFLAGS += -include "ircd/config.h"
GPU_CPPFLAGS += $(CPPFLAGS)
GPU_CPPFLAGS += $(CWARNS)
GPU_CPPFLAGS += $(DEFS)

GPU_CFLAGS =#
GPU_CFLAGS += -std=cl1.1
GPU_CFLAGS += -fident
GPU_CFLAGS += -fno-builtin
GPU_CFLAGS += -fverbose-asm
GPU_CFLAGS += -fno-discard-value-names
GPU_CFLAGS += -mllvm -propagate-attrs=true
GPU_CFLAGS += -O3
GPU_CFLAGS += -fno-trapping-math
GPU_CFLAGS += -ffinite-math-only
GPU_CFLAGS += -fno-signed-zeros
GPU_CFLAGS += -ffp-contract=fast
#GPU_CFLAGS += -include "ircd/config.h"
#GPU_CFLAGS += -include "clc/clc.h"
#GPU_CFLAGS += -Wassume
#GPU_CFLAGS += -Rpass-analysis=".*"
#GPU_CFLAGS += -Rpass-missed=".*"

GPU_LINKFLAGS =#
GPU_LINKFLAGS +=#

GPU_OPTFLAGS =#
GPU_OPTFLAGS += -propagate-attrs=true
GPU_OPTFLAGS += -O3
#GPU_OPTFLAGS += -opt-bisect-limit=-1
#GPU_OPTFLAGS += -debug-pass=Arguments
#GPU_OPTFLAGS += -pass-remarks='.*'

GPU_ASFLAGS =#
GPU_ASFLAGS += -fident
GPU_ASFLAGS += -fno-builtin
GPU_ASFLAGS += -mllvm -propagate-attrs=true
GPU_ASFLAGS += -mllvm -verify-machineinstrs
GPU_ASFLAGS += -O3
GPU_ASFLAGS += -Rpass-analysis=asm-printer
#GPU_ASFLAGS += -Rpass-analysis=".*"
#GPU_ASFLAGS += -mllvm -debug-pass=Arguments
#GPU_ASFLAGS += -mllvm -pass-remarks='.*'

#
# SPV
#

BUILT_SOURCES += gpt_gpu.spv.bc
gpt_gpu.spv.bc: gpt_gpu.cl
	$(CC) -target spir-- $(GPU_CPPFLAGS) -O0 -emit-llvm -o $@ -x cl -c $^

BUILT_SOURCES += gpt_gpu.spv
gpt_gpu.spv: gpt_gpu.spv.bc
	llvm-spirv -o $@ $^

CLEANFILES += gpt_gpu.spv.cc
libircd_la_SOURCES += gpt_gpu.spv.cc
gpt_gpu.spv.cc: gpt_gpu.spv
	xxd -i $^ $@

#
# R600
#

R600_TARGET = r600--

#
# R600 Saint Barthélemy
#

R600_BARTS_CFLAGS = $(GPU_CFLAGS)
R600_BARTS_CFLAGS += -target $(R600_TARGET)
R600_BARTS_CFLAGS += -mcpu=barts
R600_BARTS_CFLAGS += -Xclang -mlink-bitcode-file -Xclang /usr/lib/clc/barts-r600--.bc

BUILT_SOURCES += gpt_gpu.r600_barts.bc
gpt_gpu.r600_barts.bc: gpt_gpu.cl
	$(CC) $(R600_BARTS_CFLAGS) $(GPU_CPPFLAGS) -emit-llvm -o $@ -x cl -c $^

#BUILT_SOURCES += gpt_gpu.r600_barts.link.bc
gpt_gpu.r600_barts.link.bc: gpt_gpu.r600_barts.bc
	llvm-link-14 $(GPU_LINKFLAGS) -o $@ $^ /usr/lib/clc/barts-r600--.bc
	opt-14 $(GPU_OPTFLAGS) -o $@ $@

#BUILT_SOURCES += gpt_gpu.r600_barts.s
gpt_gpu.r600_barts.s: gpt_gpu.r600_barts.link.bc
	$(CC) -cc1 $(GPU_ASFLAGS) -triple $(R600_TARGET) -emit-obj -S -o $@ -x ir $^

#BUILT_SOURCES += gpt_gpu.r600_barts.o
gpt_gpu.r600_barts.o: gpt_gpu.r600_barts.link.bc
	$(CC) -cc1 $(GPU_ASFLAGS) -triple $(R600_TARGET) -emit-obj -o $@ -x ir $^

CLEANFILES += gpt_gpu.r600_barts.bc.cc
libircd_la_SOURCES += gpt_gpu.r600_barts.bc.cc
gpt_gpu.r600_barts.bc.cc: gpt_gpu.r600_barts.bc
	xxd -i $^ $@

#
#
#

endif # OPENCL
endif # CLANG
