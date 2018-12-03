################################################################################
#
# IRCd Modules
#

AM_CXXFLAGS = \
	@EXTRA_CXXFLAGS@ \
	###

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	@ROCKSDB_CPPFLAGS@ \
	@JS_CPPFLAGS@ \
	@BOOST_CPPFLAGS@ \
	@SSL_CPPFLAGS@ \
	@CRYPTO_CPPFLAGS@ \
	@SODIUM_CPPFLAGS@ \
	@MAGIC_CPPFLAGS@ \
	@SNAPPY_CPPFLAGS@ \
	@LZ4_CPPFLAGS@ \
	@Z_CPPFLAGS@ \
	-include $(top_srcdir)/include/ircd/ircd.pic.h \
	-include $(top_srcdir)/include/ircd/mods/mapi.h \
	@EXTRA_CPPFLAGS@ \
	###

AM_LDFLAGS = \
	-avoid-version \
	-module \
	-Wl,-fuse-ld=gold \
	-Wl,--shared \
	-Wl,--export-dynamic \
	-Wl,--no-gnu-unique \
	-Wl,--no-allow-shlib-undefined \
	-Wl,--gc-sections \
	-Wl,--icf=all \
	-Wl,--rosegment \
	-L$(top_srcdir)/ircd \
	$(PLATFORM_LDFLAGS) \
	@EXTRA_LDFLAGS@ \
	# -Wl,--no-undefined \
	# -export-symbols-regex IRCD_MODULE* \
	# -export-symbols-regex * \
	###

###############################################################################
#
# /
#

moduledir = @moduledir@

webroot_la_SOURCES = webroot.cc
console_la_SOURCES = console.cc
vm_la_SOURCES = vm.cc

module_LTLIBRARIES = \
	webroot.la \
	console.la \
	vm.la \
	###

###############################################################################
#
# Server modules
#

s_moduledir = @moduledir@

s_conf_la_SOURCES = s_conf.cc
s_control_la_SOURCES = s_control.cc
s_dns_la_SOURCES =  s_dns.cc s_dns_cache.cc s_dns_resolver.cc
s_node_la_SOURCES = s_node.cc
s_listen_la_SOURCES = s_listen.cc
s_keys_la_SOURCES = s_keys.cc
s_fetch_la_SOURCES = s_fetch.cc

s_module_LTLIBRARIES = \
	s_conf.la \
	s_control.la \
	s_dns.la \
	s_node.la \
	s_listen.la \
	s_keys.la \
	s_fetch.la \
	###

###############################################################################
#
# Messages / protocol
#

m_moduledir = @moduledir@

m_noop_la_SOURCES = m_noop.cc
m_user_la_SOURCES = m_user.cc
m_event_la_SOURCES = m_event.cc
m_typing_la_SOURCES = m_typing.cc
m_receipt_la_SOURCES = m_receipt.cc
m_presence_la_SOURCES = m_presence.cc
m_state_la_SOURCES = m_state.cc
m_rooms_la_SOURCES = m_rooms.cc
m_room_la_SOURCES = m_room.cc
m_room_create_la_SOURCES = m_room_create.cc
m_room_member_la_SOURCES = m_room_member.cc
m_room_join_rules_la_SOURCES = m_room_join_rules.cc
m_room_history_visibility_la_SOURCES = m_room_history_visibility.cc
m_room_canonical_alias_la_SOURCES = m_room_canonical_alias.cc
m_room_aliases_la_SOURCES = m_room_aliases.cc
m_room_message_la_SOURCES = m_room_message.cc
m_room_power_levels_la_SOURCES = m_room_power_levels.cc

