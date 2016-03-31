CREATE EXTENSION telephone;

CREATE TABLE telephones(id serial primary key, telephone telephone unique);
INSERT INTO telephones(telephone) VALUES('123456789*#');
INSERT INTO telephones(telephone) VALUES('ABCDEFGHIJKLMNOPQRSTUVWXYZ');
INSERT INTO telephones(telephone) VALUES('1 1');
INSERT INTO telephones(telephone) VALUES('2p2');
INSERT INTO telephones(telephone) VALUES('2w3');
INSERT INTO telephones(telephone) VALUES('1/1');
INSERT INTO telephones(telephone) VALUES('1,2');
INSERT INTO telephones(telephone) VALUES('1;3');;
INSERT INTO telephones(telephone) VALUES('10000003');
INSERT INTO telephones(telephone) VALUES('30000001');
INSERT INTO telephones(telephone) VALUES('20000002');
INSERT INTO telephones(telephone) VALUES('2000002');
INSERT INTO telephones(telephone) VALUES('1000003');
INSERT INTO telephones(telephone) VALUES('3000001');
INSERT INTO telephones(telephone) VALUES('520002');
INSERT INTO telephones(telephone) VALUES('51000003');
INSERT INTO telephones(telephone) VALUES('5300001');
INSERT INTO telephones(telephone) VALUES('41000034');
INSERT INTO telephones(telephone) VALUES('43000014');
INSERT INTO telephones(telephone) VALUES('42000024');
INSERT INTO telephones(telephone) VALUES('80000003');
INSERT INTO telephones(telephone) VALUES('80000002');
INSERT INTO telephones(telephone) VALUES('80000001');
INSERT INTO telephones(telephone) VALUES('8000003');
INSERT INTO telephones(telephone) VALUES('8000002');
INSERT INTO telephones(telephone) VALUES('8000001');
INSERT INTO telephones(telephone) VALUES('4  1');
INSERT INTO telephones(telephone) VALUES('4 / 1');
INSERT INTO telephones(telephone) VALUES('4 ; 2');
INSERT INTO telephones(telephone) VALUES('4 , 3');
INSERT INTO telephones(telephone) VALUES('5 // 1');
INSERT INTO telephones(telephone) VALUES('5 ;; 1');
INSERT INTO telephones(telephone) VALUES('5 ,, 2');
INSERT INTO telephones(telephone) VALUES('6 , / , 6');
INSERT INTO telephones(telephone) VALUES('6 , ; , 6');
INSERT INTO telephones(telephone) VALUES('7 , / , 7');
INSERT INTO telephones(telephone) VALUES('7 , ; , 7');
INSERT INTO telephones(telephone) VALUES('');
INSERT INTO telephones(telephone) VALUES('########');

SELECT * FROM telephones ORDER BY telephone;

SELECT MIN(telephone) AS min FROM telephones;
SELECT MAX(telephone) AS max FROM telephones;

-- index scan
TRUNCATE telephones;
INSERT INTO telephones(telephone) SELECT '4'||id FROM generate_series(5678, 8000) id;
SELECT id,telephone::text FROM telephones WHERE telephone = '48000';

SET enable_seqscan = false;
SELECT id,telephone::text FROM telephones WHERE telephone = '46000';
SELECT id,telephone FROM telephones WHERE telephone >= '47000' LIMIT 5;
SELECT count(id) FROM telephones;
SELECT count(id) FROM telephones WHERE telephone <> ('46500'::text)::telephone;
RESET enable_seqscan;

-- operators and conversions
SELECT '0'::telephone < '0'::telephone;
SELECT '0'::telephone > '0'::telephone;
SELECT '0'::telephone < '1'::telephone;
SELECT '0'::telephone > '1'::telephone;
SELECT '0'::telephone <= '0'::telephone;
SELECT '0'::telephone >= '0'::telephone;
SELECT '0'::telephone <= '1'::telephone;
SELECT '0'::telephone >= '1'::telephone;
SELECT '0'::telephone <> '0'::telephone;
SELECT '0'::telephone <> '1'::telephone;
SELECT '0'::telephone = '0'::telephone;
SELECT '0'::telephone = '1'::telephone;

