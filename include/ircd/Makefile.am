AUTOMAKE_OPTIONS = foreign

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
	@EXTRA_CPPFLAGS@ \
    ###

AM_CXXFLAGS = \
	@EXTRA_CXXFLAGS@ \
	###

if GCC
AM_CXXFLAGS += \
	-fpch-deps \
	###
endif

if DEBUG
if GCC
AM_CXXFLAGS += \
	-fmax-errors=2 \
	###
endif
endif

if BUILD_PCH
BUILT_SOURCES = \
	ircd.h.gch \
	ircd.pic.h.gch \
	spirit.h.gch \
	###
endif

if GCC
if LOWMEM_COMPILE
LOWMEM_FLAGS= --param ggc-min-expand=1
endif
endif

DEPS = *.h */*.h */*/*.h */*/*/*.h

nobase_include_HEADERS = $(DEPS)
noinst_HEADERS = *.gch

ircd.h.gch: $(DEPS)
	$(CXX) $(CXXFLAGS) $(AM_CXXFLAGS) -o ircd.h.gch $(DEFS) $(CPPFLAGS) $(AM_CPPFLAGS) ircd.h

ircd.pic.h.gch: $(DEPS)
	cp ircd.h ircd.pic.h
	$(CXX) $(CXXFLAGS) $(AM_CXXFLAGS) -fPIC -o ircd.pic.h.gch $(DEFS) $(CPPFLAGS) $(AM_CPPFLAGS) -DPIC ircd.pic.h

spirit.h.gch: $(DEPS)
	$(CXX) $(CXXFLAGS) $(LOWMEM_FLAGS) $(AM_CXXFLAGS) -fPIC -o spirit.h.gch $(DEFS) $(CPPFLAGS) $(AM_CPPFLAGS) -DPIC -include ircd.pic.h spirit.h

clean-local:
	rm -f ircd.h.gch
	rm -f ircd.pic.h
	rm -f ircd.pic.h.gch
	rm -f spirit.h.gch
	rm -f *.gch.tmp