m_module_LTLIBRARIES = \
	m_noop.la \
	m_user.la \
	m_event.la \
	m_typing.la \
	m_receipt.la \
	m_presence.la \
	m_state.la \
	m_rooms.la \
	m_room.la \
	m_room_create.la \
	m_room_member.la \
	m_room_join_rules.la \
	m_room_history_visibility.la \
	m_room_canonical_alias.la \
	m_room_aliases.la \
	m_room_message.la \
	m_room_power_levels.la \
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
client_client_publicised_groups_la_SOURCES = client/publicised_groups.cc
client_client_initialsync_la_SOURCES = client/initialsync.cc
client_client_search_la_SOURCES = client/search.cc
client_client_joined_groups_la_SOURCES = client/joined_groups.cc

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
	client/client_publicised_groups.la \
	client/client_initialsync.la \
	client/client_search.la \
	client/client_joined_groups.la \
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
	client/rooms/invite.cc \
	client/rooms/leave.cc \
	client/rooms/forget.cc \
	client/rooms/kick.cc \
	client/rooms/ban.cc \
	client/rooms/unban.cc \
	client/rooms/read_markers.cc \
	client/rooms/initialsync.cc \
	client/rooms/rooms.cc \
	###

client_module_LTLIBRARIES += client/client_rooms.la

#
# client/user/
#

client_client_user_la_SOURCES = \
	client/user/openid.cc \
	client/user/filter.cc \
	client/user/account_data.cc \
	client/user/rooms.cc \
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

#
# client/thirdparty/
#

client_client_thirdparty_protocols_la_SOURCES = \
	client/thirdparty/protocols.cc \
	###

client_module_LTLIBRARIES += \
	client/client_thirdparty_protocols.la \
	###

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
federation_federation_get_groups_publicised_la_SOURCES = federation/get_groups_publicised.cc
federation_federation_version_la_SOURCES = federation/version.cc
federation_federation_sender_la_SOURCES = federation/sender.cc
federation_federation_query_la_SOURCES = federation/query.cc
federation_federation_invite_la_SOURCES = federation/invite.cc
federation_federation_make_join_la_SOURCES = federation/make_join.cc
federation_federation_send_join_la_SOURCES = federation/send_join.cc
federation_federation_state_ids_la_SOURCES = federation/state_ids.cc
federation_federation_state_la_SOURCES = federation/state.cc
federation_federation_make_leave_la_SOURCES = federation/make_leave.cc
federation_federation_send_leave_la_SOURCES = federation/send_leave.cc
federation_federation_backfill_la_SOURCES = federation/backfill.cc
federation_federation_backfill_ids_la_SOURCES = federation/backfill_ids.cc
federation_federation_event_auth_la_SOURCES = federation/event_auth.cc
federation_federation_publicrooms_la_SOURCES = federation/publicrooms.cc
federation_federation_federation_la_SOURCES = federation/federation.cc

federation_module_LTLIBRARIES = \
	federation/federation_send.la \
	federation/federation_event.la \
	federation/federation_get_missing_events.la \
	federation/federation_get_groups_publicised.la \
	federation/federation_version.la \
	federation/federation_sender.la \
	federation/federation_query.la \
	federation/federation_invite.la \
	federation/federation_make_join.la \
	federation/federation_send_join.la \
	federation/federation_state_ids.la \
	federation/federation_state.la \
	federation/federation_make_leave.la \
	federation/federation_send_leave.la \
	federation/federation_backfill.la \
	federation/federation_backfill_ids.la \
	federation/federation_event_auth.la \
	federation/federation_publicrooms.la \
	federation/federation_federation.la \
	###

###############################################################################
#
# /_matrix/media/
#

media_moduledir = @moduledir@

media_media_media_la_SOURCES = \
	media/download.cc \
	media/upload.cc \
	media/thumbnail.cc \
	media/preview_url.cc \
	media/media.cc \
	###

media_module_LTLIBRARIES = \
	media/media_media.la \
	###

###############################################################################
#
# /_matrix/app/
#

app_moduledir = @moduledir@

app_app_register_la_SOURCES = \
	app/register.cc \
	###

app_module_LTLIBRARIES = \
	app/app_register.la \
	###

###############################################################################
#
# JavaScript
#

if JS

js_moduledir = @moduledir@

js_js_console_la_SOURCES = js/console.cc
js_js_listener_la_SOURCES = js/listener.cc
js_js_kernel_la_SOURCES = js/kernel.cc

js_module_LTLIBRARIES = \
	js/js_console.la \
	js/js_listener.la \
	js/js_kernel.la \
	###

endif
