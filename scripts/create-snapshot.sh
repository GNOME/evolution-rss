#!/bin/bash

MAJOR_VERSION=0.2.1
rm -Rf /tmp/evolution-rss
git clone git://git.gnome.org/evolution-rss /tmp/evolution-rss
cd /tmp/evolution-rss
git_version=`git log --oneline -1|cut -d" " -f1`
gitdate=`date +%Y%m%d`
git archive --format=tar --prefix=evolution-rss-${MAJOR_VERSION}/ ${git_version} | gzip > /tmp/evolution-rss-${MAJOR_VERSION}-${gitdate}.tar.gz
echo "HEAD: "`git log -1 --pretty=format:%h`
