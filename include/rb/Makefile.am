AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = @BOOST_CPPFLAGS@
AM_LDFLAGS = @BOOST_LDFLAGS@

BUILT_SOURCES = rb.h.gch

rb.h.gch:
	$(CXX) $(CXXFLAGS) -o rb.h.gch $(DEFS) $(CPPFLAGS) rb.h

clean-local:
	rm -f rb.h.gch

mrproper-local:
	rm -f config.h
	rm -f config.h.in
	rm -f stamp-h1
