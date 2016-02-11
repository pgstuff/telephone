telephone
=========

Synopsis
--------

  SELECT telephone_number_only('800 555-01AZ'::telephone);
  8005550129

Description
-----------

You would not use text fields to store dates, so why use text fields to store
telephone numbers.

telephone provides a telephone data type for storing telephone numbers.

* Formatting is normalized.  Punctuation is replaced with spaces.  Consecutive
  spaces are reduced to single spaces.

* Letters are supported.

* Equality compares identity, not representation.  For example: 2 = A = B = C,
  "1 2" = "12"

* Pause and confirm directives are supported.  Like letters and white-space,
  they are not part of the identity.

* Numbers only formatting (no white-space, changes letters to numbers & removes
  directives, when requested).

If you store a telephone number with letters, you can also find it by its
numbers.  This is analogous to case insensitive matching in text.  Letters must
be in uppercase.

Unique constraints will make telephone numbers be unique by identity,
irrespective of their format.

The above describes the digits mode.

If the numbers start with a +, a valid calling code and supported dialing plan
is required.  This mode provides additional features.  However, only some
calling codes are supported, and some calling codes may have partial dialing
plan support.

Wrapper functions can remove the + when a dialing plan is unsupported.  The
wrapper is needed because when a dialing plan is unsupported, you still want to
store the phone number (unless if you are using a completely supported dialing
plan, then an error counts as a validation problem).  So the wrapper functions
remove the + and use the digits mode if there is a calling code or dialing plan
problem.  Note that it might not be possible to distinguish between an
unsupported calling code or dialing plan and an invalid number.  Currently, only
a few dialing plans, such as the North American Numbering Plan, are fully
supported.  If you get an error with the 1 calling code, chances are, it is
because the number is invalid.  Note that some dialing plans make it impossible
to dial a telephone number from another calling code.  Some of these numbers
include local services.  If you need to store telephone numbers that are not
accessible from another calling code, then use the digits mode.  You may want to
include an additional column to specify which calling code these numbers are
accessible from.

When using the calling code mode (+), additional metadata and formatting
functionality are available:

* Validation by dialing plan (invalid ranges, misplaced white-space, and
  incomplete numbers generate an error).

* E.123 format (formatting with spaces only).

* Domestic formatting (when requested).

* Public numbers only formatting (changes letters to numbers, removes directives
  & the extension, when requested).

* Extension indicator.

* Service type (land/geographical, cell, toll-free, charge).

* Fictitious telephone number indicator.

What telephone is not:

* A geocoding library or network identification library.  This information
  should be kept in a table and installed as a separate extension.  This library
  (telephone) can return the geographical and network segments needed for
  supporting the table joins in the other library.  Keep reading for an example.

Examples:

When the text is parsed, the formatting is normalized:
SELECT '1 (800) -  555.01AZ  ;  123'::telephone
Returns: 1 800 555 01AZ;123

Letters are equal to their numeric form and formatting differences are ignored:
SELECT 'ABCD'::telephone = '222 3'::telephone
Returns: True

Only return a complete NANP number, without letters, formatting, or extensions:
SELECT telephone_domestic_numbers_get('800 555-01AZ 123'::telephone, 1, 10);
SELECT telephone_domestic_numbers_get('+1 800 555 0129 x 1E'::telephone, 1, 10)
Both return: 8005550129

Return a telephone from text, assuming calling code 1 if not specified:
SELECT telephone_domestic_assume_set('800 555 0199 x1E', 1)
Returns: +1 800 555 0199 x1E

Use the domestic formatting if the number is in calling code 1, otherwise, use
the E.123 format:
SELECT telephone_domestic_prefer_get(phone_number, 1)
FROM (VALUES
    ('+18005550199'::telephone),
    ('+441134960262'::telephone)) AS phone_numbers (phone_number)
Returns:
(800) 555-0199
+44 113 496 0262

See test/expected/base.out for more examples.

Example geocoding tables:

CREATE EXTENSION telephone;

CREATE TABLE telephone_geo_numbers (
    phone_prefix telephone PRIMARY KEY,
    area_name text
);

CREATE TABLE telephone_example_numbers (
    phone_number telephone PRIMARY KEY,
    example_name text
);

INSERT INTO telephone_geo_numbers VALUES ('+1^', 'North American Numbering Plan');
INSERT INTO telephone_geo_numbers VALUES ('+1 510^', 'Oakland');
INSERT INTO telephone_geo_numbers VALUES ('+1 800 555 0199', 'Fictitious Phone Numer');
INSERT INTO telephone_geo_numbers VALUES ('+44^', 'United Kingdom');

INSERT INTO telephone_example_numbers VALUES ('+1 800 555 0198', 'USA Unlisted Area Code Example');
INSERT INTO telephone_example_numbers VALUES ('+1 510 555 0199', 'USA Landline Example');
INSERT INTO telephone_example_numbers VALUES ('+1 800 555 0199', 'USA Toll-free Example');
INSERT INTO telephone_example_numbers VALUES ('+44 20 7946 0000', 'London Example');

SELECT  DISTINCT ON (telephone_example_numbers.phone_number)
        *
FROM    telephone_example_numbers LEFT OUTER JOIN
        telephone_geo_numbers ON
        telephone_geo_numbers.phone_prefix = ANY(telephone_geo_parts_get(telephone_example_numbers.phone_number, TRUE, TRUE))
ORDER BY telephone_example_numbers.phone_number, LENGTH(telephone_geo_numbers.phone_prefix::text) DESC

Returns:
+1 510 555 0199     USA Landline Example            +1 510^         Oakland
+1 800 555 0198     USA Unlisted Area Code Example  +1^             North American Numbering Plan
+1 800 555 0199     USA Toll-free Example           +1 800 555 0199 Fictitious Phone Numer
+44 20 7946 0000    London Example                  +44^            United Kingdom

See doc/geo_nanp.sql and doc/geo_earth.sql for more.

The ^ directive allows an incomplete number to be stored.  This is useful if you
only want to include a portion of a dialing plan.  When a partial calling code
telephone number is displayed, the ^ is included as an indicator that the number
is incomplete per its dialing plan.  The ^ character is not stored.

Usage
-----

CREATE EXTENSION telephone;

CREATE TABLE telephone_numbers(
  telephone telephone PRIMARY KEY,
  notes     text);

INSERT INTO telephone_numbers(telephone, notes)
VALUES ('1 (800) 555-01AZ'::telephone, 'test');

SELECT * FROM telephone_numbers WHERE telephone = '18005550129'::telephone;

SELECT telephone_domestic_numbers_get('800 555-01AZ'::telephone, 1);

Support
-------

  The Internet.

Author
------

[The maintainer's name]

Copyright and License
---------------------

Copyright (c) 2016 The maintainer's name.

telephone copyright is novated to the PostgreSQL Global Development Group.
