-- Run geo_nanp.sql without state_code based time_zone_ids (in comments)

CREATE EXTENSION hstore;

/*
DROP TABLE telephone_prefixes;
DROP TABLE localities;
DROP TABLE sub_administrative_areas;
DROP TABLE administrative_areas;
DROP TABLE countries;

DROP TABLE locality_ids_tmp;
DROP TABLE sub_administrative_area_ids_tmp;
DROP TABLE administrative_area_ids_tmp;
*/

CREATE TABLE countries
(
  iso_3166_1_code character(2),
  country_name hstore NOT NULL,
  time_zone_ids text[],
  geo_lat numeric,
  geo_long numeric,
  PRIMARY KEY(iso_3166_1_code)
);

CREATE TABLE administrative_areas
(
  administrative_area_id serial NOT NULL,
  iso_3166_1_code character(2) NOT NULL,
  iso_3166_2_code varchar(6),
  postal_admin_area_code text,
  administrative_area_name hstore,
  time_zone_ids text[],
  geo_lat numeric,
  geo_long numeric,
  PRIMARY KEY(administrative_area_id)
);
ALTER TABLE administrative_areas
  ADD FOREIGN KEY (iso_3166_1_code) REFERENCES countries (iso_3166_1_code) ON UPDATE RESTRICT ON DELETE RESTRICT;

CREATE TABLE sub_administrative_areas
(
  sub_administrative_area_id serial NOT NULL,
  administrative_area_id INT NOT NULL,
  sub_administrative_area_name hstore,
  time_zone_ids text[],
  geo_lat numeric,
  geo_long numeric,
  PRIMARY KEY(sub_administrative_area_id)
);
ALTER TABLE sub_administrative_areas
  ADD FOREIGN KEY (administrative_area_id) REFERENCES administrative_areas (administrative_area_id) ON UPDATE RESTRICT ON DELETE RESTRICT;


CREATE TABLE localities
(
  locality_id serial NOT NULL,
  administrative_area_id integer NOT NULL,
  sub_administrative_area_id integer NOT NULL,
  locality_name hstore,
  time_zone_ids text[],
  geo_lat numeric,
  geo_long numeric,
  PRIMARY KEY (locality_id)
);
ALTER TABLE localities
  ADD FOREIGN KEY (administrative_area_id) REFERENCES administrative_areas (administrative_area_id) ON UPDATE RESTRICT ON DELETE RESTRICT;
ALTER TABLE localities
  ADD FOREIGN KEY (sub_administrative_area_id) REFERENCES sub_administrative_areas (sub_administrative_area_id) ON UPDATE RESTRICT ON DELETE RESTRICT;

CREATE TABLE telephone_prefixes
(
  telephone_prefix telephone NOT NULL,
  rate_center text,
  iso_3166_1_codes char(2)[],
  administrative_area_ids integer[],
  sub_administrative_area_ids integer[],
  locality_ids integer[],
  PRIMARY KEY (telephone_prefix)
);

-- SELECT * FROM telephone_prefixes_nanp LIMIT 999;

INSERT INTO countries (
        iso_3166_1_code,
        country_name)
SELECT  telephone_prefixes_nanp.country,
        hstore('en_US',
                CASE
                        WHEN country = 'US' THEN 'United States of America'
                END
        )
FROM    telephone_prefixes_nanp
WHERE   country = 'US'
LIMIT   1;

-- TODO:  Add non-US NANP logic.

WITH administrative_area_ids_cte AS (
        INSERT INTO administrative_areas (
                iso_3166_1_code,
                postal_admin_area_code)
        SELECT  DISTINCT
                country,
                state_code
        FROM    telephone_prefixes_nanp
        WHERE   country = 'US' AND
                state_code IS NOT NULL
        RETURNING administrative_area_id, iso_3166_1_code, postal_admin_area_code
)
SELECT  *
INTO    TEMPORARY TABLE administrative_area_ids_tmp
FROM    administrative_area_ids_cte;

