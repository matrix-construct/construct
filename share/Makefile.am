################################################################################
#
# IRCd read-only data (share) assets.
#

datadir = @datadir@/$(PACKAGE_TARNAME)

install-mkdirs:
	-@if test ! -d $(datadir); then \
		echo "mkdir -p $(datadir)"; \
		mkdir -p $(datadir); \
	fi

install: install-mkdirs
	@echo "ircd: installing shared assets"
	@for i in $(srcdir)/*; do \
		if [[ "$$i" != "./Makefile"* ]] && [[ "$$i" != "README.md" ]]; then \
			echo "installing $$i to $(DESTDIR)$(datadir)"; \
			cp -r $$i $(DESTDIR)$(datadir); \
		fi \
	done
