# FREQUENTLY ASKED QUESTIONS


##### Why does it say IRCd everywhere?

This is a long story which is not covered in full here. The short version
is that this project was originally intended to implement an IRC federation
using an extended superset of the rfc1459/rfc2812 protocol. This concept went
through several iterations. The Atheme Services codebase was first considered
for development into a "gateway" for IRC networks to connect to each other.
That was succeeded by the notion of eliminating separate services-daemons in
favor of IRCd-meshing for redundancy and scale. At that point Charybdis/4 was
chosen as a basis for the project.

Around this time, the Matrix protocol was emerging as a potential candidate
for federating synchronous-messaging. Though far from perfect, it had enough
potential to outweigh the troubles of inventing and promoting yet another
messaging protocol in a wildly diverse and already saturated space.

Somewhile after, the original collaborators of this endeavor became
disillusioned by many of the finer details of Matrix. Many red-flags observed
about its stewards, community, and the overall engineering requirements placed
on implementations made it clear this project's goals would never be reached in
a timely or cost-effective way. Coupled with the political situation and
death-spiral of IRC itself, the original collaborators disbanded.

One developer decided to continue by simplifying the mission down to just
creating a Matrix server first, and worrying about IRC later, maybe through
TS6, or maybe never. This reasoning was bolstered by the ongoing poor
performance of Matrix's principal reference implementation in python+pgsql.
Today there is virtually nothing left of any original IRCd. The project
namespaces like "ircd::" and IRCD_ remain but they too might be replaced by
"ctor" etc at some time in the future.


##### Why is there a SpiderMonkey JavaScript embedding?

One of the goals of this project is realtime team collaboration and
development inside chat rooms. The embedding is intended to replace the
old notion of running a "bot" which is just a single instance of a program
that some user connects. The embedding facilitates a cloud-esque or so-called
"lambda" ecosystem of many untrusted user-written modules that are stored
and managed by the server.

*The SpiderMonkey embedding is defunct and no longer developed. It is planned
to be succeeded by WASM.*
