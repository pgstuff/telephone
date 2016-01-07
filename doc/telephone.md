telephone
=========

Synopsis
--------

  SELECT telephone_number_only('800 555-01AZ'::telephone);
  8005550129

Description
-----------

You would not use text fields to store dates, so why use text fields to store telephone numbers.

telephone provides a telephone data type for storing telephone numbers.

* Formatting is normalized.

* Letters are supported.

* Equality compares identity, not representation.  For example: 2 = A = B = C, "1 2" = "12"

* Pause and confirm directives are supported.  Like letters and whitespace, they are not part of the identity.

* Numbers only formatting (no whitespace, changes letters to numbers & removes directives, when requested).

If you store a telephone number with letters, you can also find it by its numbers.

Unique constraints will make telephone numbers be unique by identity, not format.

The above describes the digits mode/format.

If the numbers starts with a +, there are additional features.

However, only some calling codes are supported, and some calling codes may have partial support.

The provided wrapper functions can remove the + when a dialing plan is unsupported.

When using the calling code mode/format (+), additional metadata and formatting functionality is available:

* Validation by dialing plan (invalid ranges, misplaced white-space & incomplete numbers generate an error).

* E.123 format.

* Domestic formatting (when requested).

* Numbers only formatting (no whitespace, changes letters to numbers, removes directives & the extension, when requested).

* Extension indicator.

* Service type (land/geographical, cell, toll-free, charge).

* Fictitious telephone number function.

To build it, just do this:

Usage
-----

CREATE EXTENSION telephone;

CREATE TABLE telephone_numbers(
telephone telephone PRIMARY KEY,
notes text);

INSERT INTO telephone_numbers(telephone, notes)
VALUES ('1 (800) 555-01AZ'::telephone, 'test');

SELECT * FROM telephone_numbers WHERE telephone = '18005550129'::telephone;

SELECT telephone_number_only('800 555-01AZ'::telephone);

Support
-------

  The Internet.

Author
------

[The maintainer's name]

Copyright and License
---------------------

Copyright (c) 2016 The maintainer's name.

telephone copyright is novated to PostgreSQL Global Development Group.
