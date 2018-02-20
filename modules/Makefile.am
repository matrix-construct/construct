################################################################################
#
# IRCd Modules
#

AM_CXXFLAGS = \
	###

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@JS_CPPFLAGS@ \
	@BOOST_CPPFLAGS@ \
	-include $(top_srcdir)/include/ircd/ircd.h \
	-include $(top_srcdir)/include/ircd/m/m.h \
	-include $(top_srcdir)/include/ircd/mapi.h \
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

###############################################################################
#
# (TBD) equipment
#

# This puts the source in vm/ but the installed
# library is vm_X.so in the main modules dir.
m_moduledir = @moduledir@

console_la_SOURCES = console.cc
m_noop_la_SOURCES = m_noop.cc

m_module_LTLIBRARIES = \
	console.la \
	m_noop.la \
	###

###############################################################################
#
# /
#

moduledir = @moduledir@

root_la_SOURCES = root.cc

module_LTLIBRARIES = \
	root.la \
	###

###############################################################################
#
# /_matrix/client/
#

# This puts the source in client/ but the installed
# library is client_X.so in the main modules dir.
client_moduledir = @moduledir@

client_client_versions_la_SOURCES = client/versions.cc
client_client_events_la_SOURCES = client/events.cc
client_client_register_la_SOURCES = client/register.cc
client_client_login_la_SOURCES = client/login.cc
client_client_logout_la_SOURCES = client/logout.cc
client_client_sync_la_SOURCES = client/sync.cc
client_client_presence_la_SOURCES = client/presence.cc
client_client_profile_la_SOURCES = client/profile.cc
client_client_devices_la_SOURCES = client/devices.cc
client_client_keys_la_SOURCES = client/keys.cc
client_client_pushers_la_SOURCES = client/pushers.cc
client_client_publicrooms_la_SOURCES = client/publicrooms.cc
client_client_createroom_la_SOURCES = client/createroom.cc
client_client_pushrules_la_SOURCES = client/pushrules.cc
client_client_join_la_SOURCES = client/join.cc
client_client_voip_turnserver_la_SOURCES = client/voip/turnserver.cc
client_client_directory_room_la_SOURCES = client/directory/room.cc
client_client_directory_user_la_SOURCES = client/directory/user.cc

client_module_LTLIBRARIES = \
	client/client_versions.la \
	client/client_events.la \
	client/client_register.la \
	client/client_login.la \
	client/client_logout.la \
	client/client_sync.la \
	client/client_presence.la \
	client/client_profile.la \
	client/client_devices.la \
	client/client_keys.la \
	client/client_pushers.la \
	client/client_publicrooms.la \
	client/client_createroom.la \
	client/client_pushrules.la \
	client/client_join.la \
	client/client_voip_turnserver.la \
	client/client_directory_room.la \
	client/client_directory_user.la \
	###

#
# client/rooms/
#

client_client_rooms_la_SOURCES = \
	client/rooms/messages.cc \
	client/rooms/state.cc \
	client/rooms/members.cc \
	client/rooms/context.cc \
	client/rooms/send.cc \
	client/rooms/typing.cc \
	client/rooms/redact.cc \
	client/rooms/receipt.cc \
	client/rooms/join.cc \
	client/rooms/read_markers.cc \
	client/rooms/rooms.cc \
	###

client_module_LTLIBRARIES += client/client_rooms.la

#
# client/user/
#

client_client_user_la_SOURCES = \
	client/user/filter.cc \
	client/user/account_data.cc \
	client/user/user.cc \
	###

client_module_LTLIBRARIES += client/client_user.la

#
# client/account/
#

client_client_account_la_SOURCES = \
	client/account/3pid.cc \
	client/account/whoami.cc \
	client/account/password.cc \
	client/account/deactivate.cc \
	client/account/account.cc \
	###

client_module_LTLIBRARIES += client/client_account.la

###############################################################################
#
# /_matrix/key/
#

key_moduledir = @moduledir@

key_key_server_la_SOURCES = key/server.cc
key_key_query_la_SOURCES = key/query.cc

key_module_LTLIBRARIES = \
	key/key_server.la \
	key/key_query.la \
	###

###############################################################################
#
# /_matrix/federation/
#

federation_moduledir = @moduledir@

federation_federation_send_la_SOURCES = federation/send.cc
federation_federation_event_la_SOURCES = federation/event.cc
federation_federation_get_missing_events_la_SOURCES = federation/get_missing_events.cc
federation_federation_version_la_SOURCES = federation/version.cc

federation_module_LTLIBRARIES = \
	federation/federation_send.la \
	federation/federation_event.la \
	federation/federation_get_missing_events.la \
	federation/federation_version.la \
	###

###############################################################################
#
# /_matrix/media/
#

media_moduledir = @moduledir@

media_media_download_la_SOURCES = media/download.cc
media_media_upload_la_SOURCES = media/upload.cc

media_module_LTLIBRARIES = \
	media/media_download.la \
	media/media_upload.la \
	###

###############################################################################
#
# JavaScript
#

if JS

server_moduledir = @moduledir@

server_server_console_la_SOURCES = server/console.cc
server_server_listener_la_SOURCES = server/listener.cc

server_module_LTLIBRARIES = \
	server/server_console.la \
	server/server_listener.la \
	###

module_LTLIBRARIES += \
	kernel.la \
	###

endif
