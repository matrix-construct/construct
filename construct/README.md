# Construct Server

Construct is the executable running `libircd`. This application provides an
interface for the server administrator to start, stop, configure and locally
communicate with the daemon. It sets up an `asio::io_service` which is passed
to `libircd`, then it sets up signal handling, and then it runs the ios event
loop until commanded to exit.

This program executes in the foreground. It does not "daemonize" anymore with a
`fork()` etc. You are free to use your shell to execute or move the program
to the background, or simply use a `tmux` or `screen`. Construct will output
the libircd log to stdout and stderr by default.

### Signals

Construct handles certain POSIX signals and their behavior is documented
below. Signals are only handled here in the Construct executable; libircd
itself does not use any signal logic (that we know about).

* Signal handling is accomplished through `boost::asio`'s mechanism which
installs a handler to intercept the signal's delivery and posts it to the
event loop for execution at the next event slice. This is how signal safety
is achieved. Furthermore, according to boost docs, when signals are used
this way they can be compatible with windows environments.

##### SIGQUIT

A `ctrl-\` to Construct will cleanly shut down the server. It will not generate
a coredump.

##### SIGINT

A `ctrl-c` to Construct will bring up the command line console interface. It
will not halt the daemon. Log messages will be suppressed while the console
is waiting for input, but service is still continuing in the background.

##### SIGHUP

A "HangUP" to Construct is only relevant to the command line console, and
signals it to close like an `EOF`. The legacy functionality for reloading
server configuration et al is moved to `SIGUSR1`.

##### SIGUSR1

This signal commands the server to reload and refresh various aspects of its
configuration and running state.
