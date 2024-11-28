#! /bin/bash
#
rev_new_raw=$(svnversion -n . 2>/dev/null | tr '\n' ' ' | tr -d '\r')
[ -n "$rev_new_raw" ] || rev_new_raw=$(SubWCRev . 2>/dev/null | tr '\n' ' ' | tr -d '\r')

[ "${#rev_new_raw}" != "5" ] && rev_new_raw=$(cat version.txt) && skip_ver_bump="true"

rev_new_raw=$(echo $rev_new_raw | sed 's/[^0-9]*\([0-9]*\)\(.*\)/\1 \2/')
rev_new=0
a=$(echo $rev_new_raw | sed 's/\([0-9]*\).*/\1/')
let "a+=0"
#find max rev
while [ "$a" ]; do
	[ "$a" -gt "$rev_new" ] && rev_new=$a
	rev_new_raw=$(echo -n $rev_new_raw | sed 's/[0-9]*[^0-9]*\([0-9]*\)\(.*\)/\1 \2/')
	a=$(echo $rev_new_raw | sed 's/\([0-9]*\).*/\1/')
done

rev_old=$(cat ./source/svnrev.c 2>/dev/null | tr -d '\n' | sed 's/[^0-9]*\([0-9]*\).*/\1/')

if [ "$rev_new" != "$rev_old" ] || [ ! -f ./source/svnrev.c ]; then

	cat <<EOF > ./source/svnrev.c
#define SVN_REV "$rev_new"

const char *GetRev()
{
	return SVN_REV;
}
EOF

	if [ -z "$rev_old" ]; then
		echo "Created svnrev.c and set the revision to $rev_new" >&2
	else
		echo "Changed the revision from $rev_old to $rev_new" >&2
	fi
fi


[ "$skip_ver_bump" != "true" ] && rev_new=`expr $rev_new + 1`
rev_date=`date -u +%Y%m%d%H%M%S`

cat <<EOF > ./hbc/meta.xml
<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<app version="1">
	<name>USB Loader GX</name>
	<coder>USB Loader GX Team</coder>
	<version>3.0.$rev_new</version>
	<release_date>$rev_date</release_date>
	<!-- remove this line to enable arguments
	<arguments>
		<arg>--ios=249</arg>
		<arg>--bootios=58</arg>
		<arg>--usbport=0</arg>
		<arg>--mountusb=1</arg>
		<arg>--sdmode=0</arg>
	</arguments>
	remove this line to enable arguments -->
	<ahb_access/>
	<short_description>Game Loader</short_description>
	<long_description>Play Wii disc games from USB storage devices.</long_description>
</app>
EOF
