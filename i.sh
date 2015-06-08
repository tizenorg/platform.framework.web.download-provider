#!/bin/bash

sdb root on
sdb shell change-booting-mode.sh --update

sdb -d push /home/youngj/GBS-ROOT/local/repos/tizen/armv7l/RPMS/org.tizen.browser* /root

sdb shell rpm -e --nodeps org.tizen.browser
sdb shell pkgcmd -i -t rpm -p /root/org.tizen.browser-4*