-- COPY FROM/TO
TRUNCATE telephones;
COPY telephones(telephone) FROM STDIN;

########
\.
COPY telephones TO STDOUT;

-- clean up --
DROP TABLE telephones;

-- errors
SELECT '!'::telephone;
SELECT '+1 800 555 019'::telephone;
SELECT '+1 80 0 555 0199'::telephone;
SELECT '0000000001111111111222222222233333333334444444444555555555566'::telephone;

-- calling code mode
SELECT  telephone_to_format(telephone => '+1 800 555 0199 x 1E 3,4;5'::telephone, format_type => 'international'::public.telephone_format, inclusive_subfields => FALSE, subfields => '{}', letters => TRUE, pause_confirm => TRUE, allow_digits_mode => TRUE),
        telephone_to_format(telephone => '+44 1632 960123 x 1E 3,4;5'::telephone, format_type => 'international'::public.telephone_format, inclusive_subfields => FALSE, subfields => '{}', letters => TRUE, pause_confirm => TRUE, allow_digits_mode => TRUE),
        telephone_to_format(telephone => '+61 2 5550 0123 x 1E 3,4;5'::telephone, format_type => 'international'::public.telephone_format, inclusive_subfields => FALSE, subfields => '{}', letters => TRUE, pause_confirm => TRUE, allow_digits_mode => TRUE);

SELECT  telephone_to_format(telephone => '+1 800 555 0199 x 1E 3,4;5'::telephone, format_type => 'domestic'::public.telephone_format, inclusive_subfields => FALSE, subfields => '{calling_code}', letters => TRUE, pause_confirm => TRUE, allow_digits_mode => TRUE),
        telephone_to_format(telephone => '+44 1632 960123 x 1E 3,4;5'::telephone, format_type => 'domestic'::public.telephone_format, inclusive_subfields => FALSE, subfields => '{calling_code}', letters => TRUE, pause_confirm => TRUE, allow_digits_mode => TRUE),
        telephone_to_format(telephone => '+61 2 5550 0123 x 1E 3,4;5'::telephone, format_type => 'domestic'::public.telephone_format, inclusive_subfields => FALSE, subfields => '{calling_code}', letters => TRUE, pause_confirm => TRUE, allow_digits_mode => TRUE);

SELECT  telephone_to_format(telephone => '+1 800 555 0199 x 1E 3,4;5'::telephone, format_type => 'digits_only'::public.telephone_format, inclusive_subfields => FALSE, subfields => '{}', letters => FALSE, pause_confirm => TRUE, allow_digits_mode => TRUE),
        telephone_to_format(telephone => '+44 1632 960123 x 1E 3,4;5'::telephone, format_type => 'digits_only'::public.telephone_format, inclusive_subfields => FALSE, subfields => '{}', letters => FALSE, pause_confirm => TRUE, allow_digits_mode => TRUE),
        telephone_to_format(telephone => '+61 2 5550 0123 x 1E 3,4;5'::telephone, format_type => 'digits_only'::public.telephone_format, inclusive_subfields => FALSE, subfields => '{}', letters => FALSE, pause_confirm => TRUE, allow_digits_mode => TRUE);

SELECT  telephone_to_format(telephone => '+1 800 555 0199 x 1E 3,4;5'::telephone, format_type => 'digits_only'::public.telephone_format, inclusive_subfields => FALSE, subfields => '{calling_code}', letters => FALSE, pause_confirm => FALSE, allow_digits_mode => TRUE),
        telephone_to_format(telephone => '+44 1632 960123 x 1E 3,4;5'::telephone, format_type => 'digits_only'::public.telephone_format, inclusive_subfields => FALSE, subfields => '{calling_code}', letters => FALSE, pause_confirm => FALSE, allow_digits_mode => TRUE),
        telephone_to_format(telephone => '+61 2 5550 0123 x 1E 3,4;5'::telephone, format_type => 'digits_only'::public.telephone_format, inclusive_subfields => FALSE, subfields => '{calling_code}', letters => FALSE, pause_confirm => FALSE, allow_digits_mode => TRUE);

