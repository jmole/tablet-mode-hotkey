# Makefile for tablet-mode-hotkey (Fedora-friendly)

PREFIX    ?= /usr/local
BINDIR    ?= $(PREFIX)/sbin
UNITDIR   ?= /etc/systemd/system
MODPROBED ?= /etc/modprobe.d
BLACKLIST ?= $(MODPROBED)/tablet-mode-hotkey-blacklist.conf
SYSTEMCTL ?= systemctl
MODPROBE  ?= modprobe
CC        ?= gcc

# Use pkg-config if available; fall back to plain flags.
CFLAGS   ?= -O2 -Wall $(shell pkg-config --cflags libudev 2>/dev/null)
LIBS     ?= $(shell pkg-config --libs libudev 2>/dev/null)
ifeq ($(strip $(LIBS)),)
LIBS = -ludev
endif

.PHONY: all install install-service clean uninstall

all: tablet-mode-switch

tablet-mode-switch: hotkey.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

install: tablet-mode-switch
	-$(SYSTEMCTL) stop tablet-mode-switch.service
	install -Dm0755 tablet-mode-switch $(DESTDIR)$(BINDIR)/tablet-mode-switch
	# Blacklist intel_hid so it doesn't autoload
	printf '%s\n' 'blacklist intel_hid' | install -Dm0644 /dev/stdin $(DESTDIR)$(BLACKLIST)
	# If it's currently loaded, try to remove it (ignore errors)
	-$(MODPROBE) -r intel_hid || true

install-service: install
	sed 's|/usr/local/sbin|$(PREFIX)/sbin|g' tablet-mode-switch.service | \
		install -Dm0644 /dev/stdin $(DESTDIR)$(UNITDIR)/tablet-mode-switch.service
	$(SYSTEMCTL) daemon-reload
	-$(SYSTEMCTL) enable --now tablet-mode-switch.service

clean:
	-rm -f tablet-mode-switch

uninstall:
	-$(SYSTEMCTL) disable --now tablet-mode-switch.service
	-rm -f $(DESTDIR)$(UNITDIR)/tablet-mode-switch.service
	$(SYSTEMCTL) daemon-reload
	-rm -f $(DESTDIR)$(BINDIR)/tablet-mode-switch
	# Remove blacklist and (best effort) load intel_hid again
	-rm -f $(DESTDIR)$(BLACKLIST)
	-$(MODPROBE) intel_hid || true

