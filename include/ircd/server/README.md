## Interface To Remote Servers

This system manages connections and requests to remote servers when this
server plays the role of client. This interface allows its user to make
concurrent requests using the promise/future pattern. It is built on top of
`ircd::ctx::future`, as well as many functions of `ircd::net` and also
uses `ircd::http`.
