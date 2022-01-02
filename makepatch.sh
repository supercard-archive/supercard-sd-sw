#!/bin/sh

set -e

timout() {
	tflag="--foreground"
	secs=30
	case "$romno" in
		1347|1415|1450|1510|1584|1669) secs=180 ;;
	esac
	timeout --help 2>&1 | grep BusyBox >/dev/null && tflag='-t'
	timeout -s ALRM $tflag $secs "$@"
}

md5() {
        md5sum "$1"|cut -d " " -f 1
}
equal() {
# running md5 on 2 files is a lot slower than the specially crafted file_equal
# binary, as the whole process is very slow already.
#        test $(md5 "$1") = $(md5 "$2")
	./file_equal "$1" "$2"
}
die() {
	echo "$1" >&2
	# this is the diff we use as "already done" marker
	rm -f "$outdir"/3.diff
	exit 1
}
truncat() {
./trunc -a 16 "$1"
}
dopatch() {
# autoit is a bit unstable so give it max 3 tries.
set +e
e=3
while test $e -gt 0 ; do
	WINEDEBUG=fixme-all,err-all timout wine /root/wine/autoit3/AutoIt3.exe autopatch.au3 "$romno" "$1" && break
	e=$((e - 1))
done
test $e = 0 && die "patching $inf failed"
set -e
}
dopatchall() {
# these roms are multiple-choice, but have been re-numbered in no-intro set 202111
# old rom numbers are here: https://sites.google.com/site/magnusdcp/romsgba
case $romno in
0110) romno=0111 ;;
0211) romno=0212 ;;
0324) romno=0333 ;;
0335) romno=0344 ;;
0972) romno=0988 ;;
0993) romno=1008 ;;
1004) romno=1020 ;;
1356) romno=1375 ;;
esac
# autoit is a bit unstable so give it max 3 tries.
set +e
e=3
while test $e -gt 0 ; do
	WINEDEBUG=fixme-all,err-all timout wine /root/wine/autoit3/AutoIt3.exe autopatch.au3 "$romno" && break
	e=$((e - 1))
done
for ix in `seq 0 3` ; do if ! test -e out/g$ix.gba ; then e=0 ; fi ; done
if test $e = 0 ; then
	echo "$code:$romno" >> failed
	die "patching $inf failed"
fi
set -e
}

dodiff() {
x=$1
outdiff="$outdir"/$x.diff
patched=out/g$x.gba
test $(./gameid $patched) = $code || die "patched file $patched mismatching game code"
$HD d g.gba out/g$x.gba > "$outdiff"
$HD p g.gba tmp.gba < "$outdiff"
equal out/g$x.gba tmp.gba || { die "patching inconsistent! $outdiff" ; }
}

bn=$(basename "$1")
romno=$(printf "%s\n" "$bn" | cut -b -4)

case "$1" in
*.gba) inf="$1" ;;
*.7z) test -d unpack && rm -f unpack/* || mkdir -p unpack ; 7zr x "$1" -ounpack 1>/dev/null 2>&1; mv unpack/*.gba unpack/x.gba ; inf=unpack/x.gba ;;
*) die "invalid file extension"
esac

HD=./haxdiff
code=$(./gameid "$inf")
echo "processing $romno $code ($1)"

outdir=patches/$code
mkdir -p "$outdir"
mkdir -p sramdiff
mkdir -p full
rm -f temp/*.tmp
rm -f out/g*.gba

if test -e "$outdir/3.diff"  ; then
	echo "patches already exist, nothing to do."
	exit 0
fi
rm -f out/g.sav

# first do patch 0 "enable saver" - which is basically the base patch supercardsd.exe does to all patching methods.
cp "$inf" "g.gba"
dopatchall
cp out/g0.gba tmp.gba
mv out/g*.gba full/

truncat g.gba
dopatchall

dodiff 0
tail -n1 < "$outdir"/0.diff | grep '^- ff' >/dev/null && die "patch0 removes padding" || true
equal out/g0.gba tmp.gba || die "patched roms differ after trunc"
cp out/g0.gba g.gba
md5 out/g.sav > "$outdir"/sav.md5sum

for step in `seq 1 3` ; do
# step 3 also always activates step2, avoid duplicating those
#dopatch $step
test $step = 3 && cp out/g2.gba g.gba
dodiff $step
done

outdir_orig=$outdir
outdir=temp_out
mkdir -p temp_out
cp "$inf" g.gba
cp g.gba g_trunc.gba
truncat g_trunc.gba
$HD p g_trunc.gba g_trunc0.gba < "$outdir_orig"/0.diff
$HD P g_trunc0.gba g_trunc1.gba < "$outdir_orig"/1.diff
$HD P g_trunc0.gba g_trunc2.gba < "$outdir_orig"/2.diff
$HD P g_trunc2.gba g_trunc3.gba < "$outdir_orig"/3.diff


# use untruncated input file and make sure we get the same result with our
# patch series.
for step in `seq 1 3` ; do
#dopatch $step
equal full/g$step.gba g_trunc$step.gba || die "untruncated patch result differ full/g$step.gba g_trunc$step.gba"
done

set +e
truncat g.gba
res=false
./srampatch 128 g.gba tmp.gba && res=true
if $res ; then
	echo "srampatch applied"
	$HD P g.gba trunc1.gba < "$outdir_orig"/1.diff
	if ! equal tmp.gba trunc1.gba ; then
		echo "warning srampatch differ $inf, saving diff to sramdiff/$code.diff"
		$HD d trunc1.gba tmp.gba > sramdiff/$code.diff
	fi
#	$HD d g.gba tmp.gba > tmp.diff
#	equal $outdir_orig/1.diff tmp.diff || { die "srampatch differ $inf" ; }
else
	echo "srampatch failed"
fi
exit 0
