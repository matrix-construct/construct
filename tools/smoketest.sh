#!/bin/sh
# Simple test that compiles and runs ircd, verifying that certain aspects of
# PRIVMSG work.

case $# in
1) ;;
*) printf 'Usage: %s tarball\n' "$0" >&2; exit 64 ;;
esac
cr=$(printf '\r')
rc=0 ircdpid= tarball=${1:?}
case $tarball in
/*)	;;
*)	tarball=$PWD/$tarball ;;
esac
dir=$(mktemp -d "${TMPDIR:-/tmp}/ircdtest.XXXXXXXXXX") || exit 2
trap '[ -z "$ircdpid" ] || kill $ircdpid; rm -rf "$dir"' 0
cd "$dir" || exit 2
tar xjf "$tarball" || exit 2
srcdir=${tarball##*/}
srcdir=${srcdir%.tbz2}
srcdir=${srcdir%.tgz}
srcdir=${srcdir%.tar.*}
srcdir=${srcdir%.tar}
cd "$srcdir" || exit 2
prefix=$dir/prefix
./configure --prefix="$prefix" >"$dir/out" 2>&1 || { cat "$dir/out"; exit 2; }
make -j2 >"$dir/out" 2>&1 || { cat "$dir/out"; exit 2; }
make install >"$dir/out" 2>&1 || { cat "$dir/out"; exit 2; }
cd "$prefix" || exit 2
servername=smoke$(date +%Y%m%d%H%M%S).test
port=$(date +50%S)
sed -e '/^serverinfo/,/^}/s/name = ".*";/name = "'"$servername"'";/' \
	-e '/^listen/,/^}/s/port = .*;/port = '"$port"';/' \
	-e '/^blacklist/,/^}/s/^/#/' \
	etc/ircd.conf.example >etc/ircd.conf || exit 2
bin/ircd || exit 2
ircdpid=$(cat etc/ircd.pid) || exit 2
cd "$dir" || exit 2
echo "Will use servername $servername port $port, pid is $ircdpid"
{
	echo 'USER testu . . :Test user'
	echo 'NICK test1'
	sleep 1
	echo 'JOIN #test'
	sleep 2
	echo "PRIVMSG #test :channel message via $servername"
	echo "PRIVMSG @#test :chanops 1 via $servername"
	echo "MODE #test +o test2"
	echo "PRIVMSG @#test :chanops 2 via $servername"
	sleep 1
	echo "QUIT"
} | nc 127.0.0.1 "$port" >out1 &
{
	echo 'NICK test2'
	echo 'USER testu2 . . :Test user'
	sleep 2
	echo 'JOIN #test'
	echo "PRIVMSG test1 :private message via $servername"
	sleep 2
	echo "QUIT :Bye"
} | nc 127.0.0.1 "$port" >out2 &
wait
if ! grep -q "^:$servername 001 test1 :" out1; then
	echo "FAIL: Missing 001 in out1 or wrong server"
	rc=1
fi
if ! grep -q "^:$servername 001 test2 :" out2; then
	echo "FAIL: Missing 001 in out2 or wrong server"
	rc=1
fi
if ! grep -q "^:test2!.*@.* PRIVMSG test1 :private message via $servername$cr\$" out1; then
	echo "FAIL: Missing private message in out1"
	rc=1
fi
if ! grep -q "^:test1!.*@.* PRIVMSG #test :channel message via $servername$cr\$" out2; then
	echo "FAIL: Missing channel message in out2"
	rc=1
fi
if grep -q "chanops 1 via" out2; then
	echo "FAIL: Wrong chanops message in out2"
	rc=1
fi
if ! grep -q "^:test1!.* MODE #test +o test2[[:space:]]*$cr\$" out1; then
	echo "FAIL: Missing mode in out1"
	rc=1
fi
if ! grep -q "^:test1!.* MODE #test +o test2[[:space:]]*$cr\$" out2; then
	echo "FAIL: Missing mode in out2"
	rc=1
fi
if ! grep -q "^:test1!.* PRIVMSG @#test :chanops 2 via $servername$cr\$" out2; then
	echo "FAIL: Missing chanops message in out2"
	rc=1
fi
if [ "$rc" -ne 0 ] && [ -t 0 ] && [ -t 1 ] && [ -t 2 ]; then
	echo 'Starting shell for investigation...'
	PS1='ircd-smoketest$ ' sh -i
fi
if [ "$rc" -eq 0 ]; then
	echo 'PASS'
fi
exit $rc
