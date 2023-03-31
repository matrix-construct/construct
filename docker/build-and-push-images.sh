#!/bin/sh

# Sundry configuration

BASEDIR=$(dirname "$0")
stage=$1
mode=$2

export DOCKER_BUILDKIT=1
if test "$CI" = true; then
	export BUILDKIT_PROGRESS="plain"
	echo "plain"
fi

# The stage argument can be "base" "full" "built" "test" or "push"
default_stage="push"
stage=${stage:=$default_stage}

# The mode argument can be "real" or "test"
default_mode="real"
mode=${mode:=$default_mode}

if test "$ctor_test" = true; then
	mode="test"
fi

# Account configuration environment.
#
# Override these for yourself.

default_ctor_url="https://github.com/jevolk/charybdis"
default_ctor_id="jevolk/construct"

ctor_url=${ctor_url:=$default_ctor_url}
ctor_id=${ctor_id:=$default_ctor_id}
ctor_acct=${ctor_acct:=$(echo $ctor_id | cut -d"/" -f1)}
ctor_repo=${ctor_repo:=$(echo $ctor_id | cut -d"/" -f2)}

# Job matrix configuration environment
#
# All combinations of these arrays will be run by this script. Overriding
# these with one element for each variable allows this script to do one thing
# at a time.

default_ctor_features="base full"
default_ctor_distros="ubuntu-22.04 ubuntu-22.10 alpine-3.17"
default_ctor_machines="amd64 amd64-avx amd64-avx512"
default_ctor_toolchains="gcc-10 gcc-11 gcc-12 clang-14"

ctor_features=${ctor_features:=$default_ctor_features}
ctor_distros=${ctor_distros:=$default_ctor_distros}
ctor_machines=${ctor_machines:=$default_ctor_machines}
ctor_toolchains=${ctor_toolchains:=$default_ctor_toolchains}

###############################################################################

matrix()
{
	for ctor_feature in $ctor_features; do
		for ctor_distro in $ctor_distros; do
			for ctor_machine in $ctor_machines; do
				for ctor_toolchain in $ctor_toolchains; do
					build $ctor_feature $ctor_distro $ctor_machine $ctor_toolchain
					if test $? -ne 0; then return 1; fi
				done
			done
		done
	done
	return 0
}

build()
{
	feature=$1
	distro=$2
	machine=$3
	toolchain=$4
	dist_name=$(echo $distro | cut -d"-" -f1)
	dist_version=$(echo $distro | cut -d"-" -f2)
	runner_name=$(echo $RUNNER_NAME | cut -d"." -f1)
	runner_num=$(echo $RUNNER_NAME | cut -d"." -f2)

	args="$ctor_docker_build_args"
	args="$args --compress=true"

	if test ! -z "$runner_num"; then
		cpu_num=$(expr $runner_num % $(nproc))
		args="$args --cpuset-cpus=${cpu_num}"
	fi

	args="$args --build-arg acct=${ctor_acct}"
	args="$args --build-arg repo=${ctor_repo}"
	args="$args --build-arg ctor_url=${ctor_url}"
	args="$args --build-arg dist_name=${dist_name}"
	args="$args --build-arg dist_version=${dist_version}"
	args="$args --build-arg machine=${machine}"
	args="$args --build-arg feature=${feature}"

	args_dist $dist_name $dist_version
	if test $? -ne 0; then return 1; fi

	args_machine $machine
	if test $? -ne 0; then return 1; fi

	args_platform $machine
	if test $? -ne 0; then return 1; fi

	if test $toolchain != "false"; then
		args_toolchain $toolchain $dist_name $dist_version
		if test $? -ne 0; then return 1; fi
	fi

	if test $mode = "test"; then
		cmd=$(which echo)
	else
		cmd=$(which docker)
	fi

	# Intermediate stage build; usually cached from prior iteration.
	tag="$ctor_acct/$ctor_repo:${distro}-${feature}-${machine}"
	arg="$args -t $tag $BASEDIR/${dist_name}/${feature}"
	eval "$cmd build $arg"
	if test $? -ne 0; then return 1; fi
	if test $stage = $feature; then return 0; fi
	if test $toolchain = "false"; then return 0; fi

	# Leaf build; unique to each iteration.
	tag="$ctor_acct/$ctor_repo:${distro}-${feature}-built-${toolchain}-${machine}"
	arg="$args -t $tag $BASEDIR/${dist_name}/built"
	eval "$cmd build $arg"
	if test $? -ne 0; then return 1; fi
	if test $stage = "built"; then return 0; fi

	# Test built;
	arg="$args $BASEDIR/${dist_name}/test"
	eval "$cmd build $arg"
	if test $? -ne 0; then return 1; fi
	if test $stage = "test"; then return 0; fi

	# Push built
	eval "$cmd push $tag"
	if test $? -ne 0; then return 1; fi

	return 0
}

