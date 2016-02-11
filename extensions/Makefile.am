AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)/libratbox/include $(LTDLINCL)
AM_LDFLAGS  = -module -export-dynamic -avoid-version -no-undefined -shared
AM_LDFLAGS += -export-symbols-regex _mheader
LIBS += $(top_srcdir)/libratbox/src/libratbox.la $(top_srcdir)/ircd/libircd.la

extensiondir=@moduledir@/extensions

extension_LTLIBRARIES =		\
  chm_adminonly.la		\
  chm_operonly.la		\
  chm_operonly_compat.la		\
  chm_insecure.la		\
  chm_nonotice.la		\
  chm_operpeace.la		\
  chm_quietunreg_compat.la	\
  chm_sslonly.la			\
  chm_sslonly_compat.la		\
  createauthonly.la		\
  createoperonly.la		\
  extb_account.la		\
  extb_canjoin.la		\
  extb_channel.la		\
  extb_hostmask.la		\
  extb_oper.la			\
  extb_server.la			\
  extb_ssl.la			\
  extb_realname.la		\
  extb_usermode.la		\
  extb_extgecos.la		\
  extb_combi.la			\
  force_user_invis.la		\
  helpops.la			\
  hurt.la			\
  ip_cloaking.la			\
  ip_cloaking_old.la		\
  ip_cloaking_3.0.la		\
  ip_cloaking_4.0.la		\
  override.la			\
  restrict-unauthenticated.la    \
  sno_channelcreate.la		\
  sno_farconnect.la		\
  sno_globalkline.la		\
  sno_globalnickchange.la	\
  sno_globaloper.la		\
  sno_whois.la			\
  m_42.la			\
  m_adminwall.la			\
  m_echotags.la			\
  m_extendchans.la		\
  m_findforwards.la		\
  m_identify.la			\
  m_mkpasswd.la                  \
  m_ojoin.la			\
  m_okick.la			\
  m_omode.la			\
  m_opme.la			\
  m_sendbans.la			\
  m_webirc.la			\
  m_remove.la			\
  m_roleplay.la			\
  hide_uncommon_channels.la	\
  no_kill_services.la		\
  no_locops.la			\
  no_oper_invis.la		\
  spy_admin_notice.la		\
  spy_info_notice.la		\
  spy_links_notice.la		\
  spy_motd_notice.la		\
  spy_stats_notice.la            \
  spy_stats_p_notice.la		\
  spy_trace_notice.la		\
  example_module.la
