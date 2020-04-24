{ pkgs ? import <nixpkgs> { } }:

let
  package = pkgs.callPackage ./. {};

  configureFlags = [
    "--enable-generic"
    "--with-custom-branding=nix"
    "--with-custom-version=dev"
    "--with-boost-libdir=${pkgs.boost.out}/lib"
    "--with-boost=${pkgs.boost.dev}"
    "--with-magic-file=${pkgs.file}/share/misc/magic.mgc"
    "--enable-jemalloc"
    "--with-imagemagick-includes=${pkgs.graphicsmagick}/include/GraphicsMagick"
    "--with-log-level=DEBUG"
  ];

  configure = pkgs.writeShellScriptBin "configure" ''
    exec ./configure ${pkgs.lib.concatStringsSep " " configureFlags}
  '';
in pkgs.mkShell {
  buildInputs = with pkgs; [
    libsodium openssl file boost gmp llvm
    (rocksdb.overrideAttrs (super: rec {
      version = "5.16.6";
      src = pkgs.fetchFromGitHub {
        owner = "facebook";
        repo = "rocksdb";
        rev = "v${version}";
        sha256 = "0yy09myzbi99qdmh2c2mxlddr12pwxzh66ym1y6raaqglrsmax66";
      };
      NIX_CFLAGS_COMPILE = "${super.NIX_CFLAGS_COMPILE} -Wno-error=redundant-move";
    }))
    zlib lz4 snappy
    graphicsmagick
    jemalloc
  ];
  nativeBuildInputs = with pkgs; [
    git autoconf automake libtool gcc clang cmake pkg-config doxygen graphviz
    configure
  ];

  shellHook = ''
    WORKDIR=$(mktemp -d)
    export RUNTIME_DIRECTORY=$WORKDIR/runtime
    export STATE_DIRECTORY=$WORKDIR/state
    export CACHE_DIRECTORY=$WORKDIR/cache
    export LOGS_DIRECTORY=$WORKDIR/logs
    export CONFIGURATION_DIRECTORY=$WORKDIR/configuration

    mkdir -p $RUNTIME_DIRECTORY $STATE_DIRECTORY $LOGS_DIRECTORY
    mkdir -p $CACHE_DIRECTORY $CONFIGURATION_DIRECTORY
  '';
}
