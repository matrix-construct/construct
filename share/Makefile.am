################################################################################
#
# IRCd read-only data (share) assets.
#

datadir = @datadir@

install-mkdirs:
	-@if test ! -d $(DESTDIR)$(datadir); then \
		echo "mkdir -p $(DESTDIR)$(datadir)"; \
		mkdir -p $(DESTDIR)$(datadir); \
	fi

install: install-mkdirs
	@echo "ircd: installing shared assets"
	@for i in $(srcdir)/*; do \
		case "$$i" in \
			"./Makefile"*) \
				;; \
			"README.md") \
				;; \
			*) \
				echo "installing $$i to $(DESTDIR)$(datadir)"; \
				cp -u -r $$i $(DESTDIR)$(datadir); \
				;; \
		esac \
	done
