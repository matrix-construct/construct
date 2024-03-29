AUTOMAKE_OPTIONS = foreign

DEFS += -DIRCD_UNIT
DEFS += -DIRCD_UNIT_MODULE

AM_CPPFLAGS =#
AM_CPPFLAGS += -I$(top_srcdir)/include

AM_CXXFLAGS =#
AM_CXXFLAGS += -ftls-model=global-dynamic

AM_LDFLAGS =#
AM_LDFLAGS += -module
AM_LDFLAGS += -avoid-version
AM_LDFLAGS += -Wl,--allow-shlib-undefined
AM_LDFLAGS += -Wl,-Bsymbolic
AM_LDFLAGS += -Wl,-z,lazy
AM_LDFLAGS += -L$(top_srcdir)/ircd
AM_LDFLAGS += -L$(top_srcdir)/matrix
AM_LDFLAGS += $(PLATFORM_LDFLAGS)

if CLANG_PCH
AM_CPPFLAGS += -include-pch $(top_srcdir)/include/ircd/matrix.pic.h.gch
AM_CPPFLAGS += -include $(top_srcdir)/include/ircd/mods/mapi.h
else
AM_CPPFLAGS += -include $(top_srcdir)/include/ircd/matrix.pic.h
AM_CPPFLAGS += -include $(top_srcdir)/include/ircd/mods/mapi.h
endif

if GOLD
AM_LDFLAGS += -Wl,--no-gnu-unique
AM_LDFLAGS += -Wl,--sort-common=descending
endif

CPPFLAGS += $(EXTRA_CPPFLAGS)
CXXFLAGS += $(EXTRA_CXXFLAGS)
LDFLAGS += $(EXTRA_LDFLAGS)

###############################################################################
#
# /
#

moduledir = @moduledir@

net_dns_cache_la_SOURCES = net_dns_cache.cc
stats_la_SOURCES = stats.cc
console_la_SOURCES = console.cc
web_root_la_SOURCES = web_root.cc
web_hook_la_SOURCES = web_hook.cc
well_known_la_SOURCES = well_known.cc

module_LTLIBRARIES = \
	net_dns_cache.la \
	stats.la \
	console.la \
	web_root.la \
	web_hook.la \
	well_known.la \
	###

#
# LLVM
#
if LLVM

llvm_la_SOURCES = llvm.cc
llvm_la_CPPFLAGS = $(AM_CPPFLAGS) @LLVM_CPPFLAGS@
llvm_la_LDFLAGS = $(AM_LDFLAGS) @LLVM_LDFLAGS@
llvm_la_LIBADD = $(AM_LIBS) @LLVM_LIBS@
module_LTLIBRARIES += llvm.la

endif LLVM

###############################################################################
#
# Messages / protocol
#

m_moduledir = @moduledir@

m_breadcrumbs_la_SOURCES = m_breadcrumbs.cc
m_bridge_la_SOURCES = m_bridge.cc
m_bridge_register_la_SOURCES = m_bridge_register.cc
m_command_la_SOURCES = m_command.cc
m_control_la_SOURCES = m_control.cc
m_device_la_SOURCES = m_device.cc
m_device_list_update_la_SOURCES = m_device_list_update.cc
m_signing_key_update_la_SOURCES = m_signing_key_update.cc
m_direct_la_SOURCES = m_direct.cc
m_direct_to_device_la_SOURCES = m_direct_to_device.cc
m_ignored_user_list_la_SOURCES = m_ignored_user_list.cc
m_listen_la_SOURCES = m_listen.cc
m_noop_la_SOURCES = m_noop.cc
m_presence_la_SOURCES = m_presence.cc
m_profile_la_SOURCES = m_profile.cc
m_push_la_SOURCES = m_push.cc
m_pusher_la_SOURCES = m_pusher.cc
m_receipt_la_SOURCES = m_receipt.cc
m_relation_la_SOURCES = m_relation.cc
m_room_aliases_la_SOURCES = m_room_aliases.cc
m_room_canonical_alias_la_SOURCES = m_room_canonical_alias.cc
m_room_create_la_SOURCES = m_room_create.cc
m_room_history_visibility_la_SOURCES = m_room_history_visibility.cc
m_room_join_rules_la_SOURCES = m_room_join_rules.cc
m_room_member_la_SOURCES = m_room_member.cc
m_room_message_la_SOURCES = m_room_message.cc
m_room_name_la_SOURCES = m_room_name.cc
m_room_power_levels_la_SOURCES = m_room_power_levels.cc
m_room_redaction_la_SOURCES = m_room_redaction.cc
m_room_server_acl_la_SOURCES = m_room_server_acl.cc
m_room_third_party_invite_la_SOURCES = m_room_third_party_invite.cc
m_room_tombstone_la_SOURCES = m_room_tombstone.cc

