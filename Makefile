all: tablet-mode-switch

tablet-mode-switch:
	gcc -o tablet-mode-switch hotkey.c -levdev -ludev -I/usr/include/libevdev-1.0

install: tablet-mode-switch
	-systemctl stop tablet-mode-switch
	cp tablet-mode-switch /usr/local/sbin

install-service: install
	-systemctl disable tablet-mode-switch.service
	cp tablet-mode-switch.service /etc/systemd/systemctl
	systemctl enable tablet-mode-switch.service

clean:
	-rm tablet-mode-switch
	-rm tablet-mode-test

uninstall:
	-systemctl stop tablet-mode-switch.service
	-systemctl disable tablet-mode-switch.service
	-rm /usr/local/sbin/tablet-mode-switch

test:
	gcc -o tablet-mode-test test.c -levdev -ludev -I/usr/include/libevdev-1.0
