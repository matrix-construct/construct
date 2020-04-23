
# TROUBLESHOOTING

##### Useful program options

Start the daemon with one or more of the following program options to make it
easier to troubleshoot and perform maintenance:

- *-single* will start in "single user mode" which is a convenience combination
of *-nolisten -wa -console* options described below.

- *-nolisten* will disable the loading of any listener sockets during startup.

- *-wa* write-avoid will discourage (but not deny) writes to the database. This
prevents a lot of background tasks and other noise for any maintenance.

- *-console* convenience to immediately drop to the adminstrator console
after startup.

- *-debug* enables full debug log output.

##### Recovering from broken configurations

If your server ever fails to start from an errant conf item: you can override
any item using an environmental variable before starting the program. To do
this simply replace the '.' characters with '_' in the name of the item when
setting it in the environment. The name is otherwise the same, including its
lower case.

##### Recovering from database corruption

In very rare cases after a hard crash the journal cannot completely restore
data before the crash. Due to the design of rocksdb and the way we apply it
for Matrix, data is lost in chronological order starting from the most recent
transaction (matrix event). The database is consistent for all events up until
the first corrupt event, called the point-in-time.

When any loss has occurred the daemon will fail to start normally. To enable
point-in-time recovery use the command-line option `-pitrecdb` at the next
invocation.

##### Trouble with reverse proxies and middlewares

Construct is designed to be capable internet service software and should
perform best when directly interfacing with remote parties. Nevertheless,
some users wish to employ middlewares known as "reverse-proxies" through
which all communication is forwarded. This gives the appearance, from the
server's perspective, that all clients are connecting from the same IP
address on different ports.

At this time there are some known issues with reverse proxies which may be
mitigated by administrators having reviewed the following:

1. The connection limit from a single remote IP address must be raised from
its default, for example by entering the following in !control or console:

```
conf set ircd.client.max_client_per_peer 65535
```

2. The server does not yet support non-SSL listening sockets. Administrators
may have to generate locally signed certificates for communication from the
reverse-proxy.