SELECT  telephone_to_format(telephone => '+1 800 555 0199 x 1E 3,4;5'::telephone, format_type => 'digits_only'::public.telephone_format, inclusive_subfields => FALSE, subfields => '{calling_code,extension}', letters => FALSE, pause_confirm => FALSE, allow_digits_mode => TRUE),
        telephone_to_format(telephone => '+44 1632 960123 x 1E 3,4;5'::telephone, format_type => 'digits_only'::public.telephone_format, inclusive_subfields => FALSE, subfields => '{calling_code,extension}', letters => FALSE, pause_confirm => FALSE, allow_digits_mode => TRUE),
        telephone_to_format(telephone => '+61 2 5550 0123 x 1E 3,4;5'::telephone, format_type => 'digits_only'::public.telephone_format, inclusive_subfields => FALSE, subfields => '{calling_code,extension}', letters => FALSE, pause_confirm => FALSE, allow_digits_mode => TRUE);

SELECT  telephone_mode_get('+1 800 555 0199 x 1E 3,4;5'::telephone),
        telephone_mode_get('1 800 555 0199 ; 1E 3,4;5'::telephone);

SELECT  telephone_calling_code_get('+1 800 555 0199 x 1E 3,4;5'::telephone),
        telephone_calling_code_get('1 800 555 0199 ; 1E 3,4;5'::telephone);

SELECT  telephone_service_get('+1 800 555 0199 x 1E 3,4;5'::telephone),
        telephone_service_get('1 800 555 0199 ; 1E 3,4;5'::telephone);

SELECT  telephone_fictitious_get('+1 800 555 0199 x 1E 3,4;5'::telephone),
        telephone_fictitious_get('1 800 555 0199 ; 1E 3,4;5'::telephone);

SELECT  telephone_fictitious_get('+44 1632 960000 21'::telephone);

SELECT  telephone_fictitious_get('+44 113 496 0000 21'::telephone),
        telephone_fictitious_get('+44 121 496 0000 21'::telephone),
        telephone_fictitious_get('+44 20 7946 0000 21'::telephone),
        telephone_fictitious_get('+44 191 498 0000 21'::telephone),
        telephone_fictitious_get('+44 28 9018 0000 21'::telephone),
        telephone_fictitious_get('+44 29 2018 0000 21'::telephone);

SELECT  telephone_fictitious_get('+44 7700 900000 21'::telephone),
        telephone_fictitious_get('+44 808 157 0000 21'::telephone),
        telephone_fictitious_get('+44 909 879 0000 21'::telephone),
        telephone_fictitious_get('+44 306 999 0000 21'::telephone);

SELECT  telephone_fictitious_is('+1 201 555 0199 x 1E 3,4;5'::telephone),
        telephone_fictitious_is('1 201 555 0199 ; 1E 3,4;5'::telephone);

SELECT  telephone_domestic_numbers_get('+1 800 555 0199 x 1E 3,4;5'::telephone, 1, 10),
        telephone_domestic_numbers_get('+1 800 555 0199 x 1E 3,4;5'::telephone, 7, 10);
SELECT  telephone_domestic_numbers_get('1 800 555 0199 ; 1E 3,4;5'::telephone, 1, 10),
        telephone_domestic_numbers_get('1 800 555 0199 ; 1E 3,4;5'::telephone, 7, 10);
SELECT  telephone_domestic_numbers_get('800 555 0199'::telephone, 1, 10),
        telephone_domestic_numbers_get('800 555 0199'::telephone, 7, 10);
SELECT  telephone_domestic_numbers_get('800 555 019'::telephone, 1, 10),
        telephone_domestic_numbers_get('800 555 019'::telephone, 7, 10);

