extensiondir=@moduledir@

AM_CPPFLAGS = \
	 -I$(top_srcdir)/include \
	$(LTDLINCL) \
	@BOOST_CPPFLAGS@ \
	-include $(top_srcdir)/include/ircd/ircd.h

if BUILD_PCH
endif

AM_LDFLAGS = \
	-L$(top_srcdir)/ircd \
	-L$(top_srcdir)/rb \
	@BOOST_LDFLAGS@

AM_LDFLAGS += \
	-module \
	-shared \
	-export-dynamic \
	-avoid-version \
	-no-undefined \
	--no-allow-shlib-undefined \
	-export-symbols-regex _mheader

AM_LDFLAGS += \
	-lircd \
	-lrb \
	@LIBLTDL@ \
	@BOOST_LIBS@


#chm_adminonly_la_SOURCES = chm_adminonly.cc
#chm_operonly_la_SOURCES = chm_operonly.cc
#chm_operonly_compat_la_SOURCES = chm_operonly_compat.cc
#chm_insecure_la_SOURCES = chm_insecure.cc
#chm_nonotice_la_SOURCES = chm_nonotice.cc
#chm_operpeace_la_SOURCES = chm_operpeace.cc
#chm_quietunreg_compat_la_SOURCES = chm_quietunreg_compat.cc
#chm_spamfilter_la_SOURCES = chm_spamfilter.cc
#chm_sslonly_la_SOURCES = chm_sslonly.cc
#chm_sslonly_compat_la_SOURCES = chm_sslonly_compat.cc
#createauthonly_la_SOURCES = createauthonly.cc
#createoperonly_la_SOURCES = createoperonly.cc
#extb_account_la_SOURCES = extb_account.cc
#extb_canjoin_la_SOURCES = extb_canjoin.cc
#extb_channel_la_SOURCES = extb_channel.cc
#extb_hostmask_la_SOURCES = extb_hostmask.cc
#extb_oper_la_SOURCES = extb_oper.cc
#extb_server_la_SOURCES = extb_server.cc
#extb_ssl_la_SOURCES = extb_ssl.cc
#extb_realname_la_SOURCES = extb_realname.cc
#extb_usermode_la_SOURCES = extb_usermode.cc
#extb_extgecos_la_SOURCES = extb_extgecos.cc
#extb_combi_la_SOURCES = extb_combi.cc
#force_user_invis_la_SOURCES = force_user_invis.cc
#helpops_la_SOURCES = helpops.cc
#hurt_la_SOURCES = hurt.cc
#ip_cloaking_la_SOURCES = ip_cloaking.cc
#override_la_SOURCES = override.cc
#restrict_unauthenticated_la_SOURCES = restrict-unauthenticated.cc
#sno_channelcreate_la_SOURCES = sno_channelcreate.cc
#sno_farconnect_la_SOURCES = sno_farconnect.cc
#sno_globalkline_la_SOURCES = sno_globalkline.cc
#sno_globalnickchange_la_SOURCES = sno_globalnickchange.cc
#sno_globaloper_la_SOURCES = sno_globaloper.cc
#sno_whois_la_SOURCES = sno_whois.cc
#umode_noctcp_la_SOURCES = umode_noctcp.cc
#m_adminwall_la_SOURCES = m_adminwall.cc
#m_echotags_la_SOURCES = m_echotags.cc
#m_extendchans_la_SOURCES = m_extendchans.cc
#m_findforwards_la_SOURCES = m_findforwards.cc
#m_identify_la_SOURCES = m_identify.cc
#m_locops_la_SOURCES = m_locops.cc
#m_mkpasswd_la_SOURCES = m_mkpasswd.cc
#m_ojoin_la_SOURCES = m_ojoin.cc
#m_okick_la_SOURCES = m_okick.cc
#m_omode_la_SOURCES = m_omode.cc
#m_opme_la_SOURCES = m_opme.cc
#m_sendbans_la_SOURCES = m_sendbans.cc
#m_webirc_la_SOURCES = m_webirc.cc
#m_remove_la_SOURCES = m_remove.cc
#m_roleplay_la_SOURCES = m_roleplay.cc
#hide_uncommon_channels_la_SOURCES = hide_uncommon_channels.cc
#no_kill_services_la_SOURCES = no_kill_services.cc
#no_locops_la_SOURCES = no_locops.cc
#no_oper_invis_la_SOURCES = no_oper_invis.cc
#spamfilter_nicks_la_SOURCES = spamfilter_nicks.cc
#spy_admin_notice_la_SOURCES = spy_admin_notice.cc
#spy_info_notice_la_SOURCES = spy_info_notice.cc
#spy_links_notice_la_SOURCES = spy_links_notice.cc
#spy_motd_notice_la_SOURCES = spy_motd_notice.cc
#spy_stats_notice_la_SOURCES = spy_stats_notice.cc
#spy_stats_p_notice_la_SOURCES = spy_stats_p_notice.cc
#spy_trace_notice_la_SOURCES = spy_trace_notice.cc
#example_module_la_SOURCES = example_module.cc


#extension_LTLIBRARIES =		\
#  chm_adminonly.la		\
#  chm_operonly.la		\
#  chm_operonly_compat.la	\
#  chm_insecure.la		\
#  chm_nonotice.la		\
#  chm_operpeace.la		\
#  chm_quietunreg_compat.la	\
#  chm_spamfilter.la		\
#  chm_sslonly.la		\
#  chm_sslonly_compat.la		\
#  createauthonly.la		\
#  createoperonly.la		\
#  extb_account.la		\
#  extb_canjoin.la		\
#  extb_channel.la		\
#  extb_hostmask.la		\
#  extb_oper.la			\
#  extb_server.la		\
#  extb_ssl.la			\
#  extb_realname.la		\
#  extb_usermode.la		\
#  extb_extgecos.la		\
#  extb_combi.la			\
#  force_user_invis.la		\
#  helpops.la			\
#  hurt.la			\
#  ip_cloaking.la		\
#  override.la			\
#  restrict-unauthenticated.la    \
#  sno_channelcreate.la		\
#  sno_farconnect.la		\
#  sno_globalkline.la		\
#  sno_globalnickchange.la	\
#  sno_globaloper.la		\
#  sno_whois.la			\
#  umode_noctcp.la		\
#  m_adminwall.la		\
#  m_echotags.la			\
#  m_extendchans.la		\
#  m_findforwards.la		\
#  m_identify.la			\
#  m_locops.la			\
#  m_mkpasswd.la			\
#  m_ojoin.la			\
#  m_okick.la			\
#  m_omode.la			\
#  m_opme.la			\
#  m_sendbans.la			\
#  m_webirc.la			\
#  m_remove.la			\
#  m_roleplay.la			\
#  hide_uncommon_channels.la	\
#  no_kill_services.la		\
#  no_locops.la			\
#  no_oper_invis.la		\
#  spamfilter_nicks.la		\
#  spy_admin_notice.la		\
#  spy_info_notice.la		\
#  spy_links_notice.la		\
#  spy_motd_notice.la		\
#  spy_stats_notice.la		\
#  spy_stats_p_notice.la		\
#  spy_trace_notice.la		\
#  example_module.la
#
#if PCRE
#
#extension_LTLIBRARIES += spamfilter_expr.la
#spamfilter_expr_la_SOURCES = spamfilter_expr.cc
#spamfilter_expr_la_CPPFLAGS = $(AM_CPPFLAGS) $(PCRE_CFLAGS)
#spamfilter_expr_la_LIBADD = $(PCRE_LIBS)
#
#endif
#
