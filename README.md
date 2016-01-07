telephone
=========

You would not use text fields to store dates, so why use text fields to store telephone numbers.

telephone provides a telephone data type for storing telephone numbers.

* Formatting is normalized.

* Letters are supported.

* Equality compares identity, not representation.  For example: 2 = A = B = C, "1 2" = "12"

* Pause and confirm directives are supported.  Like letters and whitespace, they are not part of the identity.

If you store a telephone number with letters, you can also find it by its numbers.

The above describes the digits mode/format.

If the numbers starts with a +, there are additional features.

However, only some calling codes are supported, and some calling codes may have partial support.

Wrapper functions can remove the + when a dialing plan is unsupported.

When using the calling code mode/format (+), additional metadata and formatting functionality is available:

* Validation by dialing plan (invalid ranges & misplaced white-space, and incomplete numbers generate an error).

* E.123 format.

* Domestic formatting (when requested).

* Numbers only formatting (changes letters to numbers, removes directives & the extension, when requested).

* Extension indicator.

* Service type (land/geographical, cell, toll-free, charge).

* Fictitious telephone number indicator.

To build it, just do this:

    make
    make installcheck
    make install

If you encounter an error such as:

    "Makefile", line 8: Need an operator

You need to use GNU make, which may well be installed on your system as
`gmake`:

    gmake
    gmake install
    gmake installcheck

If you encounter an error such as:

    make: pg_config: Command not found

Be sure that you have `pg_config` installed and in your path. If you used a
package management system such as RPM to install PostgreSQL, be sure that the
`-devel` package is also installed. If necessary tell the build process where
to find it:

    env PG_CONFIG=/path/to/pg_config make && make installcheck && make install

If you encounter an error such as:

    ERROR:  must be owner of database regression

You need to run the test suite using a super user, such as the default
"postgres" super user:

    make installcheck PGUSER=postgres

Once telephone is installed, you can add it to a database. If you're running
PostgreSQL 9.1.0 or greater, it's a simple as connecting to a database as a
super user and running:

    CREATE EXTENSION telephone;

If you want to install telephone and all of its supporting objects into a specific
schema, use the `PGOPTIONS` environment variable to specify the schema, like
so:

    PGOPTIONS=--search_path=extensions psql -d mydb -f telephone.sql

Dependencies
------------
The `telephone` data type has no dependencies other than PostgreSQL.

Copyright and License
---------------------

Copyright (c) 2016 The maintainer's name.

telephone copyright is novated to PostgreSQL Global Development Group.