UPDATE  administrative_areas
SET     time_zone_ids = ARRAY['America/Los_Angeles'] -- Pacific
WHERE   iso_3166_1_code = 'US' AND
        postal_admin_area_code IN ('CA','NV','WA');

UPDATE  administrative_areas
SET     time_zone_ids = ARRAY['America/Denver'] -- Mountain
WHERE   iso_3166_1_code = 'US' AND
        postal_admin_area_code IN ('CO','MT','NM','UT','WY');

UPDATE  administrative_areas
SET     time_zone_ids = ARRAY['America/Chicago'] -- Central
WHERE   iso_3166_1_code = 'US' AND
        postal_admin_area_code IN ('AL','AR','IA','IL','LA','MN','MO','MS','OK','WI');

UPDATE  administrative_areas
SET     time_zone_ids = ARRAY['America/New_York'] -- Eastern
WHERE   iso_3166_1_code = 'US' AND
        postal_admin_area_code IN ('CT','DC','DE','GA','MA','MD','ME','NC','NH','NJ','NY','OH','PA','RI','SC','VA','VT','WV');

UPDATE  administrative_areas
SET     time_zone_ids = ARRAY['America/Anchorage','America/Adak']
WHERE   iso_3166_1_code = 'US' AND
        postal_admin_area_code IN ('AK');

UPDATE  administrative_areas
SET     time_zone_ids = ARRAY['America/Los_Angeles','America/Denver']
WHERE   iso_3166_1_code = 'US' AND
        postal_admin_area_code IN ('ID','NV','OR');

UPDATE  administrative_areas
SET     time_zone_ids = ARRAY['America/Denver','America/Chicago']
WHERE   iso_3166_1_code = 'US' AND
        postal_admin_area_code IN ('KS','NE','ND','TX');

UPDATE  administrative_areas
SET     time_zone_ids = ARRAY['America/Chicago','America/New_York']
WHERE   iso_3166_1_code = 'US' AND
        postal_admin_area_code IN ('FL','IN','KY','MI','TN');

UPDATE  administrative_areas
SET     time_zone_ids = ARRAY['America/Denver','America/Phoenix']
WHERE   iso_3166_1_code = 'US' AND
        postal_admin_area_code IN ('AZ');

UPDATE  administrative_areas
SET     time_zone_ids = ARRAY['Pacific/Honolulu']
WHERE   iso_3166_1_code = 'US' AND
        postal_admin_area_code IN ('HI');

UPDATE  administrative_areas
SET     time_zone_ids = ARRAY['America/Puerto_Rico']
WHERE   iso_3166_1_code = 'US' AND
        postal_admin_area_code IN ('PR');

WITH sub_administrative_area_ids_cte AS  (
        INSERT INTO sub_administrative_areas (
                administrative_area_id,
                sub_administrative_area_name)
        SELECT  DISTINCT
                administrative_area_ids_tmp.administrative_area_id,
                hstore('en_US', telephone_prefixes_nanp.county_name)
        FROM    telephone_prefixes_nanp JOIN
                administrative_area_ids_tmp ON
                        administrative_area_ids_tmp.iso_3166_1_code = telephone_prefixes_nanp.country AND
                        administrative_area_ids_tmp.postal_admin_area_code = telephone_prefixes_nanp.state_code
        WHERE   telephone_prefixes_nanp.country = 'US' AND
                telephone_prefixes_nanp.state_code IS NOT NULL AND
                telephone_prefixes_nanp.county_name IS NOT NULL
        RETURNING
                sub_administrative_area_id, administrative_area_id,
                sub_administrative_area_name->'en_US' AS sub_administrative_area_name_en_us
)
SELECT  *
INTO    TEMPORARY TABLE sub_administrative_area_ids_tmp
FROM    sub_administrative_area_ids_cte;

