AUTOMAKE_OPTIONS = foreign

BUILT_SOURCES =#
CLEANFILES =#

DEFS += \
	-DIRCD_UNIT \
	-DIRCD_MATRIX_UNIT \
	###

CXXFLAGS += \
	$(EXTRA_CXXFLAGS) \
	###

AM_CXXFLAGS = \
	-include ircd/matrix.pic.h \
	-include ircd/mods/mapi.h \
	###

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	$(EXTRA_CPPFLAGS) \
	###

AM_LDFLAGS = \
	-version-info 0:1:0 \
	-Wl,--no-undefined-version \
	-Wl,--allow-shlib-undefined \
	-Wl,--unresolved-symbols=ignore-in-shared-libs \
	-Wl,-z,lazy \
	-L$(top_srcdir)/ircd \
	$(PLATFORM_LDFLAGS) \
	$(EXTRA_LDFLAGS) \
	###

AM_LIBS = \
	-lircd \
	###

if GCC
AM_CXXFLAGS += -ftls-model=local-dynamic
endif

if LTO
AM_CXXFLAGS += -flto=thin
endif

if LTO
if CLANG
AM_CXXFLAGS += -fstrict-vtable-pointers
AM_CXXFLAGS += -fwhole-program-vtables
endif
endif

if !PROFILE
if CLANG
AM_CXXFLAGS += -fprofile-use
BUILT_SOURCES += default.profdata
CLEANFILES += default.profdata
endif
endif

if MINGW
AM_LDFLAGS += -Wl,--enable-runtime-pseudo-reloc
endif

libircd_matrixdir = @libdir@
libircd_matrix_LTLIBRARIES = \
	libircd_matrix.la \
	###

#
# Unit list
#

