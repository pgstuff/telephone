/*
 * Author: The maintainer's name
 * Created at: Wed Oct 21 12:43:52 -0400 2015
 *
 */

--
-- This is a example code genereted automaticaly
-- by pgxn-utils.

SET client_min_messages = warning;

CREATE TYPE telephone;

CREATE TYPE telephone_format AS ENUM (
    'international', 'domestic', 'digits_only');

CREATE TYPE telephone_subfield AS ENUM (
    'digits', 'calling_code',
    'group1', 'group2', 'group3', 'group4',
    'subscriber', 'extension');

CREATE TYPE telephone_service AS ENUM (
    'unknown_unknown', 'known_unknown',
    'land', 'other_geo', 'land_or_cell', 'cell', 'pager', 'voip', 'follow_me',
    'tollfree', 'shared_cost', 'caller_cost', 'charge',
    'carrier_services_intra', 'carrier_services_international',
    'government', 'voicemail', 'other');

CREATE TYPE telephone_fictitious AS ENUM (
    'real', 'real_mistaken',
    'real_fictitious', 'fictitious_unreserved', 'fictitious_reserved');

CREATE TYPE telephone_mode AS ENUM (
    'calling_code', 'digits');

CREATE FUNCTION telephone_in(cstring)
   RETURNS telephone
   AS '$libdir/telephone'
   LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION telephone_out(telephone)
   RETURNS cstring
   AS '$libdir/telephone'
   LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION telephone_recv(internal)
    RETURNS telephone
    AS '$libdir/telephone'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION telephone_send(telephone)
    RETURNS bytea
    AS '$libdir/telephone'
    LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE telephone (
   internallength = VARIABLE,
   input = telephone_in,
   output = telephone_out,
   receive = telephone_recv,
   send = telephone_send,
   LIKE = bytea
);


COMMENT ON TYPE telephone IS 'Telephone digits';

/* Does this survive a pg_dump?
Use bytea cast for debugging only.  Never use it to force invalid data in.  You might not get your data back.
CREATE CAST (bytea       AS telephone)   WITHOUT FUNCTION AS ASSIGNMENT;
CREATE CAST (telephone   AS bytea)       WITHOUT FUNCTION;
*/

-- to/from text conversion
CREATE OR REPLACE FUNCTION telephone_to_text(telephone) RETURNS text AS '$libdir/telephone'
LANGUAGE C IMMUTABLE STRICT;
CREATE OR REPLACE FUNCTION telephone_from_text(text) RETURNS telephone AS '$libdir/telephone'
LANGUAGE C IMMUTABLE STRICT;

-- cast from/to text
-- Disable these 2 casts if you want to force type casting in your SQL.
CREATE CAST (telephone AS text) WITH FUNCTION telephone_to_text(telephone) AS ASSIGNMENT;
CREATE CAST (text AS telephone) WITH FUNCTION telephone_from_text(text) AS ASSIGNMENT;

--
-- Operator Functions.
--

CREATE OR REPLACE FUNCTION telephone_eq(telephone, telephone)
RETURNS bool
AS '$libdir/telephone'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION telephone_ne(telephone, telephone)
RETURNS bool
AS '$libdir/telephone'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION telephone_lt(telephone, telephone)
RETURNS bool
AS '$libdir/telephone'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION telephone_le(telephone, telephone)
RETURNS bool
AS '$libdir/telephone'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION telephone_gt(telephone, telephone)
RETURNS bool
AS '$libdir/telephone'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION telephone_ge(telephone, telephone)
RETURNS bool
AS '$libdir/telephone'
    LANGUAGE C IMMUTABLE STRICT;


CREATE OPERATOR < (
    PROCEDURE = telephone_lt,
    LEFTARG = telephone,
    RIGHTARG = telephone,
    COMMUTATOR = >,
    NEGATOR = >=,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);


--ALTER OPERATOR pg_catalog.< (telephone, telephone) OWNER TO postgres;

COMMENT ON OPERATOR < (telephone, telephone) IS 'less than';


CREATE OPERATOR <= (
    PROCEDURE = telephone_le,
    LEFTARG = telephone,
    RIGHTARG = telephone,
    COMMUTATOR = >=,
    NEGATOR = >,
    RESTRICT = scalarltsel,
    JOIN = scalarltjoinsel
);


--ALTER OPERATOR pg_catalog.<= (telephone, telephone) OWNER TO postgres;