args_dist()
{
	case $1 in
	alpine)
		case $2 in
		3.16)
			args="$args --build-arg rocksdb_version=7.2.2"
			return 0
			;;
		3.17)
			args="$args --build-arg rocksdb_version=7.7.3"
			return 0
			;;
		esac
		;;
	ubuntu)
		case $2 in
		22.04)
			args="$args --build-arg rocksdb_version=7.4.3"
			args="$args --build-arg boost_version=1.74"
			args="$args --build-arg icu_version=70"
			return 0
			;;
		22.10)
			args="$args --build-arg rocksdb_version=7.10.2"
			args="$args --build-arg boost_version=1.74"
			args="$args --build-arg icu_version=71"
			return 0
			;;
		esac
		;;
	esac
	return 1
}

args_toolchain()
{
	_name=$(echo $1 | cut -d"-" -f1)
	_epoch=$(echo $1 | cut -d"-" -f2)

	case $2 in
	alpine)
		toolchain=$_name
		case $1 in
		gcc*)
			extra_dev="gcc"
			extra_dev="${extra_dev} g++"
			args="$args --build-arg extra_packages_dev=\"${extra_dev}\""
			args="$args --build-arg cc=gcc --build-arg cxx=g++"
			return 0
			;;
		clang*)
			extra_dev="clang"
			extra_dev="${extra_dev} llvm"
			extra_dev="${extra_dev} llvm-dev"
			args="$args --build-arg extra_packages_dev=\"${extra_dev}\""
			args="$args --build-arg cc=clang --build-arg cxx=clang++"
			test $3 != "3.16"
			return $?
			;;
		esac
		;;
	ubuntu)
		case $1 in
		gcc*)
			extra_dev="gcc-${_epoch}"
			extra_dev="${extra_dev} g++-${_epoch}"
			args="$args --build-arg extra_packages_dev=\"${extra_dev}\""
			args="$args --build-arg cc=gcc-${_epoch} --build-arg cxx=g++-${_epoch}"
			return 0
			;;
		clang*)
			extra_dev="clang-${_epoch}"
			extra_dev="${extra_dev} llvm-${_epoch}"
			extra_dev="${extra_dev} llvm-spirv-${_epoch}"
			extra_dev="${extra_dev} libclc-${_epoch}-dev"
			args="$args --build-arg extra_packages_dev=\"${extra_dev}\""
			args="$args --build-arg cc=clang-${_epoch} --build-arg cxx=clang++-${_epoch}"
			return 0
			;;
		esac
		;;
	esac
	return 1
}

args_machine()
{
	case $1 in
	amd64)
		args="$args --build-arg machine_spec=arch=amdfam10"
		;;
	amd64-avx)
		args="$args --build-arg machine_spec=arch=sandybridge"
		args="$args --build-arg rocksdb_avx=1"
		;;
	amd64-avx2)
		args="$args --build-arg machine_spec=arch=haswell"
		args="$args --build-arg rocksdb_avx2=1"
		;;
	amd64-avx512)
		args="$args --build-arg machine_spec=arch=skylake-avx512"
		args="$args --build-arg rocksdb_avx2=1"
		;;
	esac
	return 0
}

args_platform()
{
	case $1 in
	amd64*)
		args="$args --platform linux/amd64"
		test $(uname -m) = "x86_64"
		return $?
		;;
	arm64*)
		args="$args --platform linux/arm64"
		test $(uname -m) = "aarch64"
		return $?
		;;
	esac
	return 1
}

matrix
