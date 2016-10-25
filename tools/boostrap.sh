#!/bin/bash

BTOOLSET=$1              # The platform toolchain name (gcc, clang, mingw, ...)
BLIBS=$2                 # A comma-separated list of which boost libs to build
BVARIANT=$3              # release optimization or debug symbols etc
BLINK=$4                 # whether to build with shared or static linkage (we like shared)
BTHREADING=$5            # whether to build with thread-safety (we benefit from SINGLE THREADED)
TOPDIR=$6                # This should be an absolute path to the repo root


if [ -z $TOPDIR ]; then
	TOPDIR=$PWD
fi


if [ -z $BLIBS ]; then
	BLIBS="system"
fi


case "$BTOOLSET" in
g\+\+*|gcc*)
	BSFLAGS="--with-toolset=gcc";;

clang\+\+*|clang*)
	BSFLAGS="--with-toolset=clang";;

mingw\+\+*|mingw*)
	BSFLAGS="";;

*)
	BTOOLSET="";;
esac


if [ -z $BVARIANT ]; then
	BVARIANT="release"
fi


if [ -z $BLINK ]; then
	BLINK="shared"
fi


if [ -z $BTHREADING ]; then
	BTHREADING="single"
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


echo "*** Synchronizing boost... "

# Save current dir and return to it later
USERDIR=$PWD

### Populate the boost submodule directory.
run cd $TOPDIR
run git submodule update --init boost
run cd boost

### Build tools
run git submodule update --init --recursive tools/build
run git submodule update --init --recursive tools/inspect

### These are the libraries we need. Most of them are header-only. If not header-only,
### add to the list --with-libraries in the ./bootstrap command below
run git submodule update --init --recursive libs/predef
run git submodule update --init --recursive libs/assert
run git submodule update --init --recursive libs/static_assert
run git submodule update --init --recursive libs/type_traits
run git submodule update --init --recursive libs/config
run git submodule update --init --recursive libs/core
run git submodule update --init --recursive libs/detail

run git submodule update --init --recursive libs/asio
run git submodule update --init --recursive libs/system
run git submodule update --init --recursive libs/regex

run git submodule update --init --recursive libs/lexical_cast
run git submodule update --init --recursive libs/range
run git submodule update --init --recursive libs/concept_check
run git submodule update --init --recursive libs/utility
run git submodule update --init --recursive libs/throw_exception
run git submodule update --init --recursive libs/numeric
run git submodule update --init --recursive libs/integer
run git submodule update --init --recursive libs/array
run git submodule update --init --recursive libs/functional
run git submodule update --init --recursive libs/container
run git submodule update --init --recursive libs/move
run git submodule update --init --recursive libs/math

run git submodule update --init --recursive libs/tokenizer
run git submodule update --init --recursive libs/iterator
run git submodule update --init --recursive libs/mpl
run git submodule update --init --recursive libs/preprocessor
run git submodule update --init --recursive libs/date_time
run git submodule update --init --recursive libs/smart_ptr
run git submodule update --init --recursive libs/bind

run git submodule update --init --recursive libs/filesystem
run git submodule update --init --recursive libs/io

run git submodule update --init --recursive libs/dll
run git submodule update --init --recursive libs/align
run git submodule update --init --recursive libs/winapi

run git submodule update --init --recursive libs/spirit
run git submodule update --init --recursive libs/phoenix
run git submodule update --init --recursive libs/proto
run git submodule update --init --recursive libs/fusion
run git submodule update --init --recursive libs/typeof
run git submodule update --init --recursive libs/variant
run git submodule update --init --recursive libs/type_index
run git submodule update --init --recursive libs/foreach
run git submodule update --init --recursive libs/optional
run git submodule update --init --recursive libs/function
run git submodule update --init --recursive libs/function_types
run git submodule update --init --recursive libs/iostreams

run git submodule update --init --recursive libs/coroutine
#run git submodule update --init --recursive libs/coroutine2
## ASIO does not need coroutine2 at this time, but there is
## some issue with segmented stack support requiring inclusion
## of libs/context...
run git submodule update --init --recursive libs/context
run git submodule update --init --recursive libs/thread
run git submodule update --init --recursive libs/chrono
run git submodule update --init --recursive libs/atomic
run git submodule update --init --recursive libs/ratio
run git submodule update --init --recursive libs/intrusive
run git submodule update --init --recursive libs/tuple
run git submodule update --init --recursive libs/exception
run git submodule update --init --recursive libs/algorithm

run git submodule update --init --recursive libs/locale

### Install should go right into this local submodule repository
run ./bootstrap.sh --prefix=$PWD --libdir=$PWD/lib --with-libraries=$BLIBS $BSFLAGS
run ./b2 -d0 headers
run ./b2 -d0 install threading=$BTHREADING variant=$BVARIANT link=$BLINK address-model=64 warnings=all

### TODO: this shouldn't be necessary.
### XXX: required when boost submodules are fetched and built la carte, but not required
### XXX: when all submodules are fetched and built. we're missing a step.
for lib in `ls -d libs/*/include`; do
	run cp -r ${lib}/* include/
done
run cp -r libs/numeric/conversion/include/* include/

# Return to user's original directory
run cd $USERDIR
