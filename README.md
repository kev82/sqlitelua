[![Build Status](https://travis-ci.com/kev82/sqlitelua.svg?branch=master)](https://travis-ci.com/kev82/sqlitelua)

### Motivation

I have done some work in a project where an in-memory sqlite3 database is used
as part of a data transform pipeline. Using sqlite3 has provided great benefits
in ensuring the raw data that enters the pipeline is self consistent and meets
specific requirements through the use of foreign keys, check constraints, and
raise(). Writing these things declaratively has been orders of magnitude easier
than trying to implement the checks in an imperative style.
 
However, the declarative nature of sql combined with the core functions that
come with sqlite has made some things very difficult to express. Some simple
checks have resulted in 500 line CTEs, others have been skipped entirely as the
cost of implementing the check directly in sql is more than the benefit gained
from the check.

Sqlite's extension API makes it very easy to add new functions from C, and we
did this for the exponential function, as it was needed within the pipeline.
However, for more complex functions, we wanted to be able edit and version them
with the schema in environments without a compiler. This meant embedding some
kind of extension language in sqlite.

Knowing Lua's suitability as an extension language, a search of github led to
two projects

- https://github.com/hoelzro/sqlite-lua-extension
- https://github.com/abiliojr/sqlite-lua

Neither really seemed suitable. The first does not let you create functions,
the second does not behave in a dbms-way, ie there is no table of functions,
and hence no obvious way to save them in the database, or drop them.

### SqliteLua Project

This is an sqlite extension that creates a virtual table module `luafunctions`.
Currently that is an eponymous only table, but the plan is to be able to extend
to a virtual table with storage, allowing functions to be saved within a
database. By inserting code into the virtual table, a extension function is
added that executes the code.

The lua code when executed must return four values:

1. An array of argument names
1. An array of return value names
1. The type of function (simple, aggregate, table)
1. The actual function itself.

There is a single `lua_State` per `luafunctions` virtual table. All the added
functions run in coroutines within this state. Each function gets its own copy
of a sandboxed environment, with limited functionality (eg no i/o). Yields are
used when the function needs to consume multiple inputs (aggregate) or produce
multiple outputs (table).

Refer to the tests for examples of working with the module and different types
of functions.

#### Simple functions

Also know as scalar functions, these take some arguments and return a value
based on those arguments. The `inputs` array is only used to determine the
number of arguments.  The `returns` array is not used. The first value yielded
or returned is taken as the result.

#### Aggregate functions
 
The first input will come through the function arguments. Subsequent rows will
arrive through the returns from calling `coroutine.yield`. The first
argument/return should always be passed to the `aggregate_now` function. If
this returns true, then the database has passed all rows and now wants the
result of the aggregation.

Like simple functions, the `inputs` array is only used to get the number of
arguments. The `returns` array is not used.

#### Table functions

The inputs define the accepted arguments (which are hidden columns), the
outputs define the non-hidden columns of the table. The table will only
support equality constraints on its hidden columns, and all must be set. If
this is the case then the function is exeuted in a coroutine the first call
will contain the arguments, rows of output should be passed back with
coroutine.yield. The return value from yield is undefined and should be
ignored. When nothng is returned, it is assumed all rows have been produced.

### Building

The included Makefile with the project has been written so it will work both on
my linux VM while developing, and with travis-ci linked to github. It is
probably not the best way to build the extension generally.

To build as a sqlite extension (so/dylib/dll) you must

- Compile all the c files in the src directory together as c99.
- Make sure the headers for lua and sqlite3 can be found.
- Link statically with a liblua.a.
