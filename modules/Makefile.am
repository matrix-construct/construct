#
#AM_CXXFLAGS = \
#	-fno-implicit-templates
#

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@JS_CPPFLAGS@ \
	@BOOST_CPPFLAGS@ \
	-include $(top_srcdir)/include/ircd/ircd.h \
	-include $(top_srcdir)/include/ircd/mapi.h \
	-include $(top_srcdir)/include/ircd/m.h \
	###

AM_LDFLAGS = \
	-L$(top_srcdir)/ircd \
	@JS_LDFLAGS@ \
	@BOOST_LDFLAGS@ \
	###

AM_LDFLAGS += \
	-avoid-version \
	-module \
	-Wl,-fuse-ld=gold \
	-Wl,--shared \
	-Wl,--export-dynamic \
	-Wl,--no-gnu-unique \
	-Wl,--no-allow-shlib-undefined \
	$(PLATFORM_LDFLAGS) \
	# -Wl,--no-undefined \
	# -export-symbols-regex IRCD_MODULE* \
	# -export-symbols-regex * \
	###

moduledir=@moduledir@
root_la_SOURCES = root.cc
module_LTLIBRARIES = \
	root.la \
	###

# This puts the source in db/ but the installed
# library is client_X.so in the main modules dir.
db_moduledir = @moduledir@
db_db_events_la_SOURCES = db/events.cc
db_module_LTLIBRARIES = \
	db/db_events.la \
	###

# This puts the source in client/ but the installed
# library is client_X.so in the main modules dir.
client_moduledir = @moduledir@
client_client_versions_la_SOURCES = client/versions.cc
client_client_events_la_SOURCES = client/events.cc
client_client_account_la_SOURCES = client/account.cc
client_client_register_la_SOURCES = client/register.cc
client_client_login_la_SOURCES = client/login.cc
client_client_logout_la_SOURCES = client/logout.cc
client_client_sync_la_SOURCES = client/sync.cc
client_client_rooms_la_SOURCES = client/rooms.cc
client_client_publicrooms_la_SOURCES = client/publicrooms.cc
client_client_createroom_la_SOURCES = client/createroom.cc
client_client_pushrules_la_SOURCES = client/pushrules.cc
client_client_user_la_SOURCES = client/user.cc
client_client_voip_turnserver_la_SOURCES = client/voip/turnserver.cc
client_module_LTLIBRARIES = \
	client/client_versions.la \
	client/client_events.la \
	client/client_account.la \
	client/client_register.la \
	client/client_login.la \
	client/client_logout.la \
	client/client_sync.la \
	client/client_rooms.la \
	client/client_publicrooms.la \
	client/client_createroom.la \
	client/client_pushrules.la \
	client/client_user.la \
	client/client_voip_turnserver.la \
	###

if JS
server_moduledir = @moduledir@
server_server_console_la_SOURCES = server/console.cc
server_server_listener_la_SOURCES = server/listener.cc
server_module_LTLIBRARIES = \
        server/server_console.la \
        server/server_listener.la \
        ###
endif

if JS
module_LTLIBRARIES += \
	kernel.la \
	###
endif
