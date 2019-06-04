
# TROUBLESHOOTING

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
