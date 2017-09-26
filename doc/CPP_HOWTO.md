# How to CPP for Charybdis


In the post-C++11 world it is time to leave C99 behind and seriously consider
C++ as C proper. It has been a hard 30 year journey to finally earn that, but
now it is time. This document is the effective style guide for how Charybdis
will integrate -std=gnu++14 and how developers should approach it.


### C++ With Respect For C People


Remember your C heritage. There is nothing wrong with C, it is just incomplete.
There is also no overhead with C++, that is a myth. If you write C code in C++
it will be the same C code. Think about it like this: if C is like a bunch of
macros on assembly, C++ is a bunch of macros on C. This guide will not address
any more myths and for that we refer you [here](https://isocpp.org/blog/2014/12/myths-3).

###### Repeat the following mantra:
1. How would I do this in C?
2. Why is that dangerous, hacky, or ugly?
3. What feature does C++ offer to do it right?

This can be applied to many real patterns seen in C software which really beg
for something C++ did to make it legitimate and proper. Examples:
* Leading several structures with the same member, then casting to that leading
type to deal with the structure abstractly for container insertion. -> Think
inheritance.
* Creating a structure with a bunch of function pointers, then having a user
of the structure fill in the pointers with their own functionality. -> Think
virtual functions.


======


#### Encapsulation will be relaxed


To summarize, most structures will default to being fully public unless there
is a very pressing reason to create a private section. Such a reason is not
"the user *could* break something by touching this," instead it is "the user
*will only ever* break something by touching this."

* Do not use the keyword `class` unless your sole intent is to have the members
immediately following it be private.

* Using `class` followed by a `public:` label is nubile.


#### Direct initialization


Use `=` only for assignment to an existing object. *Break your C habit right now.*
Use bracket initialization `{}` of all variables and objects. Fall back to parens `()`
if absolutely necessary to quash warnings about conversions.

* Do not put uninitialized variables at the top of a function and assign them later.

> Quick note to preempt a confusion for C people:
> Initialization in C++ is like C but you don't have to use the `=`.
>
> ```C++
> struct user { const char *nick; };
> struct user you = {"you"};
> user me {"me"};
> ```
>

* Use allman style for complex/long initialization statements. It's like a function
  returning the value to your new object; it is easier to read then one giant line.

> ```C++
> const auto sum
> {
>     1 + (2 + (3 * 4) + 5) + 6
> };
> ```


#### Use full const correctness


`const` correctness should extend to all variables, pointers, arguments, and
functions- not just "pointed-to" data. If it *can* be `const` then make it
`const` and relax it later if necessary.


#### Use auto


Use `auto` whenever it is possible to use it; specify a type when you must.
If the compiler can't figure out the auto, that's when you indicate the type.


#### RAII will be in full force


All variables, whether they're function-local, class-members, even globals,
must always be under some protection at all times. There must be the
expectation at *absolutely any point* including *between those points*
everything will blow up randomly and the protection will be invoked to back-out
the way you came. That is, essentially, **the juice of why we are here.**

**This is really serious business.** You have to do one thing at a time. When you
move on to the next thing the last thing has to have already fully succeeded
or fully failed. Everything is a **transaction**. Nothing in the future exists.
There is nothing you need from the future to give things a consistent state.

* The program should be effectively reversible -- should be able to "go backwards"
or "unwind" from any point. **Think in terms of stacks, not linear procedures.**
This means when a variable, or member (a **resource**) first comes into scope,
i.e. it is declared or accessible (**acquired**), it must be **initialized**
to a completely consistent state at that point.


#### Exceptions will be used


Wait, you were trolling "respect for C people" right? **No.** If you viewed
the above section merely through the prism avoiding classic memory leaks, and
can foresee how to now write stackful, reversible, protected programs without
even calling free() or delete: you not only have earned the right, but you
**have** to use exceptions. This is no longer a matter of arguing for or
against `if()` statement clutter and checking return types and passing errors
down the stack.

* Object construction (logic in the initialization list, constructor body, etc)
is actual real program logic. Object construction is **not something to just
prepare some memory, like initializing it to zero**, leaving an instance
somewhere for further functions to conduct operations on. Your whole program
could be running - the entire universe could be running - in some member
initializer somewhere. The only way to error out of this is to throw, and it
is perfectly legitimate to do so.

* **Function bodies and return types should not be concerned with error
handling and passing of such. They only cause and generate the errors.**

* Try/catch style note: We specifically discourage naked try/catch blocks.
In other words, **most try-catch blocks are of the
[function-try-catch](http://en.cppreference.com/w/cpp/language/function-try-block)
variety.** The style is simply to piggyback the try/catch where another block
would have been.

> ```C++
> while(foo) try
> {
>     ...
> }
> catch(exception)
> {
> }
> ```

* We extend this demotion style of keywords to `do` as well, which should
  avoid having its own line if possible.

> ```C++
> int x; do
> {
>     ...
> }
> while((x = foo());
> ```



#### Pointers and References


* Biblical maxim: Use references when you can, pointers when you must.

* Pass arguments by const reference `const foo &bar` preferably, non-const
reference `foo &bar` if you must.

* Use const references even if you're not referring to anything created yet.
const references can construct, contain, and refer to an instance of the type
with all in one magic.

* Passing by value indicates some kind of need for object construction in
the argument, or that something may be std::move()'ed to and from it. Except
for some common patterns, this is generally suspect.

* Passing to a function with an rvalue reference argument `foo &&bar` indicates
something will be std::move()'ed to it, and ownership is now acquired by that
function.

* In a function with a template `template<class foo>`, an rvalue reference in
the prototype for something in the template `void func(foo &&bar)` is actually
a [universal reference](https://isocpp.org/blog/2012/11/universal-references-in-c11-scott-meyers)
which has some differences from a normal rvalue reference. To make this clear
our style is to move the `&&` like so `void func(foo&& bar)`. This actually has
a real use, because a variadic template foo
`template<class... foo>` will require the prototype `void func(foo&&... bar)`.

* Passing a pointer, or pointer arguments in general, indicates something may
be null, or optional. Otherwise suspect.

* Avoid using references as object members, you're most likely just limiting
the ability to assign, move, and reuse the object because references cannot be
reseated; then the "~~big three~~" "big five" custom constructors have to be
created and maintained, and it becomes an unnecessary mess.


#### Miscellaneous


* new and delete should rarely if ever be seen. This is more true than ever with
C++14 std::make_unique() and std::make_shared().

* We allow some C-style arrays, especially on the stack, even C99 dynamic sized ones;
there's no problem here, just be responsible.

* std::array is preferred for object members; also just generally preferred.

* C format strings are still acceptable. This is an IRC project, with heavy
use of strings and complex formats and all the stringencies. We even have
our own custom *protocol safe* format string library, and that should be used
where possible.

* streams and standard streams are generally avoided in this project. We could have
have taken the direction to customize C++'s stream interface to make it
performant, but otherwise the streams are generally slow and heavy. Instead we
chose a more classical approach with format strings and buffers -- but without
sacrificing type safety with our RTTI-based fmt library.

* ~~varargs are still legitimate.~~ There are just many cases when template
varargs, now being available, are a better choice; they can also be inlined.

	* I think a better case to use our template va_rtti is starting to emerge in
	most of our uses for varags.

* When using a `switch` over an `enum` type, put what would be the `default` case after/outside
of the `switch` unless the situation specifically calls for one. We use -Wswitch so changes to
the enum will provide a good warning to update any `switch`.

* Prototypes should name their argument variables to make them easier to understand, except if
such a name is redundant because the type carries enough information to make it obvious. In
other words, if you have a prototype like `foo(const std::string &message)` you should name
`message` because std::string is common and *what* the string is for is otherwise opaque.
OTOH, if you have `foo(const options &, const std::string &message)` one should skip the name
for `options &` as it just adds redundant text to the prototype.