COMMENT ON OPERATOR <= (telephone, telephone) IS 'less than or equal';


CREATE OPERATOR <> (
    PROCEDURE = telephone_ne,
    LEFTARG = telephone,
    RIGHTARG = telephone,
    COMMUTATOR = <>,
    NEGATOR = =,
    RESTRICT = neqsel,
    JOIN = neqjoinsel
);


--ALTER OPERATOR pg_catalog.<> (telephone, telephone) OWNER TO postgres;

COMMENT ON OPERATOR <> (telephone, telephone) IS 'not equal';


CREATE OPERATOR = (
    PROCEDURE = telephone_eq,
    LEFTARG = telephone,
    RIGHTARG = telephone,
    COMMUTATOR = =,
    NEGATOR = <>,
    MERGES,
    HASHES,
    RESTRICT = eqsel,
    JOIN = eqjoinsel
);


--ALTER OPERATOR pg_catalog.= (telephone, telephone) OWNER TO postgres;

COMMENT ON OPERATOR = (telephone, telephone) IS 'equal';


CREATE OPERATOR > (
    PROCEDURE = telephone_gt,
    LEFTARG = telephone,
    RIGHTARG = telephone,
    COMMUTATOR = <,
    NEGATOR = <=,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);


--ALTER OPERATOR pg_catalog.> (telephone, telephone) OWNER TO postgres;

COMMENT ON OPERATOR > (telephone, telephone) IS 'greater than';

CREATE OPERATOR >= (
    PROCEDURE = telephone_ge,
    LEFTARG = telephone,
    RIGHTARG = telephone,
    COMMUTATOR = <=,
    NEGATOR = <,
    RESTRICT = scalargtsel,
    JOIN = scalargtjoinsel
);


--ALTER OPERATOR pg_catalog.>= (telephone, telephone) OWNER TO postgres;

COMMENT ON OPERATOR >= (telephone, telephone) IS 'greater than or equal';

--
-- Indexing Functions.
--

CREATE OR REPLACE FUNCTION telephone_cmp(telephone, telephone)
RETURNS int4
AS '$libdir/telephone'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION telephone_hash(telephone)
RETURNS int4
AS '$libdir/telephone'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OPERATOR CLASS telephone_ops
DEFAULT FOR TYPE telephone USING btree AS
    OPERATOR    1   <  (telephone, telephone),
    OPERATOR    2   <= (telephone, telephone),
    OPERATOR    3   =  (telephone, telephone),
    OPERATOR    4   >= (telephone, telephone),
    OPERATOR    5   >  (telephone, telephone),
    FUNCTION    1   telephone_cmp(telephone, telephone);

CREATE OPERATOR CLASS telephone_ops
DEFAULT FOR TYPE telephone USING hash AS
    OPERATOR    1   =  (telephone, telephone),
    FUNCTION    1   telephone_hash(telephone);

--
-- Aggregates.
--

CREATE OR REPLACE FUNCTION telephone_smaller(telephone, telephone)
RETURNS telephone
AS '$libdir/telephone'
    LANGUAGE C IMMUTABLE STRICT;

CREATE OR REPLACE FUNCTION telephone_larger(telephone, telephone)
RETURNS telephone
AS '$libdir/telephone'
    LANGUAGE C IMMUTABLE STRICT;

CREATE AGGREGATE min(telephone)  (
    SFUNC = telephone_smaller,
    STYPE = telephone,
    SORTOP = <
);

CREATE AGGREGATE max(telephone)  (
    SFUNC = telephone_larger,
    STYPE = telephone,
    SORTOP = >
);

/*
CREATE OPERATOR CLASS _telephone_ops
    DEFAULT FOR TYPE telephone[] USING gin FAMILY array_ops AS
    STORAGE telephone;


--ALTER OPERATOR CLASS pg_catalog._telephone_ops USING gin OWNER TO postgres;


CREATE OPERATOR CLASS telephone_minmax_ops
    DEFAULT FOR TYPE telephone USING brin AS
    STORAGE telephone;


--ALTER OPERATOR CLASS pg_catalog.telephone_minmax_ops USING brin OWNER TO postgres;


CREATE OPERATOR CLASS telephone_ops
    DEFAULT FOR TYPE telephone USING btree AS
    ;


--ALTER OPERATOR CLASS pg_catalog.telephone_ops USING btree OWNER TO postgres;

CREATE OPERATOR CLASS telephone_ops
    DEFAULT FOR TYPE telephone USING hash AS
    ;


--ALTER OPERATOR CLASS pg_catalog.telephone_ops USING hash OWNER TO postgres;
*/

