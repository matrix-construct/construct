## Memory Buffer Tools

This is a modernization of the `(char *buf, size_t buf_sz)` pattern used
when working with segments of RAMs. While in C99 it is possible (and
recommended) for a project to create a `struct buffer { char *ptr; size_t size; };`
and then manually perform object semantics `buffer_copy(dst, src);`
`buffer_move(dst, src)``buffer_free(buf);` etc, we create those devices using
C++ language features here instead.

This suite is born out of (though not directly based on) the boost::asio buffer
objects `boost::asio::const_buffer` and `boost::asio::mutable_buffer` and the
two principal objects used ubiquitously throughout IRCd share the same names
and general properties. We also offer conversions between them for developers
working with any asio interfaces directly.

To summarize some basics about these tools:

- Most of these interfaces are "thin" and don't own their underlying data, nor
will they copy their underlying data even if their instance itself is copied.

- We work with signed `char *` (and `const char *`) types. We do not work with
`void` pointers because size integers always represent a count of single bytes
and there is no reason to lose or confuse that information. If `unsigned char *`
types are required by some library function an explicit cast to `uint8_t *` may
be required especially to avoid warnings. Note that we compile this project with
`-fsigned-char` and don't support platforms that have incompatible conversions.
