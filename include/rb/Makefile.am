AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = @BOOST_CPPFLAGS@

if GCC
AM_CXXFLAGS = -fpch-deps
endif

if BUILD_PCH
BUILT_SOURCES = rb.h.gch rb.pic.h.gch
endif

rb.h.gch:
	$(CXX) $(CXXFLAGS) $(AM_CXXFLAGS) -o rb.h.gch $(DEFS) $(CPPFLAGS) $(AM_CPPFLAGS) rb.h

rb.pic.h.gch:
	cp rb.h rb.pic.h
	$(CXX) $(CXXFLAGS) $(AM_CXXFLAGS) -fPIC -o rb.pic.h.gch -DPIC $(DEFS) $(CPPFLAGS) $(AM_CPPFLAGS) rb.pic.h

clean-local:
	rm -f rb.h.gch
	rm -f rb.pic.h
	rm -f rb.pic.h.gch

mrproper-local:
	rm -f config.h
	rm -f config.h.in
	rm -f stamp-h1
