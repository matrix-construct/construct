AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@LTDLINCL@ \
	@BOOST_CPPFLAGS@ 

AM_LDFLAGS = \
	@BOOST_LDFLAGS@ \
	-L$(top_srcdir)/rb


bin_PROGRAMS = charybdis-mkpasswd

charybdis_mkpasswd_LDADD = \
	-lrb \
	@BOOST_LIBS@

charybdis_mkpasswd_SOURCES = \
	mkpasswd.cc


bin_PROGRAMS += charybdis-mkfingerprint

charybdis_mkfingerprint_LDADD = \
	-lrb \
	@BOOST_LIBS@

charybdis_mkfingerprint_SOURCES = \
	mkfingerprint.cc


mrproper-local:
	rm -f genssl