m_module_LTLIBRARIES = \
	m_breadcrumbs.la \
	m_bridge.la \
	m_bridge_register.la \
	m_command.la \
	m_control.la \
	m_device.la \
	m_device_list_update.la \
	m_signing_key_update.la \
	m_direct.la \
	m_direct_to_device.la \
	m_ignored_user_list.la \
	m_listen.la \
	m_noop.la \
	m_presence.la \
	m_profile.la \
	m_push.la \
	m_pusher.la \
	m_receipt.la \
	m_relation.la \
	m_room_aliases.la \
	m_room_canonical_alias.la \
	m_room_create.la \
	m_room_history_visibility.la \
	m_room_join_rules.la \
	m_room_member.la \
	m_room_message.la \
	m_room_name.la \
	m_room_power_levels.la \
	m_room_redaction.la \
	m_room_server_acl.la \
	m_room_third_party_invite.la \
	m_room_tombstone.la \
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
# /_matrix/identity/
#

identity_moduledir = @moduledir@

identity_identity_v1_la_SOURCES = identity/v1.cc
identity_identity_pubkey_la_SOURCES = identity/pubkey.cc

identity_module_LTLIBRARIES = \
	identity/identity_v1.la \
	identity/identity_pubkey.la \
	###

###############################################################################
#
# /_matrix/media/
#

media_moduledir = @moduledir@

media_media_media_la_SOURCES = \
	media/media.cc \
	media/config.cc \
	media/upload.cc \
	media/download.cc \
	media/thumbnail.cc \
	media/preview_url.cc \
	###

media_module_LTLIBRARIES = \
	media/media_media.la \
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
federation_federation_state_la_SOURCES = federation/state.cc
federation_federation_make_leave_la_SOURCES = federation/make_leave.cc
federation_federation_send_leave_la_SOURCES = federation/send_leave.cc
federation_federation_backfill_la_SOURCES = federation/backfill.cc
federation_federation_event_auth_la_SOURCES = federation/event_auth.cc
federation_federation_query_auth_la_SOURCES = federation/query_auth.cc
federation_federation_publicrooms_la_SOURCES = federation/publicrooms.cc
federation_federation_user_devices_la_SOURCES = federation/user_devices.cc
federation_federation_user_keys_query_la_SOURCES = federation/user_keys_query.cc
federation_federation_user_keys_claim_la_SOURCES = federation/user_keys_claim.cc
federation_federation_rooms_la_SOURCES = federation/rooms.cc
federation_federation_hierarchy_la_SOURCES = federation/hierarchy.cc
federation_federation_timestamp_to_event_la_SOURCES = federation/timestamp_to_event.cc

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
	federation/federation_state.la \
	federation/federation_make_leave.la \
	federation/federation_send_leave.la \
	federation/federation_backfill.la \
	federation/federation_event_auth.la \
	federation/federation_query_auth.la \
	federation/federation_publicrooms.la \
	federation/federation_user_devices.la \
	federation/federation_user_keys_query.la \
	federation/federation_user_keys_claim.la \
	federation/federation_rooms.la \
	federation/federation_hierarchy.la \
	federation/federation_timestamp_to_event.la \
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
client_client_pushers_la_SOURCES = client/pushers.cc
client_client_publicrooms_la_SOURCES = client/publicrooms.cc
client_client_createroom_la_SOURCES = client/createroom.cc
client_client_pushrules_la_SOURCES = client/pushrules.cc
client_client_join_la_SOURCES = client/join.cc
client_client_publicised_groups_la_SOURCES = client/publicised_groups.cc
client_client_initialsync_la_SOURCES = client/initialsync.cc
client_client_search_la_SOURCES = client/search.cc
client_client_joined_groups_la_SOURCES = client/joined_groups.cc
client_client_register_available_la_SOURCES = client/register_available.cc
client_client_capabilities_la_SOURCES = client/capabilities.cc
client_client_send_to_device_la_SOURCES = client/send_to_device.cc
client_client_delete_devices_la_SOURCES = client/delete_devices.cc
client_client_notifications_la_SOURCES = client/notifications.cc
client_client_register_email_la_SOURCES = client/register_email.cc
client_client_create_group_la_SOURCES = client/create_group.cc
client_client_dehydrated_device_la_SOURCES = client/dehydrated_device.cc

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
	client/client_pushers.la \
	client/client_publicrooms.la \
	client/client_createroom.la \
	client/client_pushrules.la \
	client/client_join.la \
	client/client_publicised_groups.la \
	client/client_initialsync.la \
	client/client_search.la \
	client/client_joined_groups.la \
	client/client_register_available.la \
	client/client_capabilities.la \
	client/client_send_to_device.la \
	client/client_delete_devices.la \
	client/client_notifications.la \
	client/client_register_email.la \
	client/client_create_group.la \
	client/client_dehydrated_device.la \
	###

