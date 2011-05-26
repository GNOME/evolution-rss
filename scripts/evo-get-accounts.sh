#!/bin/bash

XMLFEEDS=`gconftool-2 -g /apps/evolution/mail/accounts`
for i in $XMLFEEDS; do
	echo $i
#|grep -o -e "<url>.*</url>"|sed -e "s;<url>;;" -e "s;</url>;;"
done

