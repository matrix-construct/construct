#!/bin/bash

BRANCH=$1
if [ -z $BRANCH ]; then
	BRANCH="master"
fi

CONFIG_OPTIONS=$2
if [ -z "$CONFIG_OPTIONS" ]; then
	CONFIG_OPTIONS="--enable-optimize"
fi

run ()
{
	COMMAND=$1
	# check for empty commands
	if test -z "$COMMAND" ; then
		echo -e "\033[1;5;31mERROR\033[0m No command specified!"
		return 1
	fi

	shift;
	OPTIONS="$@"
	# print a message
	if test -n "$OPTIONS" ; then
		echo -ne "\033[1m$COMMAND $OPTIONS\033[0m ... "
	else
		echo -ne "\033[1m$COMMAND\033[0m ... "
	fi

	# run or die
	$COMMAND $OPTIONS ; RESULT=$?
	if test $RESULT -ne 0 ; then
		echo -e "\033[1;5;31mERROR\033[0m $COMMAND failed. (exit code = $RESULT)"
		exit 1
	fi

	echo -e "\033[0;32myes\033[0m"
	return 0
}


echo "*** Building SpiderMonkey... "

USERDIR=$PWD            # Save current dir and return to it later

run git submodule update --init --remote deps/gecko-dev
run cd deps/gecko-dev
#run git fetch --depth=1 origin $BRANCH
run git checkout $BRANCH

run cd js/src
run autoconf2.13
mkdir build_OPT.OBJ
ALREADY_EXISTS=$?
run cd build_OPT.OBJ

# If configure is rerun make will rebuild the entire engine every time
# this script is run, which is every time charybdis gets ./configure. This
# may actually be the best behavior in production but right now this test
# prevents rebuilds
if test $ALREADY_EXISTS -eq 0; then
	run ../configure $CONFIG_OPTIONS JS_STANDALONE=1
fi

#run ../configure --enable-debug
# run ../configure --disable-shared-js --enable-debug # --enable-replace-malloc

NJOBS=`nproc`
run make -j$NJOBS

run cd $USERDIR         # Return to user's original directory