SELECT  telephone_domestic_numbers_get('800 555 01A9 ; 43,21'::telephone, 1),
        telephone_domestic_numbers_get('800 555 01A9'::telephone, 1);

SELECT  telephone_extension_numbers_get('800 555 01A9 ; 43,21'::telephone),
        telephone_extension_numbers_get('800 555 01A9'::telephone);

SELECT  telephone_domestic_prefer_get('+1 800 555 0199 x 1E 3,4;5'::telephone, 1),
        telephone_domestic_prefer_get('+1 800 555 0199 x 1E 3,4;5'::telephone, 7);
SELECT  telephone_domestic_prefer_get('1 800 555 0199 ; 1E 3,4;5'::telephone, 1),
        telephone_domestic_prefer_get('1 800 555 0199 ; 1E 3,4;5'::telephone, 7);

SELECT  telephone_domestic_assume_set('+1 800 555 0199 x 1E 3,4;5', 1),
        telephone_domestic_assume_set('+1 800 555 0199 x 1E 3,4;5', 7);
SELECT  telephone_domestic_assume_set('800 555 0199 x 1E 3,4;5', 1),
        telephone_domestic_assume_set('800 555 0199 x 1E 3,4;5', 7);
SELECT  telephone_domestic_assume_set('1 800 555 0199 x 1E 3,4;5', 1), -- The calling code validation sees and rejects: +1 180 055 5019
        telephone_domestic_assume_set('1 800 555 0199 x 1E 3,4;5', 7); -- The calling code validation sees and rejects: +7 180 055 5019

SELECT  telephone_set('+1 800 555 0199 x 1E 3,4;5'),
        telephone_set('+1 000 555 0199 x 1E 3,4;5'),
        telephone_set('800 555 0199 x 1E 3,4;5');

SELECT '+1 800 555 019^'::telephone;

SELECT  telephone_geo_parts_get('+1 510 555 0199 x 1E 1,4;5'::telephone, true, true),
        telephone_geo_parts_get('+1 510 555 0199 x 1E 1,4;5'::telephone, false, false);
SELECT  telephone_geo_parts_get('+1 510 555 0199'::telephone, true, true),
        telephone_geo_parts_get('+1 510 555 0199'::telephone, false, false);

SELECT  telephone_geo_parts_get('+1 800 555 0199'::telephone, true, true),
        telephone_geo_parts_get('+1 800 555 0199'::telephone, false, false);

SELECT  telephone_geo_is('+1 800 555 0199'::telephone),
        telephone_geo_is('+1 510 555 0199'::telephone);

SELECT  telephone_geo_parts_get('1234'::telephone, false, false),
        telephone_geo_parts_get('12345'::telephone, false, false);

SELECT  telephone_geo_parts_get('1234'::telephone, true, false),
        telephone_geo_parts_get('12345'::telephone, false, true),
        telephone_geo_parts_get('12345'::telephone, true, true);

SELECT  telephone_geo_parts_get('1234;321'::telephone, false, false),
        telephone_geo_parts_get('12345;321'::telephone, false, false);

SELECT  telephone_geo_parts_get('1234,21'::telephone, false, false),
        telephone_geo_parts_get('12345,321'::telephone, false, false);


CREATE CAST (bytea       AS telephone)   WITHOUT FUNCTION AS ASSIGNMENT;
CREATE CAST (telephone   AS bytea)       WITHOUT FUNCTION;
SELECT '\x599999999990'::bytea::telephone;


/* QUESTIONABLE_BYTEA_API

SELECT  telephone_ident_bytes_get('+1 201 555 0199 x 1E 3,4;56'::telephone),
        telephone_ident_bytes_get('1 201 555 0199 ; 1E 3,4;56'::telephone),
        telephone_ident_bytes_get('1'::telephone),
        telephone_ident_bytes_get('10'::telephone);

QUESTIONABLE_BYTEA_API */
