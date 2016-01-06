AUTOMAKE_OPTIONS = subdir-objects

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libratbox/include
AM_LDFLAGS  = -module -export-dynamic -avoid-version -no-undefined -shared
AM_LDFLAGS += -export-symbols-regex _mheader
LIBS += $(top_srcdir)/libratbox/src/libratbox.la $(top_srcdir)/src/libcore.la

auto_load_moddir=@moduledir@/autoload

auto_load_mod_LTLIBRARIES = \
  chm_nocolour.la \
  chm_noctcp.la \
  m_accept.la \
  m_admin.la \
  m_away.la \
  m_cap.la \
  m_capab.la \
  m_certfp.la \
  m_challenge.la \
  m_chghost.la \
  m_close.la \
  m_cmessage.la \
  m_connect.la \
  m_dline.la \
  m_encap.la \
  m_etrace.la \
  m_help.la \
  m_info.la \
  m_invite.la \
  m_ison.la \
  m_kline.la \
  m_knock.la \
  m_links.la \
  m_list.la \
  m_locops.la \
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

module_LTLIBRARIES = \
  core/m_ban.la \
  core/m_die.la \
  core/m_error.la \
  core/m_join.la \
  core/m_kick.la \
  core/m_kill.la \
  core/m_message.la \
  core/m_mode.la \
  core/m_nick.la \
  core/m_part.la \
  core/m_quit.la \
  core/m_server.la \
  core/m_squit.la