CREATE OR REPLACE FUNCTION public.telephone_to_format(
    telephone telephone,
    format_type telephone_format,
    inclusive_subfields boolean,
    subfields telephone_subfield[],
    letters boolean,
    pause_confirm boolean,
    allow_digits_mode boolean)
  RETURNS text AS
'$libdir/telephone', 'telephone_to_format'
  LANGUAGE c IMMUTABLE STRICT
  COST 3;

CREATE OR REPLACE FUNCTION public.telephone_mode_get(telephone telephone)
  RETURNS telephone_mode AS
'$libdir/telephone', 'telephone_mode_get'
  LANGUAGE c IMMUTABLE STRICT
  COST 1;

CREATE OR REPLACE FUNCTION public.telephone_calling_code_get(telephone telephone)
  RETURNS smallint AS
'$libdir/telephone', 'telephone_calling_code_get'
  LANGUAGE c IMMUTABLE STRICT
  COST 2;

CREATE OR REPLACE FUNCTION public.telephone_service_get(telephone telephone)
  RETURNS telephone_service AS
'$libdir/telephone', 'telephone_service_get'
  LANGUAGE c IMMUTABLE STRICT
  COST 2;

CREATE OR REPLACE FUNCTION public.telephone_fictitious_get(telephone telephone)
  RETURNS telephone_fictitious AS
'$libdir/telephone', 'telephone_fictitious_get'
  LANGUAGE c IMMUTABLE STRICT
  COST 2;

CREATE OR REPLACE FUNCTION public.telephone_geo_is(telephone telephone)
  RETURNS boolean AS
'$libdir/telephone', 'telephone_geo_is'
  LANGUAGE c IMMUTABLE STRICT
  COST 2;

CREATE OR REPLACE FUNCTION public.telephone_geo_parts_get(
    telephone telephone,
    include_calling_code boolean,
    include_subscriber boolean)
    RETURNS telephone[]
    AS '$libdir/telephone'
    LANGUAGE C IMMUTABLE STRICT
    COST 2;

/*CREATE OR REPLACE FUNCTION public.telephone_number_only_part(telephone telephone)
  RETURNS telephone AS
'$libdir/telephone', 'telephone_number_only_part'
  LANGUAGE c IMMUTABLE STRICT
  COST 2;*/


CREATE OR REPLACE FUNCTION public.telephone_domestic_numbers_get(
    telephone telephone,
    calling_code integer,
    expected_length integer)
  RETURNS text AS
$BODY$
DECLARE
    _mode telephone_mode;
    _calling_code integer;
    _text text;
    _non_number_first integer;
BEGIN
    _mode := public.telephone_mode_get($1);

    IF _mode = 'calling_code'::public.telephone_mode THEN
        _calling_code := public.telephone_calling_code_get($1);
        IF calling_code != _calling_code THEN
            RETURN NULL;
        END IF;
    END IF;

    _text := telephone_to_format(telephone => $1, format_type => 'digits_only'::public.telephone_format,
        inclusive_subfields => FALSE, subfields => '{calling_code,extension}'::public.telephone_subfield[],
        letters => FALSE, pause_confirm => FALSE, allow_digits_mode => TRUE);

    IF _mode = 'calling_code'::public.telephone_mode THEN
        RETURN _text;
    END IF;

    IF LENGTH(_text) < expected_length THEN
        RETURN NULL;
    END IF;

    RETURN substring(_text for expected_length);
END;
$BODY$
  LANGUAGE plpgsql IMMUTABLE STRICT
  COST 4;
COMMENT ON FUNCTION public.telephone_domestic_numbers_get(telephone, integer, integer) IS 'Only return numbers (no letters or formatting).  Works with both digits and calling code.  For digits, make sure that the length is complete and omit anything after the length (which is assumed to be an extension).  For calling code, return NULL if the calling code is not the requested calling code.';


CREATE OR REPLACE FUNCTION public.telephone_domestic_numbers_get(
    telephone telephone,
    calling_code integer)
  RETURNS text AS
$BODY$
DECLARE
    _mode telephone_mode;
    _calling_code integer;
    _text text;
    _extension_start integer;
