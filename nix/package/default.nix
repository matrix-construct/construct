{ source, rev, pkgs, lib, stdenv ? if useClang
                                   then (if pkgs.stdenv.cc.isClang
                                         then pkgs.stdenv
                                         else pkgs.llvmPackages_latest.stdenv)
                                   else (if pkgs.stdenv.cc.isGNU
                                         then pkgs.stdenv
                                         else pkgs.gcc.stdenv)

, debug              ? false # Debug Build
, useClang           ? false # Use Clang over GCC
, useJemalloc        ? false # Use the Dynamic Memory Allocator
, withGraphicsMagick ? true  # Allow Media Thumbnails
}:

let
  pname = "matrix-construct";
  version = lib.substring 0 9 rev;

  buildArgs = buildInputs: nativeBuildInputs: {
    inherit buildInputs nativeBuildInputs;
    preferLocalBuild = true;
    allowSubstitutes = false;
  };
in stdenv.mkDerivation rec {
  inherit pname version;
  src = source;

  buildPhase = with pkgs; ''
  '';

  CXXOPTS = "-pipe -mtune=generic -O3 -fgcse-sm -fgcse-las -fsched-stalled-insns=0 -fsched-pressure -fsched-spec-load -fira-hoist-pressure -fbranch-target-load-optimize -frerun-loop-opt -fdevirtualize-at-ltrans -fipa-pta -fmodulo-sched -fmodulo-sched-allow-regmoves -ftracer -ftree-loop-im -ftree-switch-conversion -g -ggdb -frecord-gcc-switches -fstack-protector-explicit -fvtable-verify=none -fvisibility-inlines-hidden -fnothrow-opt -fno-threadsafe-statics -fverbose-asm -fsigned-char";
  WARNOPTS = "-Wall -Wextra -Wpointer-arith -Wcast-align -Wcast-qual -Wfloat-equal -Wwrite-strings -Wparentheses -Wundef -Wpacked -Wformat -Wformat-y2k -Wformat-nonliteral -Wstrict-aliasing=2 -Wstrict-overflow=5 -Wdisabled-optimization -Winvalid-pch -Winit-self -Wuninitialized -Wunreachable-code -Wno-overloaded-virtual -Wnon-virtual-dtor -Wnoexcept -Wsized-deallocation -Wctor-dtor-privacy -Wsign-promo -Wtrampolines -Wduplicated-cond -Wrestrict -Wnull-dereference -Wplacement-new=2 -Wundef -Wodr -Werror=return-type -Wno-missing-field-initializers -Wno-unused -Wno-unused-function -Wno-unused-label -Wno-unused-value -Wno-unused-variable -Wno-unused-parameter -Wno-endif-labels -Wmissing-noreturn -Wno-unknown-attributes -Wno-unknown-pragmas -Wlogical-op -Wformat-security -Wstack-usage=16384 -Wframe-larger-than=8192 -Walloca";

  nativeBuildInputs = with pkgs; [
    libtool makeWrapper
  ] ++ lib.optional useClang llvmPackages_latest.llvm;
  buildInputs = with pkgs; [
    libsodium openssl file boost gmp zlib jemalloc rocksdb
  ] ++ lib.optional withGraphicsMagick graphicsmagick;

  includes = stdenv.mkDerivation rec {
    name = "${pname}-includes";
    src = "${source}/include/";

    configHeader = import ./config.nix {
      inherit (pkgs) writeText file;
      inherit stdenv;
      IRCD_ALLOCATOR_USE_DEFAULT = !useJemalloc;
      IRCD_ALLOCATOR_USE_JEMALLOC = useJemalloc;
    };
    buildInputs = with pkgs; [
      boost openssl
    ];

    buildPhase = with pkgs; ''
      mkdir -p $out
      cp -Lr --no-preserve=all ${source}/include/ircd $out/
      cd $out/ircd
      substituteAll ${configHeader} config.h
      $CXX -std=gnu++17 ${CXXOPTS} -x c++-header -pthread -fpch-deps -o ircd.h.gch -DHAVE_CONFIG_H -DPCH -DNDEBUG ${WARNOPTS} -I$out ircd.h
      $CXX -std=gnu++17 ${CXXOPTS} -x c++-header -pthread -fpch-deps -fPIC -o asio.h.gch -DHAVE_CONFIG_H -DPCH -DNDEBUG ${WARNOPTS} -I$out -I${boost.dev}/include -DPIC asio.h
      $CXX -std=gnu++17 ${CXXOPTS} -x c++-header -pthread -fpch-deps -o matrix.h.gch -DHAVE_CONFIG_H -DPCH -DNDEBUG ${WARNOPTS} -I$out matrix.h
      cp matrix.h matrix.pic.h
      $CXX -std=gnu++17 ${CXXOPTS} -x c++-header -pthread -fpch-deps -fPIC -o matrix.pic.h.gch -DHAVE_CONFIG_H -DPCH -DNDEBUG ${WARNOPTS} -I$out -DPIC matrix.pic.h
      $CXX -std=gnu++17 ${CXXOPTS} -x c++-header -pthread -fpch-deps -I${boost.dev}/include -fPIC -o spirit.h.gch -DHAVE_CONFIG_H -DPCH -DNDEBUG ${WARNOPTS} -I$out -DPIC spirit.h
      cp ircd.h ircd.pic.h
      $CXX -std=gnu++17 ${CXXOPTS} -x c++-header -pthread -fpch-deps -fPIC -o ircd.pic.h.gch -DHAVE_CONFIG_H -DPCH -DNDEBUG ${WARNOPTS} -I$out -DPIC ircd.pic.h
    '';

    installPhase = "true";
    dontStrip = true;
  };

  installPhase = let
    ircdUnitCXX = ccFile: loFile: extraArgs: "${pkgs.runCommandCC (lib.strings.sanitizeDerivationName loFile) (buildArgs buildInputs nativeBuildInputs) ''
      libtool --tag=CXX --mode=compile $CXX -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT                    ${extraArgs} \
        -I${includes} -include ircd/ircd.pic.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} \
        -c -o $out/${loFile} ${source}/ircd/${ccFile}
    ''}/${loFile}";
    ircdUnitCC  = asFile: loFile: extraArgs: "${pkgs.runCommandCC (lib.strings.sanitizeDerivationName loFile) (buildArgs buildInputs nativeBuildInputs) ''
      libtool --tag=CC  --mode=compile $CC               -DHAVE_CONFIG_H -DIRCD_UNIT                    ${extraArgs} \
        -I${includes}                          -DPCH -DNDEBUG ${WARNOPTS}                                              \
        -c -o $out/${loFile} ${source}/ircd/${asFile}
    ''}/${loFile}";
    ircdLD = loFiles: laFile: extraArgs: "${pkgs.runCommandCC (lib.strings.sanitizeDerivationName laFile) (buildArgs buildInputs nativeBuildInputs) ''
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=initial-exec -pthread ${CXXOPTS} -version-info 3:2:0 \
        -Wl,--no-undefined-version -Wl,--weak-unresolved-symbols -Wl,--unresolved-symbols=ignore-in-shared-libs \
        -Wl,--wrap=pthread_create -Wl,--wrap=pthread_join -Wl,--wrap=pthread_timedjoin_np -Wl,--wrap=pthread_self -Wl,--wrap=pthread_setname_np \
        -Wl,-z,nodelete -Wl,-z,nodlopen -Wl,-z,lazy -L${pkgs.boost.out}/lib \
        -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment \
        -o $out/${laFile} ${lib.concatStringsSep " " loFiles} ${extraArgs} \
        -lrocksdb -lboost_coroutine -lboost_context -lboost_thread -lboost_filesystem -lboost_chrono -lboost_system -lssl -lcrypto -L${pkgs.libsodium.out}/lib -lsodium -lmagic -lz -lpthread -latomic -lrocksdb -ldl
    ''}/${laFile}";
    matrixUnitCXX = ccFile: loFile: extraArgs: "${pkgs.runCommandCC (lib.strings.sanitizeDerivationName loFile) (buildArgs buildInputs nativeBuildInputs) ''
      libtool --tag=CXX --mode=compile $CXX -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_MATRIX_UNIT ${extraArgs} \
        -I${includes} -include ircd/matrix.pic.h -include ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=local-dynamic -pthread ${CXXOPTS} \
        -c -o $out/${loFile} ${source}/matrix/${ccFile}
    ''}/${loFile}";
    matrixLD = loFiles: laFile: extraArgs: "${pkgs.runCommandCC (lib.strings.sanitizeDerivationName laFile) (buildArgs buildInputs nativeBuildInputs) ''
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -pthread -ftls-model=local-dynamic ${CXXOPTS} -version-info 0:1:0 \
        -Wl,--no-undefined-version -Wl,--allow-shlib-undefined -Wl,--unresolved-symbols=ignore-in-shared-libs -Wl,-z,lazy -L${dirOf ircd}/ \
        -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment \
        -o $out/${laFile} ${lib.concatStringsSep " " loFiles} ${extraArgs} -lrocksdb -ldl ${if useJemalloc then "-ljemalloc" else ""}
    ''}/${laFile}";
    moduleUnitCXX = subdir: ccFile: loFile: extraArgs: "${pkgs.runCommandCC (lib.strings.sanitizeDerivationName loFile) (buildArgs buildInputs nativeBuildInputs) ''
      libtool --tag=CXX --mode=compile $CXX -std=gnu++17 -DHAVE_CONFIG_H -DIRCD_UNIT -DIRCD_UNIT_MODULE ${extraArgs} \
        -I${includes} -include ${includes}/ircd/matrix.pic.h -include ${includes}/ircd/mods/mapi.h -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=global-dynamic -pthread ${CXXOPTS} \
        -I ${source}/modules/${lib.concatStringsSep "/" subdir} -c -o $out/${loFile} ${source}/modules/${lib.concatStringsSep "/" subdir}/${ccFile}
    ''}/${loFile}";
    moduleLD = loFiles: laFile: extraArgs: "${pkgs.runCommandCC (lib.strings.sanitizeDerivationName laFile) (buildArgs buildInputs nativeBuildInputs) ''
      libtool --tag=CXX --mode=link    $CXX -std=gnu++17 -ftls-model=global-dynamic -pthread ${CXXOPTS} -module -avoid-version \
        -Wl,--allow-shlib-undefined -Wl,-z,lazy -L${dirOf ircd}/ -L${dirOf matrix}/ \
        -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment \
        -o $out/${laFile} ${lib.concatStringsSep " " loFiles} -lrocksdb -ldl ${extraArgs}
    ''}/${laFile}";
    constructUnitCXX = ccFile: obFile: extraArgs: "${pkgs.runCommandCC (lib.strings.sanitizeDerivationName obFile) (buildArgs buildInputs nativeBuildInputs) ''
      mkdir -p $out
      $CXX -std=gnu++17 -DHAVE_CONFIG_H -I${includes} -I${pkgs.boost.dev}/include -DPCH -DNDEBUG ${WARNOPTS} -ftls-model=initial-exec -pthread ${CXXOPTS} \
        -c -o $out/${obFile} ${source}/construct/${ccFile} ${extraArgs}
    ''}/${obFile}";
    constructLD = obFiles: exFile: "${pkgs.runCommandCC (lib.strings.sanitizeDerivationName exFile) (buildArgs buildInputs nativeBuildInputs) ''
      libtool --tag=CXX --mode=link g++ -std=gnu++17 -ftls-model=initial-exec -pthread ${CXXOPTS} -dlopen self \
        -Wl,--warn-execstack -Wl,--warn-common -Wl,--detect-odr-violations -Wl,--unresolved-symbols=report-all -Wl,--allow-shlib-undefined -Wl,--dynamic-list-data -Wl,--dynamic-list-cpp-new -Wl,--dynamic-list-cpp-typeinfo -Wl,--rosegment -Wl,-z,noexecstack \
        -L${dirOf ircd}/ ${lib.concatMapStringsSep " " (mod: "-L${dirOf mod}/") modules} -L${pkgs.boost.out}/lib \
        -Wl,-fuse-ld=gold -Wl,--gdb-index -Wl,--warn-common -Wl,--warn-execstack -Wl,--detect-odr-violations -Wl,--rosegment -Wl,-z,noexecstack -Wl,-z,combreloc -Wl,-z,text-unlikely-segment \
        -o $out/bin/${exFile} ${lib.concatStringsSep " " obFiles} -lircd -lboost_coroutine -lboost_context -lboost_thread -lboost_filesystem -lboost_chrono -lboost_system -lssl -lcrypto -lpthread -latomic -lrocksdb -ldl
    ''}/bin/${exFile}";

    versionDefs = let
      versions = {
        BRANDING_VERSION = lib.substring 0 9 rev;
        RB_VERSION = lib.substring 0 9 rev;
        RB_VERSION_BRANCH = "master";
        RB_VERSION_COMMIT = rev;
        RB_VERSION_TAG = rev;
      };
    in lib.concatStringsSep " " (lib.mapAttrsToList (k: v: "-U${k} -D'${k}=\"${v}\"'") versions);

    ircd = with pkgs; ircdLD [
      "${ircdUnitCXX "assert.cc"        "assert.lo"        ""}"
      "${ircdUnitCXX "info.cc"          "info.lo"          "${versionDefs}"}"
      "${ircdUnitCXX "allocator.cc"     "allocator.lo"     ""}"
      "${ircdUnitCXX "allocator_gnu.cc" "allocator_gnu.lo" ""}"
      "${ircdUnitCXX "allocator_je.cc"  "allocator_je.lo"  ""}"
      "${ircdUnitCXX "vg.cc"            "vg.lo"            ""}"
      "${ircdUnitCXX "exception.cc"        "exception.lo"        "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "util.cc"          "util.lo"          ""}"
      "${ircdUnitCXX "demangle.cc"      "demangle.lo"      ""}"
      "${ircdUnitCXX "backtrace.cc"     "backtrace.lo"     ""}"
      "${ircdUnitCXX "locale.cc"   "locale.lo"   "-I${boost.dev}/include"}"
      "${ircdUnitCXX "timedate.cc"      "timedate.lo"      ""}"
      "${ircdUnitCXX "lex_cast.cc" "lex_cast.lo" "-I${boost.dev}/include"}"
      "${ircdUnitCXX "stringops.cc"     "stringops.lo"     ""}"
      "${ircdUnitCXX "globular.cc"      "globular.lo"      ""}"
      "${ircdUnitCXX "tokens.cc"   "tokens.lo"   "-I${boost.dev}/include"}"
      "${ircdUnitCXX "parse.cc"   "parse.lo"   "-I${boost.dev}/include -include ircd/spirit.h -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly"}"
      "${ircdUnitCXX "rand.cc"          "rand.lo"          ""}"
      "${ircdUnitCXX "base.cc"     "base.lo"     "-I${boost.dev}/include"}"
      "${ircdUnitCXX "crh.cc"           "crh.lo"           ""}"
      "${ircdUnitCXX "fmt.cc"     "fmt.lo"     "-I${boost.dev}/include -include ircd/spirit.h -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly"}"
      "${ircdUnitCXX "json.cc"    "json.lo"    "-I${boost.dev}/include -include ircd/spirit.h -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly"}"
      "${ircdUnitCXX "cbor.cc"          "cbor.lo"          ""}"
      "${ircdUnitCXX "conf.cc"          "conf.lo"          ""}"
      "${ircdUnitCXX "stats.cc"         "stats.lo"         ""}"
      "${ircdUnitCXX "logger.cc"        "logger.lo"        ""}"
      "${ircdUnitCXX "run.cc"           "run.lo"           ""}"
      "${ircdUnitCXX "magic.cc"         "magic.lo"         ""}"
      "${ircdUnitCXX "sodium.cc"        "sodium.lo"        ""}"
       # pbc.cc would go here
      "${ircdUnitCXX "openssl.cc"       "openssl.lo"       ""}"
      "${ircdUnitCXX "rfc1459.cc" "rfc1459.lo" "-I${boost.dev}/include -include ircd/spirit.h -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly"}"
      "${ircdUnitCXX "rfc3986.cc" "rfc3986.lo" "-I${boost.dev}/include -include ircd/spirit.h -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly"}"
      "${ircdUnitCXX "rfc1035.cc"       "rfc1035.lo"       ""}"
      "${ircdUnitCXX "http.cc"    "http.lo"    "-I${boost.dev}/include -include ircd/spirit.h -fno-var-tracking -fno-var-tracking-assignments -femit-struct-debug-baseonly"}"
      "${ircdUnitCXX "http2.cc"         "http2.lo"         ""}"
      "${ircdUnitCXX "prof.cc"     "prof.lo"     "-I${boost.dev}/include"}"
      "${ircdUnitCXX "prof_linux.cc"    "prof_linux.lo"    ""}"
      "${ircdUnitCXX "fs.cc"               "fs.lo"               "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "fs_path.cc"          "fs_path.lo"          "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "ios.cc"              "ios.lo"              "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCC  "ctx_x86_64.S" "ctx_x86_64.lo " "-pipe"}"
      "${ircdUnitCXX "ctx.cc"              "ctx.lo"              "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "ctx_eh.cc"           "ctx_eh.lo"           "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "ctx_ole.cc"          "ctx_ole.lo"          "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "ctx_posix.cc"        "ctx_posix.lo"        "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "fs_aio.cc"           "fs_aio.lo"           "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "fs_iou.cc"           "fs_iou.lo"           "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "mods.cc"             "mods.lo"             "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "mods_ldso.cc"     "mods_ldso.lo"     ""}"
      "${ircdUnitCXX "db_port.cc"       "db_port.lo"       ""}"
      "${ircdUnitCXX "db_fixes.cc"        "db_fixes.lo"        "-I${rocksdb.src} -I${rocksdb.out}/include"}"
      "${ircdUnitCXX "db_env.cc"        "db_env.lo"        ""}"
      "${ircdUnitCXX "db.cc"            "db.lo"            ""}"
      "${ircdUnitCXX "net.cc"              "net.lo"              "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "net_addrs.cc"        "net_addrs.lo"        "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "net_dns.cc"          "net_dns.lo"          "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "net_dns_netdb.cc" "net_dns_netdb.lo" ""}"
      "${ircdUnitCXX "net_dns_cache.cc" "net_dns_cache.lo" ""}"
      "${ircdUnitCXX "net_dns_resolver.cc" "net_dns_resolver.lo" "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "net_listener.cc"     "net_listener.lo"     "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "net_listener_udp.cc" "net_listener_udp.lo" "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "server.cc"           "server.lo"           "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "client.cc"           "client.lo"           "-I${boost.dev}/include -include ircd/asio.h"}"
      "${ircdUnitCXX "resource.cc"      "resource.lo"      ""}"
      "${ircdUnitCXX "ircd.cc"          "ircd.lo"          "${versionDefs}"}"
    ] "libircd.la" "-rpath $out/lib";

    matrix = with pkgs; matrixLD [
      "${matrixUnitCXX "name.cc"                   "name.lo"                   ""}"
      "${matrixUnitCXX "id.cc" "id.lo" "-I${boost.dev}/include -include ircd/spirit.h"}"
      "${matrixUnitCXX "dbs.cc"                    "dbs.lo"                    ""}"
      "${matrixUnitCXX "dbs_event_idx.cc"          "dbs_event_idx.lo"          ""}"
      "${matrixUnitCXX "dbs_event_json.cc"         "dbs_event_json.lo"         ""}"
      "${matrixUnitCXX "dbs_event_column.cc"       "dbs_event_column.lo"       ""}"
      "${matrixUnitCXX "dbs_event_refs.cc"         "dbs_event_refs.lo"         ""}"
      "${matrixUnitCXX "dbs_event_horizon.cc"      "dbs_event_horizon.lo"      ""}"
      "${matrixUnitCXX "dbs_event_sender.cc"       "dbs_event_sender.lo"       ""}"
      "${matrixUnitCXX "dbs_event_type.cc"         "dbs_event_type.lo"         ""}"
      "${matrixUnitCXX "dbs_event_state.cc"        "dbs_event_state.lo"        ""}"
      "${matrixUnitCXX "dbs_room_events.cc"        "dbs_room_events.lo"        ""}"
      "${matrixUnitCXX "dbs_room_type.cc"          "dbs_room_type.lo"          ""}"
      "${matrixUnitCXX "dbs_room_state.cc"         "dbs_room_state.lo"         ""}"
      "${matrixUnitCXX "dbs_room_state_space.cc"   "dbs_room_state_space.lo"   ""}"
      "${matrixUnitCXX "dbs_room_joined.cc"        "dbs_room_joined.lo"        ""}"
      "${matrixUnitCXX "dbs_room_head.cc"          "dbs_room_head.lo"          ""}"
      "${matrixUnitCXX "dbs_desc.cc"               "dbs_desc.lo"               ""}"
      "${matrixUnitCXX "hook.cc"                   "hook.lo"                   ""}"
      "${matrixUnitCXX "event.cc"                  "event.lo"                  ""}"
      "${matrixUnitCXX "event_cached.cc"           "event_cached.lo"           ""}"
      "${matrixUnitCXX "event_conforms.cc"         "event_conforms.lo"         ""}"
      "${matrixUnitCXX "event_fetch.cc"            "event_fetch.lo"            ""}"
      "${matrixUnitCXX "event_get.cc"              "event_get.lo"              ""}"
      "${matrixUnitCXX "event_id.cc"               "event_id.lo"               ""}"
      "${matrixUnitCXX "event_index.cc"            "event_index.lo"            ""}"
      "${matrixUnitCXX "event_prefetch.cc"         "event_prefetch.lo"         ""}"
      "${matrixUnitCXX "event_prev.cc"             "event_prev.lo"             ""}"
      "${matrixUnitCXX "event_refs.cc"             "event_refs.lo"             ""}"
      "${matrixUnitCXX "room.cc"                   "room.lo"                   ""}"
      "${matrixUnitCXX "room_auth.cc"              "room_auth.lo"              ""}"
      "${matrixUnitCXX "room_aliases.cc"           "room_aliases.lo"           ""}"
      "${matrixUnitCXX "room_bootstrap.cc"         "room_bootstrap.lo"         ""}"
      "${matrixUnitCXX "room_create.cc"            "room_create.lo"            ""}"
      "${matrixUnitCXX "room_events.cc"            "room_events.lo"            ""}"
      "${matrixUnitCXX "room_head.cc"              "room_head.lo"              ""}"
      "${matrixUnitCXX "room_join.cc"              "room_join.lo"              ""}"
      "${matrixUnitCXX "room_leave.cc"             "room_leave.lo"             ""}"
      "${matrixUnitCXX "room_visible.cc"           "room_visible.lo"           ""}"
      "${matrixUnitCXX "room_members.cc"           "room_members.lo"           ""}"
      "${matrixUnitCXX "room_origins.cc"           "room_origins.lo"           ""}"
      "${matrixUnitCXX "room_type.cc"              "room_type.lo"              ""}"
      "${matrixUnitCXX "room_power.cc"             "room_power.lo"             ""}"
      "${matrixUnitCXX "room_state.cc"             "room_state.lo"             ""}"
      "${matrixUnitCXX "room_state_history.cc"     "room_state_history.lo"     ""}"
      "${matrixUnitCXX "room_state_space.cc"       "room_state_space.lo"       ""}"
      "${matrixUnitCXX "room_server_acl.cc"        "room_server_acl.lo"        ""}"
      "${matrixUnitCXX "room_stats.cc"             "room_stats.lo"             ""}"
      "${matrixUnitCXX "user.cc"                   "user.lo"                   ""}"
      "${matrixUnitCXX "user_account_data.cc"      "user_account_data.lo"      ""}"
      "${matrixUnitCXX "user_devices.cc"           "user_devices.lo"           ""}"
      "${matrixUnitCXX "user_events.cc"            "user_events.lo"            ""}"
      "${matrixUnitCXX "user_filter.cc"            "user_filter.lo"            ""}"
      "${matrixUnitCXX "user_ignores.cc"           "user_ignores.lo"           ""}"
      "${matrixUnitCXX "user_mitsein.cc"           "user_mitsein.lo"           ""}"
      "${matrixUnitCXX "user_notifications.cc"     "user_notifications.lo"     ""}"
      "${matrixUnitCXX "user_profile.cc"           "user_profile.lo"           ""}"
      "${matrixUnitCXX "user_pushers.cc"           "user_pushers.lo"           ""}"
      "${matrixUnitCXX "user_pushrules.cc"         "user_pushrules.lo"         ""}"
      "${matrixUnitCXX "user_register.cc"          "user_register.lo"          ""}"
      "${matrixUnitCXX "user_room_account_data.cc" "user_room_account_data.lo" ""}"
      "${matrixUnitCXX "user_room_tags.cc"         "user_room_tags.lo"         ""}"
      "${matrixUnitCXX "user_rooms.cc"             "user_rooms.lo"             ""}"
      "${matrixUnitCXX "user_tokens.cc"            "user_tokens.lo"            ""}"
      "${matrixUnitCXX "acquire.cc"                "acquire.lo"                ""}"
      "${matrixUnitCXX "bridge.cc"                 "bridge.lo"                 ""}"
      "${matrixUnitCXX "breadcrumb_rooms.cc"       "breadcrumb_rooms.lo"       ""}"
      "${matrixUnitCXX "burst.cc"                  "burst.lo"                  ""}"
      "${matrixUnitCXX "display_name.cc"           "display_name.lo"           ""}"
      "${matrixUnitCXX "event_append.cc"           "event_append.lo"           ""}"
      "${matrixUnitCXX "event_horizon.cc"          "event_horizon.lo"          ""}"
      "${matrixUnitCXX "events.cc"                 "events.lo"                 ""}"
      "${matrixUnitCXX "fed.cc"                    "fed.lo"                    ""}"
      "${matrixUnitCXX "feds.cc"                   "feds.lo"                   ""}"
      "${matrixUnitCXX "fetch.cc"                  "fetch.lo"                  ""}"
      "${matrixUnitCXX "gossip.cc"                 "gossip.lo"                 ""}"
      "${matrixUnitCXX "request.cc"                "request.lo"                ""}"
      "${matrixUnitCXX "keys.cc"                   "keys.lo"                   ""}"
      "${matrixUnitCXX "node.cc"                   "node.lo"                   ""}"
      "${matrixUnitCXX "presence.cc"               "presence.lo"               ""}"
      "${matrixUnitCXX "pretty.cc"                 "pretty.lo"                 ""}"
      "${matrixUnitCXX "receipt.cc"                "receipt.lo"                ""}"
      "${matrixUnitCXX "rooms.cc"                  "rooms.lo"                  ""}"
      "${matrixUnitCXX "membership.cc"             "membership.lo"             ""}"
      "${matrixUnitCXX "rooms_summary.cc"          "rooms_summary.lo"          ""}"
      "${matrixUnitCXX "sync.cc"                   "sync.lo"                   ""}"
      "${matrixUnitCXX "typing.cc"                 "typing.lo"                 ""}"
      "${matrixUnitCXX "users.cc"                  "users.lo"                  ""}"
      "${matrixUnitCXX "users_servers.cc"          "users_servers.lo"          ""}"
      "${matrixUnitCXX "error.cc"                  "error.lo"                  ""}"
      "${matrixUnitCXX "push.cc"                   "push.lo"                   ""}"
      "${matrixUnitCXX "filter.cc"                 "filter.lo"                 ""}"
      "${matrixUnitCXX "txn.cc"                    "txn.lo"                    ""}"
      "${matrixUnitCXX "vm.cc"                     "vm.lo"                     ""}"
      "${matrixUnitCXX "vm_eval.cc"                "vm_eval.lo"                ""}"
      "${matrixUnitCXX "vm_inject.cc"              "vm_inject.lo"              ""}"
      "${matrixUnitCXX "vm_execute.cc"             "vm_execute.lo"             ""}"
      "${matrixUnitCXX "init_backfill.cc"          "init_backfill.lo"          ""}"
      "${matrixUnitCXX "homeserver.cc"             "homeserver.lo"             ""}"
      "${matrixUnitCXX "resource.cc"               "resource.lo"               ""}"
      "${matrixUnitCXX "matrix.cc"                 "matrix.lo"                 ""}"
    ] "libircd_matrix.la" "-rpath $out/lib";

    modules = with pkgs; [
      (moduleLD [
        "${moduleUnitCXX [] "m_breadcrumb_rooms.cc"                     "m_breadcrumb_rooms.lo"                     ""}"
      ] "m_breadcrumb_rooms.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_bridge.cc"                               "m_bridge.lo"                               ""}"
      ] "m_bridge.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_command.cc"                              "m_command.lo"                              ""}"
      ] "m_command.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_control.cc"                              "m_control.lo"                              ""}"
      ] "m_control.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_device.cc"                               "m_device.lo"                               ""}"
      ] "m_device.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_device_list_update.cc"                   "m_device_list_update.lo"                   ""}"
      ] "m_device_list_update.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_direct.cc"                               "m_direct.lo"                               ""}"
      ] "m_direct.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_direct_to_device.cc"                     "m_direct_to_device.lo"                     ""}"
      ] "m_direct_to_device.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_ignored_user_list.cc"                    "m_ignored_user_list.lo"                    ""}"
      ] "m_ignored_user_list.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_listen.cc"                               "m_listen.lo"                               ""}"
      ] "m_listen.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_noop.cc"                                 "m_noop.lo"                                 ""}"
      ] "m_noop.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_presence.cc"                             "m_presence.lo"                             ""}"
      ] "m_presence.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_profile.cc"                              "m_profile.lo"                              ""}"
      ] "m_profile.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_push.cc"                                 "m_push.lo"                                 ""}"
      ] "m_push.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_receipt.cc"                              "m_receipt.lo"                              ""}"
      ] "m_receipt.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_relation.cc"                             "m_relation.lo"                             ""}"
      ] "m_relation.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_aliases.cc"                         "m_room_aliases.lo"                         ""}"
      ] "m_room_aliases.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_canonical_alias.cc"                 "m_room_canonical_alias.lo"                 ""}"
      ] "m_room_canonical_alias.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_create.cc"                          "m_room_create.lo"                          ""}"
      ] "m_room_create.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_history_visibility.cc"              "m_room_history_visibility.lo"              ""}"
      ] "m_room_history_visibility.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_join_rules.cc"                      "m_room_join_rules.lo"                      ""}"
      ] "m_room_join_rules.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_member.cc"                          "m_room_member.lo"                          ""}"
      ] "m_room_member.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_message.cc"                         "m_room_message.lo"                         ""}"
      ] "m_room_message.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_name.cc"                            "m_room_name.lo"                            ""}"
      ] "m_room_name.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_power_levels.cc"                    "m_room_power_levels.lo"                    ""}"
      ] "m_room_power_levels.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_redaction.cc"                       "m_room_redaction.lo"                       ""}"
      ] "m_room_redaction.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_server_acl.cc"                      "m_room_server_acl.lo"                      ""}"
      ] "m_room_server_acl.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_room_third_party_invite.cc"              "m_room_third_party_invite.lo"              ""}"
      ] "m_room_third_party_invite.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "m_vm_fetch.cc"                             "m_vm_fetch.lo"                             ""}"
      ] "m_vm_fetch.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "net_dns_cache.cc"                          "net_dns_cache.lo"                          ""}"
      ] "net_dns_cache.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "stats.cc"                                  "stats.lo"                                  ""}"
      ] "stats.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "console.cc"                                "console.lo"                                "${versionDefs}"}"
      ] "console.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "web_root.cc"                               "web_root.lo"                               ""}"
      ] "web_root.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "web_hook.cc"                               "web_hook.lo"                               ""}"
      ] "web_hook.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "well_known.cc"                             "well_known.lo"                             ""}"
      ] "well_known.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "admin" ] "users.cc"                         "users.lo"                              ""}"
      ] "admin_users.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "admin" ] "deactivate.cc"                    "deactivate.lo"                         ""}"
      ] "admin_deactivate.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "versions.cc"                        "versions.lo"                        ""}"
      ] "client_versions.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "events.cc"                          "events.lo"                          ""}"
      ] "client_events.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "login.cc"                           "login.lo"                           ""}"
      ] "client_login.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "logout.cc"                          "logout.lo"                          ""}"
      ] "client_logout.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "sync.cc"                            "sync.lo"                            ""}"
      ] "client_sync.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "presence.cc"                        "presence.lo"                        ""}"
      ] "client_presence.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "profile.cc"                         "profile.lo"                         ""}"
      ] "client_profile.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "devices.cc"                         "devices.lo"                         ""}"
      ] "client_devices.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "pushers.cc"                         "pushers.lo"                         ""}"
      ] "client_pushers.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "publicrooms.cc"                     "publicrooms.lo"                     ""}"
      ] "client_publicrooms.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "createroom.cc"                      "createroom.lo"                      ""}"
      ] "client_createroom.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "pushrules.cc"                       "pushrules.lo"                       ""}"
      ] "client_pushrules.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "join.cc"                            "join.lo"                            ""}"
      ] "client_join.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "publicised_groups.cc"               "publicised_groups.lo"               ""}"
      ] "client_publicised_groups.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "initialsync.cc"                     "initialsync.lo"                     ""}"
      ] "client_initialsync.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "search.cc"                          "search.lo"                          ""}"
      ] "client_search.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "joined_groups.cc"                   "joined_groups.lo"                   ""}"
      ] "client_joined_groups.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "register_available.cc"              "register_available.lo"              ""}"
      ] "client_register_available.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "capabilities.cc"                    "capabilities.lo"                    ""}"
      ] "client_capabilities.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "send_to_device.cc"                  "send_to_device.lo"                  ""}"
      ] "client_send_to_device.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "delete_devices.cc"                  "delete_devices.lo"                  ""}"
      ] "client_delete_devices.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "notifications.cc"                   "notifications.lo"                   ""}"
      ] "client_notifications.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "register_email.cc"                  "register_email.lo"                  ""}"
      ] "client_register_email.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" ] "register.cc"                        "register.lo"                        ""}"
      ] "client_register.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "rooms" ] "messages.cc"                  "messages.lo"                   ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "state.cc"                     "state.lo"                      ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "members.cc"                   "members.lo"                    ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "context.cc"                   "context.lo"                    ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "event.cc"                     "event.lo"                      ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "send.cc"                      "send.lo"                       ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "typing.cc"                    "typing.lo"                     ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "redact.cc"                    "redact.lo"                     ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "receipt.cc"                   "receipt.lo"                    ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "join.cc"                      "join.lo"                       ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "invite.cc"                    "invite.lo"                     ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "leave.cc"                     "leave.lo"                      ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "forget.cc"                    "forget.lo"                     ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "kick.cc"                      "kick.lo"                       ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "ban.cc"                       "ban.lo"                        ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "unban.cc"                     "unban.lo"                      ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "read_markers.cc"              "read_markers.lo"               ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "initialsync.cc"               "initialsync.lo"                ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "report.cc"                    "report.lo"                     ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "relations.cc"                 "relations.lo"                  ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "upgrade.cc"                   "upgrade.lo"                    ""}"
        "${moduleUnitCXX [ "client" "rooms" ] "rooms.cc"                     "rooms.lo"                      ""}"
      ] "client_rooms.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "user" ] "openid.cc"                     "openid.lo"                     ""}"
        "${moduleUnitCXX [ "client" "user" ] "filter.cc"                     "filter.lo"                     ""}"
        "${moduleUnitCXX [ "client" "user" ] "account_data.cc"               "account_data.lo"               ""}"
        "${moduleUnitCXX [ "client" "user" ] "rooms.cc"                      "rooms.lo"                      ""}"
        "${moduleUnitCXX [ "client" "user" ] "user.cc"                       "user.lo"                       ""}"
      ] "client_user.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "directory" ] "room.cc"                  "room.lo"                       ""}"
      ] "client_directory_room.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "directory" ] "user.cc"                  "user.lo"                       ""}"
      ] "client_directory_user.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "directory" "list" ] "room.cc"           "room.lo"                       ""}"
      ] "client_directory_list_room.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "directory" "list" ] "appservice.cc"     "appservice.lo"                 ""}"
      ] "client_directory_list_appservice.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "voip" ] "turnserver.cc"                 "turnserver.lo"                 ""}"
      ] "client_voip_turnserver.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "thirdparty" ] "protocols.cc"            "protocols.lo"                  ""}"
      ] "client_thirdparty_protocols.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" ] "account_data.cc"               "account_data.lo"               ""}"
      ] "client_sync_account_data.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" ] "presence.cc"                   "presence.lo"                   ""}"
      ] "client_sync_presence.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" ] "rooms.cc"                      "rooms.lo"                      ""}"
      ] "client_sync_rooms.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" ] "to_device.cc"                  "to_device.lo"                  ""}"
      ] "client_sync_to_device.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" ] "device_lists.cc"               "device_lists.lo"               ""}"
      ] "client_sync_device_lists.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" ] "device_one_time_keys_count.cc" "device_one_time_keys_count.lo" ""}"
      ] "client_sync_device_one_time_keys_count.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" "rooms" ] "account_data.cc"         "account_data.lo"         ""}"
      ] "client_sync_rooms_account_data.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" "rooms" ] "ephemeral.cc"            "ephemeral.lo"            ""}"
      ] "client_sync_rooms_ephemeral.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" "rooms" ] "state.cc"                "state.lo"                ""}"
      ] "client_sync_rooms_state.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" "rooms" ] "timeline.cc"             "timeline.lo"             ""}"
      ] "client_sync_rooms_timeline.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" "rooms" ] "unread_notifications.cc" "unread_notifications.lo" ""}"
      ] "client_sync_rooms_unread_notifications.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" "rooms" ] "summary.cc"              "summary.lo"              ""}"
      ] "client_sync_rooms_summary.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" "rooms" ] "ephemeral/receipt.cc"    "ephemeral/receipt.lo"    ""}"
      ] "client_sync_rooms_ephemeral_receipt.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "sync" "rooms" ] "ephemeral/typing.cc"     "ephemeral/typing.lo"     ""}"
      ] "client_sync_rooms_ephemeral_typing.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "keys" ] "upload.cc"                     "upload.lo"                     ""}"
      ] "client_keys_upload.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "keys" ] "query.cc"                      "query.lo"                      ""}"
      ] "client_keys_query.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "keys" ] "claim.cc"                      "claim.lo"                      ""}"
      ] "client_keys_claim.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "keys" ] "changes.cc"                    "changes.lo"                    ""}"
      ] "client_keys_changes.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "keys" ] "signatures/upload.cc"          "signatures/upload.lo"          ""}"
      ] "client_keys_signatures_upload.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "keys" ] "device_signing/upload.cc"      "device_signing/upload.lo"      ""}"
      ] "client_keys_device_signing_upload.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "room_keys" ] "version.cc"               "version.lo"                    ""}"
      ] "client_room_keys_version.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "room_keys" ] "keys.cc"                  "keys.lo"                       ""}"
      ] "client_room_keys_keys.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "client" "account" ] "3pid.cc"                    "3pid.lo"                       ""}"
        "${moduleUnitCXX [ "client" "account" ] "whoami.cc"                  "whoami.lo"                     ""}"
        "${moduleUnitCXX [ "client" "account" ] "password.cc"                "password.lo"                   ""}"
        "${moduleUnitCXX [ "client" "account" ] "deactivate.cc"              "deactivate.lo"                 ""}"
        "${moduleUnitCXX [ "client" "account" ] "account.cc"                 "account.lo"                    ""}"
      ] "client_account.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "send.cc"                        "send.lo"                        ""}"
      ] "federation_send.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "event.cc"                       "event.lo"                       ""}"
      ] "federation_event.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "get_missing_events.cc"          "get_missing_events.lo"          ""}"
      ] "federation_get_missing_events.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "get_groups_publicised.cc"       "get_groups_publicised.lo"       ""}"
      ] "federation_get_groups_publicised.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "version.cc"                     "version.lo"                     ""}"
      ] "federation_version.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "sender.cc"                      "sender.lo"                      ""}"
      ] "federation_sender.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "query.cc"                       "query.lo"                       ""}"
      ] "federation_query.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "invite.cc"                      "invite.lo"                      ""}"
      ] "federation_invite.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "invite2.cc"                     "invite2.lo"                     ""}"
      ] "federation_invite2.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "make_join.cc"                   "make_join.lo"                   ""}"
      ] "federation_make_join.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "send_join.cc"                   "send_join.lo"                   ""}"
      ] "federation_send_join.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "state_ids.cc"                   "state_ids.lo"                   ""}"
      ] "federation_state_ids.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "state.cc"                       "state.lo"                       ""}"
      ] "federation_state.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "make_leave.cc"                  "make_leave.lo"                  ""}"
      ] "federation_make_leave.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "send_leave.cc"                  "send_leave.lo"                  ""}"
      ] "federation_send_leave.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "backfill.cc"                    "backfill.lo"                    ""}"
      ] "federation_backfill.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "backfill_ids.cc"                "backfill_ids.lo"                ""}"
      ] "federation_backfill_ids.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "event_auth.cc"                  "event_auth.lo"                  ""}"
      ] "federation_event_auth.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "query_auth.cc"                  "query_auth.lo"                  ""}"
      ] "federation_query_auth.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "publicrooms.cc"                 "publicrooms.lo"                 ""}"
      ] "federation_publicrooms.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "user_devices.cc"                "user_devices.lo"                ""}"
      ] "federation_user_devices.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "user_keys_query.cc"             "user_keys_query.lo"             ""}"
      ] "federation_user_keys_query.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "user_keys_claim.cc"             "user_keys_claim.lo"             ""}"
      ] "federation_user_keys_claim.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "federation" ] "rooms.cc"                       "rooms.lo"                       ""}"
      ] "federation_rooms.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "identity" ] "v1.cc"                            "v1.lo"                          ""}"
      ] "identity_v1.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "identity" ] "pubkey.cc"                        "pubkey.lo"                      ""}"
      ] "identity_pubkey.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "key" ] "server.cc"                             "server.lo"                      ""}"
      ] "key_server.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "key" ] "query.cc"                              "query.lo"                       ""}"
      ] "key_query.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [ "media" ] "download.cc"                         "download.lo"                    ""}"
        "${moduleUnitCXX [ "media" ] "upload.cc"                           "upload.lo"                      ""}"
        "${moduleUnitCXX [ "media" ] "thumbnail.cc"                        "thumbnail.lo"                   ""}"
        "${moduleUnitCXX [ "media" ] "preview_url.cc"                      "preview_url.lo"                 ""}"
        "${moduleUnitCXX [ "media" ] "config.cc"                           "config.lo"                      ""}"
        "${moduleUnitCXX [ "media" ] "media.cc"                            "media.lo"                       ""}"
      ] "media_media.la" "-rpath $out/lib/modules")
      (moduleLD [
        "${moduleUnitCXX [] "magick.cc" "magick_la-magick.lo" "-I${graphicsmagick.out}/include/GraphicsMagick"}"
      ] "magick.la" "-rpath $out/lib/modules -lGraphicsMagick++ -lGraphicsMagickWand -lGraphicsMagick")
    ];

    construct = with pkgs; constructLD [
      "${constructUnitCXX "construct.cc" "construct.o" "${versionDefs}"}"
      "${constructUnitCXX "lgetopt.cc"   "lgetopt.o" ""}"
      "${constructUnitCXX "console.cc"   "console.o" ""}"
      "${constructUnitCXX "signals.cc"   "signals.o" ""}"
    ] "construct";
  in with pkgs; ''
    mkdir -p $out
    ln -s ${includes} $out/include

    mkdir -p $out/share
    ln -s ${source}/share/webapp $out/share/webapp

    mkdir -p $out/lib
    libtool --mode=install ${coreutils.out}/bin/install -c ${ircd} $out/lib
    libtool --mode=install ${coreutils.out}/bin/install -c ${matrix} $out/lib

    mkdir -p $out/lib/modules
    libtool --mode=install ${coreutils.out}/bin/install -c ${lib.concatStringsSep " " modules} $out/lib/modules

    mkdir -p $out/bin
    libtool --mode=install ${coreutils.out}/bin/install -c ${construct} $out/bin

    wrapProgram $out/bin/construct \
      --set-default ircd_web_root_path ${riot-web.out} \
      --set-default ircd_fs_base_prefix $out \
      --set-default ircd_fs_base_bin $out/bin \
      --set-default ircd_fs_base_lib $out/lib \
      --set-default ircd_fs_base_modules $out/lib/modules \
      --argv0 $out/bin/construct

    makeWrapper ${gdb}/bin/gdb $out/bin/construct-gdb \
      --set-default ircd_web_root_path $out/share/webapp \
      --set-default ircd_fs_base_prefix $out \
      --set-default ircd_fs_base_bin $out/bin \
      --set-default ircd_fs_base_lib $out/lib \
      --set-default ircd_fs_base_modules $out/lib/modules \
      --add-flags "$out/bin/.construct-wrapped"
  '';

  doInstallCheck = true;
  installCheckPhase = ''
    chmod -R a-w $out
    mkdir -p /tmp/cache/construct
    export RUNTIME_DIRECTORY=/tmp/run/construct
    export STATE_DIRECTORY=/tmp/lib/construct
    export LOGS_DIRECTORY=/tmp/log/construct
    cd /tmp/cache/construct
    $out/bin/construct -smoketest localhost
  '';

  #outputs = [ "bin" "dev" "out" "doc" ];
  #separateDebugInfo = true;
  enableParallelBuilding = true;
}
