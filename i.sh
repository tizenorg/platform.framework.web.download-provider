#!/bin/bash

sdb root on
sdb shell change-booting-mode.sh --update

sdb -d push /home/choi/GBS-ROOT-3.0-MOBILE-arm-wayland/local/repos/tzmo_v3.0_arm_wayland_mirror/armv7l/RPMS/org.tizen.browser* /root

sdb shell rpm -e --nodeps org.tizen.browser
sdb shell pkgcmd -i -t rpm -p /root/org.tizen.browser-4*

