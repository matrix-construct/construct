## JavaScript Object Notation

##### formal grammars & tools

The IRCd JSON subsystem is meant to be a fast, safe, and extremely
lightweight interface. We have taken a somewhat non-traditional approach
and it's important for the developer to understand a few things.

Most JSON interfaces are functions to convert some JSON input to and from
text into native-machine state like JSON.parse() for JS, boost::ptree, etc.
For a parsing operation, they make a pass recursing over the entire text,
allocating native structures, copying data into them, indexing their keys,
and perhaps performing native-type conversions and checks to present the
user with a final tree of machine-state usable in their language. The
original input is then discarded.

Instead, we are interested in having the ability to *compute directly over
JSON text* itself, and perform the allocating, indexing, copying and
converting entirely at the time and place of our discretion -- if ever.

The core of this system is a robust and efficient abstract formal grammar
built with boost::spirit. The formal grammar provides a *proof of
robustness*: security vulnerabilities are more easily spotted by vetting
this grammar rather than laboriously tracing the program flow of an
informal handwritten parser.

Next we have taught boost::spirit how to parse into std::string_view rather
than std::string. Parsing is now a composition of pointers into the original
string of JSON. No dynamic allocation ever takes place. No copying of data
ever takes place. IRCd can service an entire request from the original
network input with absolutely minimal requisite cost.

The output side is also ambitious but probably a little more friendly to
the developer. We leverage boost::spirit here also providing *formally
proven* output safety. In other words, the grammar prevents exploits like
injecting and terminating JSON as it composes the output.