BEGIN
    _mode := public.telephone_mode_get($1);

    IF _mode = 'calling_code'::public.telephone_mode THEN
        _calling_code := public.telephone_calling_code_get($1);
        IF calling_code != _calling_code THEN
            RETURN NULL;
        END IF;
    END IF;

    _text := telephone_to_format(telephone => $1, format_type => 'digits_only'::public.telephone_format,
        inclusive_subfields => FALSE, subfields => '{calling_code,extension}'::public.telephone_subfield[],
        letters => FALSE, pause_confirm => TRUE, allow_digits_mode => TRUE);

    IF _mode = 'calling_code'::public.telephone_mode THEN
        RETURN _text;
    END IF;

    _extension_start := LEAST(NULLIF(position(';' in _text), 0), NULLIF(position(',' in _text), 0));

    IF _extension_start IS NULL THEN
        RETURN _text;
    END IF;

    RETURN substring(_text for _extension_start - 1);
END;
$BODY$
  LANGUAGE plpgsql IMMUTABLE STRICT
  COST 4;
COMMENT ON FUNCTION public.telephone_domestic_numbers_get(telephone, integer) IS 'Only return numbers (no letters or formatting).  Works with both digits and calling code.  For digits, the extension is assumed to be indicated with a confirm or pause.  For calling code, return NULL if the calling code is not the requested calling code.';


CREATE OR REPLACE FUNCTION public.telephone_extension_numbers_get(
    telephone telephone,
    extension_start integer)
  RETURNS text AS
$BODY$
DECLARE
    _text text;
BEGIN
    _text := telephone_to_format(telephone => $1, format_type => 'digits_only'::public.telephone_format,
        inclusive_subfields => TRUE, subfields => '{extension}'::public.telephone_subfield[],
        letters => FALSE, pause_confirm => FALSE, allow_digits_mode => TRUE);

    IF public.telephone_mode_get($1) != 'calling_code'::public.telephone_mode THEN
        IF LENGTH(_text) >= extension_start THEN
            RETURN substring(_text from extension_start);
        END IF;
        RETURN NULL;
    ELSE
        RETURN _text;
    END IF;
END;
$BODY$
  LANGUAGE plpgsql IMMUTABLE STRICT
  COST 3;
COMMENT ON FUNCTION public.telephone_extension_numbers_get(telephone, integer) IS 'Returns the extension, without letters or formatting.  If digits, the location of where the extension begins is used.';


CREATE OR REPLACE FUNCTION public.telephone_extension_numbers_get(
    telephone telephone)
  RETURNS text AS
$BODY$
DECLARE
    _text text;
    _extension_start integer;
BEGIN
    IF public.telephone_mode_get($1) = 'calling_code'::public.telephone_mode THEN
        RETURN telephone_to_format(telephone => $1, format_type => 'digits_only'::public.telephone_format,
            inclusive_subfields => TRUE, subfields => '{extension}'::public.telephone_subfield[],
            letters => FALSE, pause_confirm => FALSE, allow_digits_mode => FALSE);
    END IF;

    _text := telephone_to_format(telephone => $1, format_type => 'digits_only'::public.telephone_format,
        inclusive_subfields => TRUE, subfields => '{extension}'::public.telephone_subfield[],
        letters => FALSE, pause_confirm => TRUE, allow_digits_mode => TRUE);

    _extension_start := LEAST(NULLIF(position(';' in _text), 0), NULLIF(position(',' in _text), 0));

    IF _extension_start IS NULL THEN
        RETURN NULL;
    END IF;

    RETURN replace(replace(substring(_text from _extension_start + 1), ',',''), ';','');
END;
$BODY$
  LANGUAGE plpgsql IMMUTABLE STRICT
  COST 3;
COMMENT ON FUNCTION public.telephone_extension_numbers_get(telephone) IS 'Returns the extension, without letters or formatting.  For digits, the extension is assumed to be indicated with a confirm or pause.';


CREATE OR REPLACE FUNCTION public.telephone_domestic_prefer_get(
    telephone telephone,
    calling_code integer)
  RETURNS text AS
$BODY$
DECLARE
    _mode telephone_mode;
    _calling_code integer;
    _text text;
