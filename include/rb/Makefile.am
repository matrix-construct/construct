AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = @BOOST_CPPFLAGS@

if GCC
AM_CXXFLAGS = -fpch-deps
endif

BUILT_SOURCES = rb.h.gch

rb.h.gch:
	$(CXX) $(CXXFLAGS) $(AM_CXXFLAGS) -o rb.h.gch $(DEFS) $(CPPFLAGS) $(AM_CPPFLAGS) rb.h

clean-local:
	rm -f rb.h.gch

mrproper-local:
	rm -f config.h
	rm -f config.h.in
	rm -f stamp-h1