WITH locality_ids_cte AS (
        INSERT INTO localities (
                administrative_area_id,
                sub_administrative_area_id,
                locality_name,
                time_zone_ids,
                geo_lat,
                geo_long)
        SELECT  DISTINCT
                administrative_area_ids_tmp.administrative_area_id,
                sub_administrative_area_ids_tmp.sub_administrative_area_id,
                hstore('en_US', telephone_prefixes_nanp.city_name),
                telephone_prefixes_nanp.time_zone_ids,
                telephone_prefixes_nanp.geo_lat,
                telephone_prefixes_nanp.geo_long
        FROM    telephone_prefixes_nanp JOIN
                administrative_area_ids_tmp ON
                        administrative_area_ids_tmp.iso_3166_1_code = telephone_prefixes_nanp.country AND
                        administrative_area_ids_tmp.postal_admin_area_code = telephone_prefixes_nanp.state_code JOIN
                sub_administrative_area_ids_tmp ON
                        sub_administrative_area_ids_tmp.administrative_area_id = administrative_area_ids_tmp.administrative_area_id AND
                        sub_administrative_area_ids_tmp.sub_administrative_area_name_en_us = telephone_prefixes_nanp.county_name
        WHERE   telephone_prefixes_nanp.country = 'US' AND
                telephone_prefixes_nanp.state_code IS NOT NULL AND
                telephone_prefixes_nanp.city_name IS NOT NULL
        UNION ALL
        SELECT  DISTINCT
                administrative_area_ids_tmp.administrative_area_id,
                NULL::INT,
                hstore('en_US', telephone_prefixes_nanp.city_name),
                telephone_prefixes_nanp.time_zone_ids,
                telephone_prefixes_nanp.geo_lat,
                telephone_prefixes_nanp.geo_long
        FROM    telephone_prefixes_nanp JOIN
                administrative_area_ids_tmp ON
                        administrative_area_ids_tmp.iso_3166_1_code = telephone_prefixes_nanp.country AND
                        administrative_area_ids_tmp.postal_admin_area_code = telephone_prefixes_nanp.state_code
        WHERE   telephone_prefixes_nanp.country = 'US' AND
                telephone_prefixes_nanp.state_code IS NOT NULL AND
                telephone_prefixes_nanp.city_name IS NOT NULL AND
                telephone_prefixes_nanp.county_name IS NULL
        RETURNING
                locality_id, administrative_area_id, sub_administrative_area_id,
                locality_name->'en_US' AS locality_name_en_us
)
SELECT  *
INTO    TEMPORARY TABLE locality_ids_tmp
FROM    locality_ids_cte;

INSERT INTO telephone_prefixes (
        telephone_prefix,
        rate_center,
        iso_3166_1_codes,
        administrative_area_ids,
        sub_administrative_area_ids,
        locality_ids)
SELECT  telephone_prefixes_nanp.telephone_prefix,
        telephone_prefixes_nanp.rate_center,
        ARRAY['US'],
        ARRAY[administrative_area_ids_tmp.administrative_area_id],
        ARRAY[sub_administrative_area_ids_tmp.sub_administrative_area_id],
        ARRAY[locality_ids_tmp.locality_id]
FROM    telephone_prefixes_nanp
        LEFT OUTER JOIN administrative_area_ids_tmp ON
                administrative_area_ids_tmp.iso_3166_1_code = telephone_prefixes_nanp.country AND
                administrative_area_ids_tmp.postal_admin_area_code = telephone_prefixes_nanp.state_code
        LEFT OUTER JOIN sub_administrative_area_ids_tmp ON
                sub_administrative_area_ids_tmp.administrative_area_id = administrative_area_ids_tmp.administrative_area_id AND
                sub_administrative_area_ids_tmp.sub_administrative_area_name_en_us = telephone_prefixes_nanp.county_name
        LEFT OUTER JOIN locality_ids_tmp ON
                locality_ids_tmp.administrative_area_id = administrative_area_ids_tmp.administrative_area_id AND
                locality_ids_tmp.locality_name_en_us = telephone_prefixes_nanp.city_name AND
                (       locality_ids_tmp.sub_administrative_area_id IS NULL OR
                        locality_ids_tmp.sub_administrative_area_id = sub_administrative_area_ids_tmp.sub_administrative_area_id);

