AUTOMAKE_OPTIONS = foreign

DEFS += \
	-DIRCD_UNIT \
	-DIRCD_MATRIX_UNIT \
	###

AM_CXXFLAGS = \
	-ftls-model=global-dynamic \
	@EXTRA_CXXFLAGS@ \
	###

if DEBUG
if GCC
AM_CXXFLAGS += -fmax-errors=2
endif
endif

AM_CPPFLAGS = \
	-I$(top_srcdir)/include \
	-include ircd/ircd.pic.h \
	-include ircd/mods/mapi.h \
	@EXTRA_CPPFLAGS@ \
	###

AM_LDFLAGS = \
	-version-info 0:1:0 \
	-Wl,--no-undefined-version \
	-Wl,--allow-shlib-undefined \
	-Wl,--unresolved-symbols=ignore-in-shared-libs \
	-Wl,-z,now \
	-L$(top_srcdir)/ircd \
	$(PLATFORM_LDFLAGS) \
	@EXTRA_LDFLAGS@ \
	###

AM_LIBS = \
	-lircd \
	###

if MINGW
AM_LDFLAGS += \
	-Wl,--enable-runtime-pseudo-reloc \
	###
endif

libircd_matrixdir = @libdir@
libircd_matrix_LTLIBRARIES = \
	libircd_matrix.la \
	###

#
# Unit list
#

libircd_matrix_la_SOURCES =#
libircd_matrix_la_SOURCES += name.cc
libircd_matrix_la_SOURCES += id.cc
libircd_matrix_la_SOURCES += dbs.cc
libircd_matrix_la_SOURCES += matrix.cc
libircd_matrix_la_SOURCES += event.cc
libircd_matrix_la_SOURCES += room.cc
libircd_matrix_la_SOURCES += user.cc
libircd_matrix_la_SOURCES += breadcrumb_rooms.cc
libircd_matrix_la_SOURCES += create.cc
libircd_matrix_la_SOURCES += device.cc
libircd_matrix_la_SOURCES += display_name.cc
libircd_matrix_la_SOURCES += event_append.cc
libircd_matrix_la_SOURCES += event_horizon.cc
libircd_matrix_la_SOURCES += events.cc
libircd_matrix_la_SOURCES += fed.cc
libircd_matrix_la_SOURCES += feds.cc
libircd_matrix_la_SOURCES += fetch.cc
libircd_matrix_la_SOURCES += ignored_user_list.cc
libircd_matrix_la_SOURCES += init_backfill.cc
libircd_matrix_la_SOURCES += init_bootstrap.cc
libircd_matrix_la_SOURCES += join.cc
libircd_matrix_la_SOURCES += keys.cc
libircd_matrix_la_SOURCES += leave.cc
libircd_matrix_la_SOURCES += node.cc
libircd_matrix_la_SOURCES += presence.cc
libircd_matrix_la_SOURCES += pretty.cc
libircd_matrix_la_SOURCES += receipt.cc
libircd_matrix_la_SOURCES += room_aliases.cc
libircd_matrix_la_SOURCES += room_auth.cc
libircd_matrix_la_SOURCES += room_bootstrap.cc
libircd_matrix_la_SOURCES += room_events.cc
libircd_matrix_la_SOURCES += room_head.cc
libircd_matrix_la_SOURCES += room_history_visibility.cc
libircd_matrix_la_SOURCES += rooms.cc
libircd_matrix_la_SOURCES += room_server_acl.cc
libircd_matrix_la_SOURCES += rooms_summary.cc
libircd_matrix_la_SOURCES += typing.cc
libircd_matrix_la_SOURCES += user_account_data.cc
libircd_matrix_la_SOURCES += user_events.cc
libircd_matrix_la_SOURCES += user_filter.cc
libircd_matrix_la_SOURCES += user_highlight.cc
libircd_matrix_la_SOURCES += user_mitsein.cc
libircd_matrix_la_SOURCES += user_profile.cc
libircd_matrix_la_SOURCES += user_register.cc
libircd_matrix_la_SOURCES += user_room_account_data.cc
libircd_matrix_la_SOURCES += user_rooms.cc
libircd_matrix_la_SOURCES += user_room_tags.cc
libircd_matrix_la_SOURCES += users.cc
libircd_matrix_la_SOURCES += users_servers.cc
libircd_matrix_la_SOURCES += conf.cc

#
# Unit configurations
#

id.lo: AM_CPPFLAGS   := ${SPIRIT_UNIT_CPPFLAGS} ${AM_CPPFLAGS}
id.lo: CXXFLAGS      += ${SPIRIT_UNIT_CXXFLAGS} ${AM_CXXFLAGS}
