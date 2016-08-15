AUTOMAKE_OPTIONS = foreign


AM_CPPFLAGS = \
    -I$(top_srcdir)/include \
    @BOOST_CPPFLAGS@

if GCC
AM_CXXFLAGS = -fpch-deps
endif

if BUILD_PCH
BUILT_SOURCES = stdinc.h.gch stdinc.pic.h.gch
endif

stdinc.h.gch:
	$(CXX) $(CXXFLAGS) $(AM_CXXFLAGS) -o stdinc.h.gch $(DEFS) $(CPPFLAGS) $(AM_CPPFLAGS) stdinc.h

stdinc.pic.h.gch:
	cp stdinc.h stdinc.pic.h
	$(CXX) $(CXXFLAGS) $(AM_CXXFLAGS) -fPIC -o stdinc.pic.h.gch $(DEFS) $(CPPFLAGS) $(AM_CPPFLAGS) stdinc.pic.h

clean-local:
	rm -f stdinc.h.gch
	rm -f stdinc.pic.h
	rm -f stdinc.pic.h.gch