-- SELECT * FROM telephone_prefixes_nanp WHERE city_name = 'Danville' AND state_code = 'OH'

-- SELECT * FROM telephone_prefixes LIMIT 999;

INSERT INTO countries (
        iso_3166_1_code,
        country_name,
        time_zone_ids)
VALUES  ('GB',
        hstore('en_US','United Kingdom of Great Britain and Northern Ireland'),
        ARRAY['Europe/London']),
        ('IT',
        'en_US=>Italy,it_IT=>Italia'::hstore,
        ARRAY['Europe/Rome']),
        ('JP',
        'en_US=>Japan,ja_JP=>にっぽん'::hstore,
        ARRAY['Asia/Tokyo']);

INSERT INTO telephone_prefixes (
        telephone_prefix,       iso_3166_1_codes)
VALUES  ('+44^',                ARRAY['GB']),
        ('+39^',                ARRAY['IT']),
        ('+81^',                ARRAY['JP']);

SELECT  countries.country_name->'en_US' AS country_name,
        administrative_areas.postal_admin_area_code,
        sub_administrative_areas.sub_administrative_area_name->'en_US' AS sub_administrative_area_name,
        localities.locality_name->'en_US' AS locality_name,
        telephone_prefixes.rate_center,
        COALESCE(
                localities.locality_name->'en_US' ||
                        COALESCE(', ' || (sub_administrative_areas.sub_administrative_area_name->'en_US')) ||
                        COALESCE(', ' || administrative_areas.postal_admin_area_code) ||
                        COALESCE(', ' || (countries.country_name->'en_US')),
                sub_administrative_areas.sub_administrative_area_name->'en_US' ||
                        COALESCE(', ' || administrative_areas.postal_admin_area_code) ||
                        COALESCE(', ' || (countries.country_name->'en_US')),
                administrative_areas.postal_admin_area_code ||
                        COALESCE(', ' || (countries.country_name->'en_US')),
                telephone_prefixes.rate_center || ' in ' || administrative_areas.postal_admin_area_code ||
                        COALESCE(', ' || (countries.country_name->'en_US')),
                telephone_prefixes.rate_center || ' in ' || (countries.country_name->'en_US'),
                countries.country_name->'en_US',
                telephone_prefixes.rate_center) AS most_local_name,
        COALESCE(
                sub_administrative_areas.time_zone_ids,
                administrative_areas.time_zone_ids,
                localities.time_zone_ids,
                countries.time_zone_ids) AS time_zone_ids
FROM    telephone_prefixes
        LEFT OUTER JOIN countries ON
                countries.iso_3166_1_code = ANY (telephone_prefixes.iso_3166_1_codes)
        LEFT OUTER JOIN localities ON
                localities.locality_id = ANY (telephone_prefixes.locality_ids)
        LEFT OUTER JOIN administrative_areas ON
                administrative_areas.administrative_area_id = ANY (telephone_prefixes.administrative_area_ids)
        LEFT OUTER JOIN sub_administrative_areas ON
                sub_administrative_areas.sub_administrative_area_id = ANY (telephone_prefixes.sub_administrative_area_ids)
WHERE   --telephone_prefixes.telephone_prefix = '+1 209 847^'
        telephone_prefixes.telephone_prefix = ANY(telephone_geo_parts_get('+44 1632 960123 x 1E 3,4;5', TRUE, TRUE))
ORDER BY LENGTH(telephone_prefix::text) DESC
LIMIT 1;

-- SELECT * FROM countries;

-- SELECT * FROM administrative_areas;