#
# client/rooms/
#

client_client_rooms_la_SOURCES = \
	client/rooms/messages.cc \
	client/rooms/state.cc \
	client/rooms/members.cc \
	client/rooms/context.cc \
	client/rooms/event.cc \
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
	client/rooms/report.cc \
	client/rooms/relations.cc \
	client/rooms/upgrade.cc \
	client/rooms/aliases.cc \
	client/rooms/hierarchy.cc \
	client/rooms/threads.cc \
	client/rooms/timestamp_to_event.cc \
	client/rooms/rooms.cc \
	###

client_module_LTLIBRARIES += \
	client/client_rooms.la \
	###

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

client_module_LTLIBRARIES += \
	client/client_user.la \
	###

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

client_module_LTLIBRARIES += \
	client/client_account.la \
	###

#
# client/directory/
#

client_client_directory_room_la_SOURCES = client/directory/room.cc
client_client_directory_user_la_SOURCES = client/directory/user.cc

client_module_LTLIBRARIES += \
	client/client_directory_room.la \
	client/client_directory_user.la \
	###

#
# client/directory/list
#

client_client_directory_list_room_la_SOURCES = client/directory/list/room.cc
client_client_directory_list_appservice_la_SOURCES = client/directory/list/appservice.cc

client_module_LTLIBRARIES += \
	client/client_directory_list_room.la \
	client/client_directory_list_appservice.la \
	###

#
# client/voip/
#

client_client_voip_turnserver_la_SOURCES = client/voip/turnserver.cc

client_module_LTLIBRARIES += \
	client/client_voip_turnserver.la \
	###

#
# client/thirdparty/
#

client_client_thirdparty_protocols_la_SOURCES = client/thirdparty/protocols.cc

client_module_LTLIBRARIES += \
	client/client_thirdparty_protocols.la \
	###

#
# client/sync/
#

client_client_sync_account_data_la_SOURCES = client/sync/account_data.cc
client_client_sync_presence_la_SOURCES = client/sync/presence.cc
client_client_sync_rooms_la_SOURCES = client/sync/rooms.cc
client_client_sync_groups_la_SOURCES = client/sync/groups.cc
client_client_sync_to_device_la_SOURCES = client/sync/to_device.cc
client_client_sync_device_lists_la_SOURCES = client/sync/device_lists.cc
client_client_sync_device_one_time_keys_count_la_SOURCES = client/sync/device_one_time_keys_count.cc
client_client_sync_device_unused_fallback_key_types_la_SOURCES = client/sync/device_unused_fallback_key_types.cc

client_module_LTLIBRARIES += \
	client/client_sync_account_data.la \
	client/client_sync_presence.la \
	client/client_sync_rooms.la \
	client/client_sync_groups.la \
	client/client_sync_to_device.la \
	client/client_sync_device_lists.la \
	client/client_sync_device_one_time_keys_count.la \
	client/client_sync_device_unused_fallback_key_types.la \
	###

