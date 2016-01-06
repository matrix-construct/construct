# $Id: Makefile.in 3376 2007-04-03 11:37:39Z nenolod $
prefix		= @prefix@
exec_prefix	= @exec_prefix@
exec_suffix	= @exec_suffix@
bindir		= @bindir@
libexecdir	= @libexecdir@
sysconfdir		= @sysconfdir@
localstatedir	= @localstatedir@

# Local to the etc Makefile
mandir          = @mandir@/man8
MANPAGES        = ircd.8

CONFS		= ircd.conf.example reference.conf

install-mkdirs:
	-@if test ! -d $(DESTDIR)$(sysconfdir); then \
		echo "mkdir -p $(sysconfdir)"; \
		mkdir -p $(DESTDIR)$(sysconfdir); \
	fi

	-@if test ! -d $(DESTDIR)$(mandir); then \
		echo "mkdir -p $(mandir)"; \
		mkdir -p $(DESTDIR)$(mandir); \
	fi

install: install-mkdirs
	@echo "ircd: installing example config files ($(CONFS))"
	@for i in $(CONFS); do \
		if test -f $(DESTDIR)$(sysconfdir)/$$i; then \
			$(MV) $(DESTDIR)$(sysconfdir)/$$i $(DESTDIR)$(sysconfdir)/$$i.old; \
		fi; \
		$(INSTALL_DATA) $$i $(DESTDIR)$(sysconfdir); \
	done

	-@if test ! -f $(DESTDIR)$(sysconfdir)/ircd.motd; then \
		echo "ircd: installing motd file (ircd.motd)"; \
		$(INSTALL_DATA) ircd.motd $(DESTDIR)$(sysconfdir); \
	fi

	-@if test -f $(DESTDIR)$(sysconfdir)/links.txt; then \
		$(RM) $(DESTDIR)$(sysconfdir)/links.txt; \
	fi

	@echo "ircd: installing manpage"
	@for i in $(MANPAGES); do \
		if test ! -f $(DESTDIR)$(mandir)/$(PROGRAM_PREFIX)$$i; then \
			$(INSTALL_DATA) $$i $(DESTDIR)$(mandir)/$(PROGRAM_PREFIX)$$i; \
		fi; \
	done