libircd_matrix_la_SOURCES =#
libircd_matrix_la_SOURCES += default.profdata
libircd_matrix_la_SOURCES += name.cc
libircd_matrix_la_SOURCES += id.cc
libircd_matrix_la_SOURCES += dbs.cc
libircd_matrix_la_SOURCES += dbs_event_idx.cc
libircd_matrix_la_SOURCES += dbs_event_json.cc
libircd_matrix_la_SOURCES += dbs_event_column.cc
libircd_matrix_la_SOURCES += dbs_event_refs.cc
libircd_matrix_la_SOURCES += dbs_event_horizon.cc
libircd_matrix_la_SOURCES += dbs_event_sender.cc
libircd_matrix_la_SOURCES += dbs_event_type.cc
libircd_matrix_la_SOURCES += dbs_event_state.cc
libircd_matrix_la_SOURCES += dbs_room_events.cc
libircd_matrix_la_SOURCES += dbs_room_type.cc
libircd_matrix_la_SOURCES += dbs_room_state.cc
libircd_matrix_la_SOURCES += dbs_room_state_space.cc
libircd_matrix_la_SOURCES += dbs_room_joined.cc
libircd_matrix_la_SOURCES += dbs_room_head.cc
libircd_matrix_la_SOURCES += dbs_desc.cc
libircd_matrix_la_SOURCES += hook.cc
libircd_matrix_la_SOURCES += event.cc
libircd_matrix_la_SOURCES += event_cached.cc
libircd_matrix_la_SOURCES += event_conforms.cc
libircd_matrix_la_SOURCES += event_fetch.cc
libircd_matrix_la_SOURCES += event_get.cc
libircd_matrix_la_SOURCES += event_id.cc
libircd_matrix_la_SOURCES += event_index.cc
libircd_matrix_la_SOURCES += event_prefetch.cc
libircd_matrix_la_SOURCES += event_auth.cc
libircd_matrix_la_SOURCES += event_prev.cc
libircd_matrix_la_SOURCES += event_refs.cc
libircd_matrix_la_SOURCES += room.cc
libircd_matrix_la_SOURCES += room_auth.cc
libircd_matrix_la_SOURCES += room_aliases.cc
libircd_matrix_la_SOURCES += room_bootstrap.cc
libircd_matrix_la_SOURCES += room_create.cc
libircd_matrix_la_SOURCES += room_events.cc
libircd_matrix_la_SOURCES += room_head.cc
libircd_matrix_la_SOURCES += room_head_fetch.cc
libircd_matrix_la_SOURCES += room_state_fetch.cc
libircd_matrix_la_SOURCES += room_join.cc
libircd_matrix_la_SOURCES += room_leave.cc
libircd_matrix_la_SOURCES += room_visible.cc
libircd_matrix_la_SOURCES += room_members.cc
libircd_matrix_la_SOURCES += room_origins.cc
libircd_matrix_la_SOURCES += room_type.cc
libircd_matrix_la_SOURCES += room_content.cc
libircd_matrix_la_SOURCES += room_power.cc
libircd_matrix_la_SOURCES += room_state.cc
libircd_matrix_la_SOURCES += room_state_history.cc
libircd_matrix_la_SOURCES += room_state_space.cc
libircd_matrix_la_SOURCES += room_server_acl.cc
libircd_matrix_la_SOURCES += room_stats.cc
libircd_matrix_la_SOURCES += user.cc
libircd_matrix_la_SOURCES += user_account_data.cc
libircd_matrix_la_SOURCES += user_devices.cc
libircd_matrix_la_SOURCES += user_events.cc
libircd_matrix_la_SOURCES += user_filter.cc
libircd_matrix_la_SOURCES += user_ignores.cc
libircd_matrix_la_SOURCES += user_mitsein.cc
libircd_matrix_la_SOURCES += user_notifications.cc
libircd_matrix_la_SOURCES += user_profile.cc
libircd_matrix_la_SOURCES += user_pushers.cc
libircd_matrix_la_SOURCES += user_pushrules.cc
libircd_matrix_la_SOURCES += user_register.cc
libircd_matrix_la_SOURCES += user_room_account_data.cc
libircd_matrix_la_SOURCES += user_room_tags.cc
libircd_matrix_la_SOURCES += user_rooms.cc
libircd_matrix_la_SOURCES += user_tokens.cc
libircd_matrix_la_SOURCES += acquire.cc
libircd_matrix_la_SOURCES += app.cc
libircd_matrix_la_SOURCES += bridge.cc
libircd_matrix_la_SOURCES += breadcrumbs.cc
libircd_matrix_la_SOURCES += burst.cc
libircd_matrix_la_SOURCES += display_name.cc
libircd_matrix_la_SOURCES += event_append.cc
libircd_matrix_la_SOURCES += event_horizon.cc
libircd_matrix_la_SOURCES += events.cc
libircd_matrix_la_SOURCES += fed.cc
libircd_matrix_la_SOURCES += fed_well_known.cc
libircd_matrix_la_SOURCES += feds.cc
libircd_matrix_la_SOURCES += fetch.cc
libircd_matrix_la_SOURCES += gossip.cc
libircd_matrix_la_SOURCES += groups.cc
libircd_matrix_la_SOURCES += request.cc
libircd_matrix_la_SOURCES += keys.cc
libircd_matrix_la_SOURCES += node.cc
libircd_matrix_la_SOURCES += presence.cc
libircd_matrix_la_SOURCES += pretty.cc
libircd_matrix_la_SOURCES += receipt.cc
libircd_matrix_la_SOURCES += rooms.cc
libircd_matrix_la_SOURCES += membership.cc
libircd_matrix_la_SOURCES += rooms_summary.cc
libircd_matrix_la_SOURCES += sync.cc
libircd_matrix_la_SOURCES += typing.cc
libircd_matrix_la_SOURCES += users.cc
libircd_matrix_la_SOURCES += users_servers.cc
libircd_matrix_la_SOURCES += error.cc
libircd_matrix_la_SOURCES += push.cc
libircd_matrix_la_SOURCES += filter.cc
libircd_matrix_la_SOURCES += txn.cc
libircd_matrix_la_SOURCES += vm.cc
libircd_matrix_la_SOURCES += vm_eval.cc
libircd_matrix_la_SOURCES += vm_inject.cc
libircd_matrix_la_SOURCES += vm_execute.cc
libircd_matrix_la_SOURCES += vm_fetch.cc
libircd_matrix_la_SOURCES += init_backfill.cc
libircd_matrix_la_SOURCES += homeserver.cc
libircd_matrix_la_SOURCES += homeserver_bootstrap.cc
libircd_matrix_la_SOURCES += resource.cc
libircd_matrix_la_SOURCES += matrix.cc

#
# Unit configurations
#

id.lo: AM_CPPFLAGS   := ${SPIRIT_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
id.lo: CXXFLAGS      += ${SPIRIT_UNIT_CXXFLAGS} ${AM_CXXFLAGS}

#
# Other recipes
#

# LLVM PGO text to binary for -fprofile-use
default.profdata:
	-$(LLVM_PROFDATA) merge -output=default.profdata default.proftext