# client/sync/rooms/

client_client_sync_rooms_account_data_la_SOURCES = client/sync/rooms/account_data.cc
client_client_sync_rooms_ephemeral_la_SOURCES = client/sync/rooms/ephemeral.cc
client_client_sync_rooms_state_la_SOURCES = client/sync/rooms/state.cc
client_client_sync_rooms_timeline_la_SOURCES = client/sync/rooms/timeline.cc
client_client_sync_rooms_unread_notifications_la_SOURCES = client/sync/rooms/unread_notifications.cc
client_client_sync_rooms_summary_la_SOURCES = client/sync/rooms/summary.cc

client_module_LTLIBRARIES += \
	client/client_sync_rooms_account_data.la \
	client/client_sync_rooms_ephemeral.la \
	client/client_sync_rooms_state.la \
	client/client_sync_rooms_timeline.la \
	client/client_sync_rooms_unread_notifications.la \
	client/client_sync_rooms_summary.la \
	###

# client/sync/rooms/ephemeral/

client_client_sync_rooms_ephemeral_receipt_la_SOURCES = client/sync/rooms/ephemeral/receipt.cc
client_client_sync_rooms_ephemeral_typing_la_SOURCES = client/sync/rooms/ephemeral/typing.cc

client_module_LTLIBRARIES += \
	client/client_sync_rooms_ephemeral_receipt.la \
	client/client_sync_rooms_ephemeral_typing.la \
	###

#
# client/keys/
#

client_client_keys_upload_la_SOURCES = client/keys/upload.cc
client_client_keys_query_la_SOURCES = client/keys/query.cc
client_client_keys_claim_la_SOURCES = client/keys/claim.cc
client_client_keys_changes_la_SOURCES = client/keys/changes.cc
client_client_keys_signatures_upload_la_SOURCES = client/keys/signatures/upload.cc
client_client_keys_device_signing_upload_la_SOURCES = client/keys/device_signing/upload.cc

client_module_LTLIBRARIES += \
	client/client_keys_upload.la \
	client/client_keys_query.la \
	client/client_keys_claim.la \
	client/client_keys_changes.la \
	client/client_keys_signatures_upload.la \
	client/client_keys_device_signing_upload.la \
	###

#
# client/room_keys/
#

client_client_room_keys_la_SOURCES = \
	client/room_keys/keys.cc \
	client/room_keys/version.cc \
	client/room_keys/room_keys.cc \
	###

client_module_LTLIBRARIES += \
	client/client_room_keys.la \
	###

#
# client/groups
#

client_client_groups_la_SOURCES = \
	client/groups/profile.cc \
	client/groups/summary.cc \
	client/groups/categories.cc \
	client/groups/roles.cc \
	client/groups/rooms.cc \
	client/groups/users.cc \
	client/groups/invited_users.cc \
	client/groups/settings.cc \
	client/groups/self.cc \
	client/groups/groups.cc \
	###

client_module_LTLIBRARIES += \
	client/client_groups.la \
	###

###############################################################################
#
# /_matrix/widget
#

widget_moduledir = @moduledir@

widget_widget_widget_la_SOURCES = widget/widget.cc
widget_widget_register_la_SOURCES = widget/register.cc
widget_widget_account_la_SOURCES = widget/account.cc
widget_widget_ui_la_SOURCES = widget/ui.cc

widget_module_LTLIBRARIES = \
	widget/widget_widget.la \
	widget/widget_register.la \
	widget/widget_account.la \
	widget/widget_ui.la \
	###

###############################################################################
#
# /_synapse/admin/
#

admin_moduledir = @moduledir@

admin_admin_users_la_SOURCES = admin/users.cc
admin_admin_deactivate_la_SOURCES = admin/deactivate.cc
admin_admin_server_version_la_SOURCES = admin/server_version.cc
admin_admin_federation_la_SOURCES = admin/federation.cc
admin_admin_rooms_la_SOURCES = admin/rooms.cc

admin_module_LTLIBRARIES = \
	admin/admin_users.la \
	admin/admin_deactivate.la \
	admin/admin_server_version.la \
	admin/admin_federation.la \
	admin/admin_rooms.la \
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
