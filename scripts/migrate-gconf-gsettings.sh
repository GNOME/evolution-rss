#!/bin/bash

a=""
IFS=
XMLFEEDS=`gconftool-2 -g /apps/evolution/evolution-rss/feeds`
for i in $XMLFEEDS; do
if [ -z $a ]; then
	a=$a$i
else
	a=$a"@"$i
fi
#a=$a"'"`echo $i|sed -e "s;\[;;g" -e "s;\];;g"`"';"
#|grep -o -e "<url>.*</url>"|sed -e "s;<url>;;" -e "s;</url>;;"
done
#echo $a
#exit
IFS=$'\n'
#b='['$a']'
b=`echo $a|sed -e "s!\@!,!g"`
echo $b
#exit
dconf write /org/gnome/evolution/plugin/rss/feeds \"$a\"
