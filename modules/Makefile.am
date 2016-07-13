AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = $(LTDLINCL)
AM_CPPFLAGS += -I$(top_srcdir)/include
AM_CPPFLAGS += -isystem $(top_srcdir)/boost/include
AM_LDFLAGS  = -module -export-dynamic -avoid-version -no-undefined -shared
AM_LDFLAGS += -export-symbols-regex _mheader
AM_LDFLAGS += -L$(top_srcdir)/boost/lib -lboost_system
AM_LDFLAGS += -L$(top_srcdir)/ircd -lircd
AM_LDFLAGS += -L$(top_srcdir)/rb -lrb

auto_load_moddir=@moduledir@/autoload

cap_account_tag_la_SOURCES = cap_account_tag.cc
cap_server_time_la_SOURCES = cap_server_time.cc
chm_nocolour_la_SOURCES = chm_nocolour.cc
chm_noctcp_la_SOURCES = chm_noctcp.cc
m_accept_la_SOURCES = m_accept.cc
m_admin_la_SOURCES = m_admin.cc
m_alias_la_SOURCES = m_alias.cc
m_away_la_SOURCES = m_away.cc
m_cap_la_SOURCES = m_cap.cc
m_capab_la_SOURCES = m_capab.cc
m_certfp_la_SOURCES = m_certfp.cc
m_challenge_la_SOURCES = m_challenge.cc
m_chghost_la_SOURCES = m_chghost.cc
m_close_la_SOURCES = m_close.cc
m_connect_la_SOURCES = m_connect.cc
m_dline_la_SOURCES = m_dline.cc
m_encap_la_SOURCES = m_encap.cc
m_etrace_la_SOURCES = m_etrace.cc
m_grant_la_SOURCES = m_grant.cc
m_help_la_SOURCES = m_help.cc
m_info_la_SOURCES = m_info.cc
m_invite_la_SOURCES = m_invite.cc
m_ison_la_SOURCES = m_ison.cc
m_kline_la_SOURCES = m_kline.cc
m_knock_la_SOURCES = m_knock.cc
m_links_la_SOURCES = m_links.cc
m_list_la_SOURCES = m_list.cc
m_lusers_la_SOURCES = m_lusers.cc
m_map_la_SOURCES = m_map.cc
m_monitor_la_SOURCES = m_monitor.cc
m_motd_la_SOURCES = m_motd.cc
m_names_la_SOURCES = m_names.cc
m_oper_la_SOURCES = m_oper.cc
m_operspy_la_SOURCES = m_operspy.cc
m_pass_la_SOURCES = m_pass.cc
m_ping_la_SOURCES = m_ping.cc
m_pong_la_SOURCES = m_pong.cc
m_post_la_SOURCES = m_post.cc
m_privs_la_SOURCES = m_privs.cc
m_rehash_la_SOURCES = m_rehash.cc
m_restart_la_SOURCES = m_restart.cc
m_resv_la_SOURCES = m_resv.cc
m_sasl_la_SOURCES = m_sasl.cc
m_scan_la_SOURCES = m_scan.cc
m_services_la_SOURCES = m_services.cc
m_set_la_SOURCES = m_set.cc
m_signon_la_SOURCES = m_signon.cc
m_snote_la_SOURCES = m_snote.cc
m_starttls_la_SOURCES = m_starttls.cc
m_stats_la_SOURCES = m_stats.cc
m_svinfo_la_SOURCES = m_svinfo.cc
m_tb_la_SOURCES = m_tb.cc
m_testline_la_SOURCES = m_testline.cc
m_testmask_la_SOURCES = m_testmask.cc
m_tginfo_la_SOURCES = m_tginfo.cc
m_time_la_SOURCES = m_time.cc
m_topic_la_SOURCES = m_topic.cc
m_trace_la_SOURCES = m_trace.cc
m_unreject_la_SOURCES = m_unreject.cc
m_user_la_SOURCES = m_user.cc
m_userhost_la_SOURCES = m_userhost.cc
m_users_la_SOURCES = m_users.cc
m_version_la_SOURCES = m_version.cc
m_wallops_la_SOURCES = m_wallops.cc
m_who_la_SOURCES = m_who.cc
m_whois_la_SOURCES = m_whois.cc
m_whowas_la_SOURCES = m_whowas.cc
m_xline_la_SOURCES = m_xline.cc
sno_routing_la_SOURCES = sno_routing.cc

auto_load_mod_LTLIBRARIES = \
  cap_account_tag.la \
  cap_server_time.la \
  chm_nocolour.la \
  chm_noctcp.la \
  m_accept.la \
  m_admin.la \
  m_alias.la \
  m_away.la \
  m_cap.la \
  m_capab.la \
  m_certfp.la \
  m_challenge.la \
  m_chghost.la \
  m_close.la \
  m_connect.la \
  m_dline.la \
  m_encap.la \
  m_etrace.la \
  m_grant.la \
  m_help.la \
  m_info.la \
  m_invite.la \
  m_ison.la \
  m_kline.la \
  m_knock.la \
  m_links.la \
  m_list.la \
  m_lusers.la \
  m_map.la \
  m_monitor.la \
  m_motd.la \
  m_names.la \
  m_oper.la \
  m_operspy.la \
  m_pass.la \
  m_ping.la \
  m_pong.la \
  m_post.la \
  m_privs.la \
  m_rehash.la \
  m_restart.la \
  m_resv.la \
  m_sasl.la \
  m_scan.la \
  m_services.la \
  m_set.la \
  m_signon.la \
  m_snote.la \
  m_starttls.la \
  m_stats.la \
  m_svinfo.la \
  m_tb.la \
  m_testline.la \
  m_testmask.la \
  m_tginfo.la \
  m_time.la \
  m_topic.la \
  m_trace.la \
  m_unreject.la \
  m_user.la \
  m_userhost.la \
  m_users.la \
  m_version.la \
  m_wallops.la \
  m_who.la \
  m_whois.la \
  m_whowas.la \
  m_xline.la \
  sno_routing.la


core_m_ban_la_SOURCES = core/m_ban.cc
core_m_die_la_SOURCES = core/m_die.cc
core_m_error_la_SOURCES = core/m_error.cc
core_m_join_la_SOURCES = core/m_join.cc
core_m_kick_la_SOURCES = core/m_kick.cc
core_m_kill_la_SOURCES = core/m_kill.cc
core_m_message_la_SOURCES = core/m_message.cc
core_m_mode_la_SOURCES = core/m_mode.cc
core_m_modules_la_SOURCES = core/m_modules.cc
core_m_nick_la_SOURCES = core/m_nick.cc
core_m_part_la_SOURCES = core/m_part.cc
core_m_quit_la_SOURCES = core/m_quit.cc
core_m_server_la_SOURCES = core/m_server.cc
core_m_squit_la_SOURCES = core/m_squit.cc


module_LTLIBRARIES = \
  core/m_ban.la \
  core/m_die.la \
  core/m_error.la \
  core/m_join.la \
  core/m_kick.la \
  core/m_kill.la \
  core/m_message.la \
  core/m_mode.la \
  core/m_modules.la \
  core/m_nick.la \
  core/m_part.la \
  core/m_quit.la \
  core/m_server.la \
  core/m_squit.la


mrproper-local:
	rm -rf core/.deps
	rm -f core/.dirstamp
