
SELECT '\x599999999990'::bytea::telephone;

SELECT '+1 210 555 4567'::telephone::bytea;

SELECT '\x565499989ab0'::bytea::telephone;

SELECT '\x555599989ab0'::bytea::telephone;




SELECT  telephone_service_get('+1-805-MESSAGE x3'::telephone);




SELECT  telephone_geo_parts_get('+91 124 00000000000000'::telephone, false, false);

SELECT  telephone_geo_parts_get('+44 20 7946 0000 21'::telephone, false, false);

SELECT  telephone_geo_parts_get('+61 2 8123 4567 21'::telephone, false, false);

SELECT  telephone_geo_parts_get('+7 388 123 4567 123'::telephone, false, false);

SELECT  telephone_geo_parts_get('+81 1378 67890 123'::telephone, false, false);

SELECT  telephone_geo_parts_get('+91 370 123 1234 123'::telephone, false, false);

SELECT '+61 2 8123 4567'::telephone;

SELECT '+7 388 123 4567 123'::telephone;



SELECT '+44 800 046 1688'::telephone;


SELECT '+44 800 1111'::telephone;
SELECT '+44 845 4647'::telephone;


SELECT '+33 9 72 17 01 31 1'::telephone;

SELECT '+334000000001'::telephone;

SELECT '+33 5 00 00 00 00 1'::telephone;

SELECT  telephone_geo_parts_get('+33 5 00 00 00 00 123'::telephone, false, false);
