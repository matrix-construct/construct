#!/bin/sh
echo "Generating self-signed certificate .. "
openssl req -x509 -nodes -newkey rsa:1024 -keyout ../etc/test.key -out ../etc/test.cert

echo "Generating Diffie-Hellman file for secure SSL/TLS negotiation .. "
openssl dhparam -out ../etc/dh.pem 1024

echo " 
Now change these lines in the IRCd config file:

    ssl_private_key = "etc/test.key";
    ssl_cert = "etc/test.cert";
    ssl_dh_params = "etc/dh.pem";

Enjoy using ssl.
"