BEGIN
    _mode := public.telephone_mode_get($1);

    IF _mode = 'calling_code'::public.telephone_mode THEN
        _calling_code := public.telephone_calling_code_get($1);
        IF $2 = _calling_code THEN
            RETURN telephone_to_format(telephone => $1, format_type => 'domestic'::public.telephone_format,
                inclusive_subfields => FALSE, subfields => '{calling_code}'::public.telephone_subfield[],
                letters => TRUE, pause_confirm => TRUE, allow_digits_mode => TRUE);
        END IF;
    END IF;

    RETURN telephone_to_format(telephone => $1, format_type => 'international'::public.telephone_format,
        inclusive_subfields => FALSE, subfields => '{}'::public.telephone_subfield[],
        letters => TRUE, pause_confirm => TRUE, allow_digits_mode => TRUE);
END;
$BODY$
  LANGUAGE plpgsql IMMUTABLE STRICT
  COST 4;
COMMENT ON FUNCTION public.telephone_domestic_prefer_get(telephone, integer) IS 'When a number is in the specified calling code, use the domestic format.  Otherwise, use the international format.';

CREATE OR REPLACE FUNCTION public.telephone_domestic_assume_set(
    telephone_text text,
    calling_code integer)
  RETURNS telephone AS
$BODY$
DECLARE
    _telephone_trimmed text = trim(telephone_text);
BEGIN
    IF LEFT(_telephone_trimmed, 1) = '+' THEN
        RETURN _telephone_trimmed::telephone;
    ELSE
        RETURN ('+' || calling_code || _telephone_trimmed)::telephone;
    END IF;

    -- If the + format failed, store by digits
    EXCEPTION WHEN OTHERS THEN
        _telephone_trimmed := replace(_telephone_trimmed, 'extn', ';'); -- Uppercase is reserved for digits.
        _telephone_trimmed := replace(_telephone_trimmed, 'ext', ';');  -- If your input does not make use of digit letters, a case
        _telephone_trimmed := replace(_telephone_trimmed, 'ex', ';');   -- insensitive replace can be used:
        _telephone_trimmed := replace(_telephone_trimmed, 'x', ';');    -- regexp_replace(_telephone_trimmed, 'extn', ';', 'gi');
        IF LEFT(_telephone_trimmed, 1) = '+' THEN
            RETURN substring(_telephone_trimmed from 2)::telephone;
        ELSE
            RETURN _telephone_trimmed::telephone;
        END IF;
END;
$BODY$
  LANGUAGE plpgsql IMMUTABLE STRICT
  COST 3;
COMMENT ON FUNCTION public.telephone_domestic_assume_set(text, integer) IS 'If a calling code is not specified in the text, assume a specified calling code.  If the calling code does not work, use digits.  When digits is used, the calling code is not added.';

CREATE OR REPLACE FUNCTION public.telephone_set(
    telephone_text text)
  RETURNS telephone AS
$BODY$
DECLARE
    _telephone_trimmed text = trim(telephone_text);
BEGIN
    RETURN _telephone_trimmed::telephone;

    -- If the + format failed, store by digits
    EXCEPTION WHEN OTHERS THEN
        _telephone_trimmed := replace(_telephone_trimmed, 'extn', ';'); -- Uppercase is reserved for digits.
        _telephone_trimmed := replace(_telephone_trimmed, 'ext', ';');  -- If your input does not make use of digit letters, a case
        _telephone_trimmed := replace(_telephone_trimmed, 'ex', ';');   -- insensitive replace can be used:
        _telephone_trimmed := replace(_telephone_trimmed, 'x', ';');    -- regexp_replace(_telephone_trimmed, 'extn', ';', 'gi');
        IF LEFT(_telephone_trimmed, 1) = '+' THEN
            RETURN substring(_telephone_trimmed from 2)::telephone;
        ELSE
            RETURN _telephone_trimmed::telephone;
        END IF;
END;
$BODY$
  LANGUAGE plpgsql IMMUTABLE STRICT
  COST 3;
COMMENT ON FUNCTION public.telephone_set(text) IS 'If the calling code is not recognized, use digits instead.';

CREATE OR REPLACE FUNCTION public.telephone_fictitious_is(
    telephone telephone)
  RETURNS boolean AS
$BODY$
BEGIN
    RETURN public.telephone_fictitious_get($1) >= 'real_fictitious'::public.telephone_fictitious;
END;
$BODY$
  LANGUAGE plpgsql IMMUTABLE STRICT
  COST 3;

/* QUESTIONABLE_BYTEA_API

CREATE OR REPLACE FUNCTION telephone_ident_bytes_get(telephone)
    RETURNS bytea
    AS '$libdir/telephone'
    LANGUAGE C IMMUTABLE STRICT
    COST 1;

QUESTIONABLE_BYTEA_API */
