AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS =                 \
    -I$(top_srcdir)/include   \
    @BOOST_CPPFLAGS@          \
    ###

AM_CXXFLAGS = \
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
	###
endif

ircd.h.gch:
	$(CXX) $(CXXFLAGS) $(AM_CXXFLAGS) -o ircd.h.gch $(DEFS) $(CPPFLAGS) $(AM_CPPFLAGS) ircd.h

ircd.pic.h.gch:
	cp ircd.h ircd.pic.h
	$(CXX) $(CXXFLAGS) $(AM_CXXFLAGS) -fPIC -o ircd.pic.h.gch $(DEFS) $(CPPFLAGS) $(AM_CPPFLAGS) ircd.pic.h

clean-local:
	rm -f ircd.h.gch
	rm -f ircd.pic.h
	rm -f ircd.pic.h.gch
