AUTOMAKE_OPTIONS = foreign

AM_CPPFLAGS = -isystem $(top_srcdir)/boost/include
AM_LDFLAGS = -L$(top_srcdir)/boost/lib

BUILT_SOURCES = rb.h.gch

rb.h.gch:
	$(CXX) -o rb.h.gch $(CPPFLAGS) rb.h

clean-local:
	rm -f rb.h.gch

mrproper-local:
	rm -f config.h
	rm -f config.h.in
	rm -f stamp-h1
