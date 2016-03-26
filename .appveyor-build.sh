set -v

export MSYSTEM=MINGW64
export PATH=/mingw64/bin:/usr/local/bin:/usr/bin:/bin:/c/Windows/system32:/c/Windows:/c/Windows/System32/Wbem:/c/Windows/System32/WindowsPowerShell/v1.0:/usr/bin/site_perl:/usr/bin/vendor_perl:/usr/bin/core_perl

sh ./autogen.sh
./configure --prefix=c:/projects/charybdis/build --enable-openssl=/mingw64
make -j2
make install
