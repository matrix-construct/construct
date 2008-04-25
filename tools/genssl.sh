#!/bin/sh
echo "Generating certificate request .. "
openssl req -new -nodes -out req.pem
echo "Generating self-signed certificate .. "
openssl req -x509 -days 365 -in req.pem -key privkey.pem -out cert.pem
echo "Generating Diffie-Hellman file for secure SSL/TLS negotiation .. "
openssl dhparam -out dh.pem 2048
mv privkey.pem rsa.key

echo " 
Now copy rsa.key, cert.pem and dh.pem into your IRCd's etc/ folder,
then change these lines in the ircd.conf file:

    ssl_private_key = "etc/rsa.key";
    ssl_cert = "etc/cert.pem";
    ssl_dh_params = "etc/dh.pem";

Enjoy using ssl.
"
