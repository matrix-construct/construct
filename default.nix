{ rev    ? "c7e0e9ed5abd0043e50ee371129fcb8640264fc4"
, sha256 ? "0c28mpvjhjc8kiwj2w8zcjsr2rayw989a1wnsqda71zpcyas3mq2"
, pkgs   ? import (builtins.fetchTarball { inherit sha256;
    url = "https://github.com/NixOS/nixpkgs/archive/${rev}.tar.gz";
  }) { }

, stdenv ? if useClang
           then (if pkgs.stdenv.cc.isClang
                 then pkgs.stdenv
                 else pkgs.llvmPackages_latest.stdenv)
           else (if pkgs.stdenv.cc.isGNU
                 then pkgs.stdenv
                 else pkgs.gcc.stdenv)
, lib ? pkgs.lib

, debug              ? false # Debug Build
, useClang           ? false # Use Clang over GCC
, useJemalloc        ? true # Use the Dynamic Memory Allocator
, withGraphicsMagick ? true # Allow Media Thumbnails
}:

stdenv.mkDerivation rec {
  pname = "matrix-construct";
  version = "development";

  src = lib.cleanSource ./.;

  preAutoreconf = let
    VERSION_COMMIT_CMD = "git rev-parse --short HEAD";
    VERSION_BRANCH_CMD = "git rev-parse --abbrev-ref HEAD";
    VERSION_TAG_CMD = "git describe --tags --abbrev=0 --dirty --always";
    VERSION_CMD = "git describe --tags --always";
    runWithGit = id: cmd: lib.removeSuffix "\n" (builtins.readFile (pkgs.runCommandNoCCLocal "construct-${id}" {
      buildInputs = [ pkgs.git ];
    } "cd ${./.} && ${cmd} > $out"));
  in ''
    substituteInPlace configure.ac --replace "${VERSION_COMMIT_CMD}" "echo ${runWithGit "version-commit" VERSION_COMMIT_CMD}"
    substituteInPlace configure.ac --replace "${VERSION_BRANCH_CMD}" "echo ${runWithGit "version-branch" VERSION_BRANCH_CMD}"
    substituteInPlace configure.ac --replace "${VERSION_TAG_CMD}" "echo ${runWithGit "version-tag" VERSION_TAG_CMD}"
    substituteInPlace configure.ac --replace "${VERSION_CMD}" "echo ${runWithGit "version" VERSION_CMD}"
  '';

  configureFlags = [
    "--enable-generic"
    "--with-custom-branding=nix"
    "--with-custom-version=dev"
    "--with-boost-libdir=${pkgs.boost.out}/lib"
    "--with-boost=${pkgs.boost.dev}"
    "--with-magic-file=${pkgs.file}/share/misc/magic.mgc"
    "--with-rocksdb-includes=${pkgs.rocksdb.src}"
    "--with-rocksdb-libs=${pkgs.rocksdb.out}"
  ] ++ lib.optional useJemalloc "--enable-jemalloc"
    ++ lib.optional withGraphicsMagick [
    "--with-imagemagick-includes=${pkgs.graphicsmagick}/include/GraphicsMagick"
  ] ++ lib.optional debug "--with-log-level=DEBUG";

  preBuild = ''
    make clean
  '';

  enableParallelBuilding = true;

  nativeBuildInputs = with pkgs; [
    autoreconfHook pkg-config
  ] ++ lib.optional useClang llvmPackages_latest.llvm
    ++ lib.optional useJemalloc jemalloc;
  buildInputs = with pkgs; [
    libsodium openssl file boost gmp rocksdb
  ] ++ lib.optional withGraphicsMagick graphicsmagick;
}
