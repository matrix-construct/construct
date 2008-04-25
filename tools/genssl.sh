#!/bin/sh
echo "Generating certificate request .. "
openssl req -new -nodes -out ../etc/req.pem

echo "Generating self-signed certificate .. "
openssl req -x509 -days 365 -in ../etc/req.pem -key ../etc/rsa.key -out ../etc/cert.pem

echo "Generating Diffie-Hellman file for secure SSL/TLS negotiation .. "
openssl dhparam -out ../etc/dh.pem 1024

echo " 
Now change these lines in the IRCd config file:

    ssl_private_key = "etc/rsa.key";
    ssl_cert = "etc/cert.pem";
    ssl_dh_params = "etc/dh.pem";

Enjoy using ssl.
"
