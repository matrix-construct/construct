AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@JS_CPPFLAGS@ \
	@BOOST_CPPFLAGS@ \
	-include $(top_srcdir)/include/ircd/ircd.h \
	-include $(top_srcdir)/include/ircd/mapi.h \
	-include $(top_srcdir)/include/ircd/m.h \
	-include $(top_srcdir)/include/ircd/db/object.h

AM_LDFLAGS = \
	-L$(top_srcdir)/ircd \
	@JS_LDFLAGS@ \
	@BOOST_LDFLAGS@

AM_LDFLAGS += \
	-avoid-version \
	-module \
	-Wl,-fuse-ld=gold \
	-Wl,--shared \
	-Wl,--export-dynamic \
	-Wl,--no-gnu-unique \
	-Wl,--no-allow-shlib-undefined \
	$(PLATFORM_LDFLAGS)
#	-Wl,--no-undefined
#	-export-symbols-regex IRCD_MODULE*
#	-export-symbols-regex *

# This puts the source in client/ but the installed
# library is client_X.so in the main modules dir.
client_moduledir=@moduledir@
client_client_versions_la_SOURCES   = client/versions.cc
client_client_register_la_SOURCES   = client/register.cc
client_client_login_la_SOURCES      = client/login.cc
client_module_LTLIBRARIES =     \
	client/client_versions.la   \
	client/client_register.la   \
	client/client_login.la

moduledir=@moduledir@
module_LTLIBRARIES = \
