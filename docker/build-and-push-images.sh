#!/bin/sh

BASEDIR=$(dirname "$0")
ACCT=jevolk
REPO=construct
CTOR_URL="https://github.com/matrix-construct/construct"

export DOCKER_BUILDKIT=1
#export BUILDKIT_PROGRESS=plain

features="base full"
distros="ubuntu-22.04 ubuntu-22.10 alpine-3.16 alpine-3.17"
machines="arm64 amd64 amd64-avx amd64-avx2 amd64-avx512"
toolchains="gcc-10 gcc-11 gcc-12 clang-14 clang-15"
stages="built test"

matrix()
{
	for feature_ in $features; do
		for distro_ in $distros; do
			for machine_ in $machines; do
				for toolchain_ in $toolchains; do
					for stage_ in $stages; do
						build $feature_ $distro_ $machine_ $toolchain_ $stage_
					done
				done
			done
		done
	done
}

build()
{
	feature=$1
	distro=$2
	machine=$3
	toolchain=$4
	stage=$5

	dist_name=$(echo $distro | cut -d"-" -f1)
	dist_version=$(echo $distro | cut -d"-" -f2)

	args=""
	args="$args --compress=true"
	args="$args --build-arg acct=$ACCT"
	args="$args --build-arg repo=$REPO"
	args="$args --build-arg ctor_url=$CTOR_URL"
	args="$args --build-arg dist_name=${dist_name}"
	args="$args --build-arg dist_version=${dist_version}"
	args="$args --build-arg feature=${feature}"
	args="$args --build-arg machine=${machine}"

	args_dist $dist_name $dist_version
	if test $? -ne 0; then return 1; fi

	args_toolchain $toolchain $dist_name $dist_version
	if test $? -ne 0; then return 1; fi

	args_machine $machine
	if test $? -ne 0; then return 1; fi

	args_platform $machine
	if test $? -ne 0; then return 1; fi

	# Intermediate stage build; usually cached from prior iteration.
	tag="$ACCT/$REPO:${distro}-${feature}-${machine}"
	cmd="$args -t $tag $BASEDIR/${dist_name}/${feature}"
	docker build $cmd

	# Leaf stage build; unique to each iteration.
	tag="$ACCT/$REPO:${distro}-${feature}-${stage}-${toolchain}-${machine}"
	cmd="$args -t $tag $BASEDIR/${dist_name}/${stage}"
	docker build $cmd
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
	_version=$(echo $1 | cut -d"-" -f2)

	case $2 in
	alpine)
		toolchain=$_name
		case $1 in
		gcc*)
			args="$args --build-arg extra_packages_dev=gcc"
			args="$args --build-arg extra_packages_dev1=g++"
			args="$args --build-arg cc=gcc --build-arg cxx=g++"
			return 0
			;;
		clang*)
			args="$args --build-arg extra_packages_dev=clang"
			args="$args --build-arg extra_packages_dev1=llvm"
			args="$args --build-arg extra_packages_dev2=llvm-dev"
			args="$args --build-arg cc=clang --build-arg cxx=clang++"
			test $3 != "3.16"
			return $?
			;;
		esac
		;;
	ubuntu)
		case $1 in
		gcc*)
			args="$args --build-arg extra_packages_dev=gcc-${_version}"
			args="$args --build-arg extra_packages_dev1=g++-${_version}"
			args="$args --build-arg cc=gcc-${_version} --build-arg cxx=g++-${_version}"
			return 0
			;;
		clang*)
			args="$args --build-arg extra_packages_dev=clang-${_version}"
			args="$args --build-arg extra_packages_dev1=llvm-${_version}-dev"
			args="$args --build-arg extra_packages_dev2=llvm-spirv-${_version}"
			args="$args --build-arg cc=clang-${_version} --build-arg cxx=clang++-${_version}"
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
