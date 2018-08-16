# Utility toolchest

This namespace contains an accumulated collection of miscellaneous classes
and functions which have demonstrated value some place in the project, but are
not really specific to the project's application. While most of libircd is
arguably a utility suite itself (i.e tokens.h and lex_cast.h and base.h
which are not within ircd::util) the contents of this namespace tend to be
even more abstract, and serve as utilities to the utilities, etc. This
collection is here to save developers time and enforce DRY.

These tools still rank below ircd::exception, *_view, buffer:: and allocator::
and can still make use of those considering the linear inclusion strategy of
the project (see: ircd.h).
