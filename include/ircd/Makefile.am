AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@EXTRA_CPPFLAGS@ \
    ###

AM_CXXFLAGS = \
	-x c++-header \
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

if GCC
if LOWMEM_COMPILE
LOWMEM_FLAGS= --param ggc-min-expand=1
endif
endif

# includedir = @includedir@/$(PACKAGE_TARNAME)
includedir = @includedir@/ircd
nobase_include_HEADERS = $(DEPS)

if BUILD_PCH # --- Actions when Precompiled headers enabled

noinst_HEADERS = *.gch
BUILT_SOURCES = \
	ircd.h.gch \
	ircd.pic.h.gch \
	spirit.h.gch \
	asio.h.gch \
	matrix.h.gch \
	matrix.pic.h.gch \
	###

ircd.h.gch: ircd.h
	$(CXX) $(CXXFLAGS) $(AM_CXXFLAGS) -o ircd.h.gch $(DEFS) $(CPPFLAGS) $(AM_CPPFLAGS) ircd.h

ircd.pic.h.gch: ircd.h
	cp ircd.h ircd.pic.h
	$(CXX) $(CXXFLAGS) $(AM_CXXFLAGS) -fPIC -o ircd.pic.h.gch $(DEFS) $(CPPFLAGS) $(AM_CPPFLAGS) -DPIC ircd.pic.h

matrix.h.gch: matrix.h m/*.h m/*/*.h
	$(CXX) $(CXXFLAGS) $(AM_CXXFLAGS) -o matrix.h.gch $(DEFS) $(CPPFLAGS) $(AM_CPPFLAGS) matrix.h

matrix.pic.h.gch: matrix.h m/*.h m/*/*.h
	cp matrix.h matrix.pic.h
	$(CXX) $(CXXFLAGS) $(AM_CXXFLAGS) -fPIC -o matrix.pic.h.gch $(DEFS) $(CPPFLAGS) $(AM_CPPFLAGS) -DPIC matrix.pic.h

spirit.h.gch: ircd.h
	$(CXX) $(CXXFLAGS) $(AM_CXXFLAGS) -fPIC -o spirit.h.gch $(DEFS) $(CPPFLAGS) $(AM_CPPFLAGS) -DPIC spirit.h

spirit.h.gch: \
	AM_CXXFLAGS += @BOOST_CPPFLAGS@ $(LOWMEM_FLAGS)

asio.h.gch: ircd.h
	$(CXX) $(CXXFLAGS) $(AM_CXXFLAGS) -fPIC -o asio.h.gch $(DEFS) $(CPPFLAGS) $(AM_CPPFLAGS) -DPIC asio.h

asio.h.gch: \
	AM_CPPFLAGS += @BOOST_CPPFLAGS@ @SSL_CPPFLAGS@ @CRYPTO_CPPFLAGS@

else # --- Actions without Precompiled Headers enabled

BUILT_SOURCES = \
	ircd.pic.h \
	matrix.pic.h \
	###

ircd.pic.h: ircd.h
	cp ircd.h ircd.pic.h

matrix.pic.h: matrix.h
	cp matrix.h matrix.pic.h

endif # --- /BUILD_PCH

#
# Clean / Install
#

clean-local:
	rm -f *.h.gch
	rm -f *.gch.tmp
	rm -f ircd.pic.h
	rm -f matrix.pic.h

install-mkdirs: $(DEPS) $(BUILT_SOURCES)
	-@if test ! -d $(DESTDIR)$(includedir); then \
		echo "mkdir -p $(DESTDIR)$(includedir)"; \
		mkdir -p $(DESTDIR)$(includedir); \
	fi

install: install-mkdirs
	@echo "ircd: installing headers"
	@for i in $(srcdir)/*; do \
		if [[ "$$i" != "./Makefile"* ]] && [[ "$$i" != *.h.gch ]]; then \
			echo "installing $$i to $(DESTDIR)$(includedir)"; \
			cp -r $$i $(DESTDIR)$(includedir); \
		fi \
	done
