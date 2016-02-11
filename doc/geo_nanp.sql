-- http://geonames.usgs.gov/domestic/download_data.htm
-- Populated Places

-- mv ~/Downloads/POP_PLACES_20151201.zip /tmp
-- chmod o+r /tmp/POP_PLACES_20151201.zip

CREATE UNLOGGED TABLE import_usgs_populated_places (
        FEATURE_ID INT,
        FEATURE_NAME text,
        FEATURE_CLASS text,
        STATE_ALPHA text,
        STATE_NUMERIC text,
        COUNTY_NAME text,
        COUNTY_NUMERIC text,
        PRIMARY_LAT_DMS text,
        PRIM_LONG_DMS text,
        PRIM_LAT_DEC numeric,
        PRIM_LONG_DEC numeric,
        SOURCE_LAT_DMS text,
        SOURCE_LONG_DMS text,
        SOURCE_LAT_DEC numeric,
        SOURCE_LONG_DEC numeric,
        ELEV_IN_M numeric,
        ELEV_IN_FT numeric,
        MAP_NAME text,
        DATE_CREATED date,
        DATE_EDITED date,
        PRIMARY KEY (FEATURE_ID)
);
COPY import_usgs_populated_places FROM PROGRAM 'unzip -p /tmp/POP_PLACES_20151201.zip' WITH DELIMITER AS '|' CSV HEADER;

-- SELECT * FROM import_usgs_populated_places LIMIT 99;

-- SELECT * FROM import_usgs_populated_places WHERE initcap(FEATURE_NAME) != FEATURE_NAME AND FEATURE_NAME NOT LIKE '% (historical)'


-- https://www.nationalnanpa.com/reports/reports_cocodes_assign.html
-- utilized codes (zip)

CREATE UNLOGGED TABLE import_allutlzd (
        "State" text,
        "NPA-NXX" text,
        "OCN" text,
        "Company" text,
        "RateCenter" text,
        "EffectiveDate" text,
        "Use" text,
        "AssignDate" text,
        "Initial/Growth" text,
        "Pooled Code" text,
        PRIMARY KEY ("NPA-NXX")
);

-- mv ~/Downloads/allutlzd.zip /tmp
-- chmod o+r /tmp/allutlzd.zip
COPY import_allutlzd FROM PROGRAM 'unzip -p /tmp/allutlzd.zip' WITH DELIMITER AS E'\t' CSV HEADER;
--COPY import_allutlzd FROM '/tmp/allutlzd.txt' WITH DELIMITER AS E'\t' CSV HEADER

-- SELECT * FROM import_allutlzd LIMIT 99;
-- SELECT * FROM import_allutlzd WHERE "Use" != 'AS'


-- https://www.nationalnanpa.com/reports/reports_npa.html
-- NPA Database

-- mv ~/Downloads/npa_report.csv /tmp
-- chmod o+r /tmp/npa_report.csv

CREATE UNLOGGED TABLE import_npa_all (
        "NPA" text,
        "Type of Code" text,
        "Assignable" text,
        "Explanation" text,
        "Reserved" text,
        "Assigned?" text,
        "Asgt Date" text,
        "Use" text,
        "Location" text,
        "Country" text,
        "In Service?" text,
        "In Svc Date" text,
        "Status" text,
        "PL" text,
        "BLANK" text,
        "Overlay" text,
        "Overlay Complex" text,
        "Parent" text,
        "Service" text,
        "Time Zone" text,
        "BLANK2" text,
        "Map" text,
        "Is NPA in Jeopardy?" text,
        "Is Relief Planning in Progress" text,
        "Home NPA Local Calls" text,
        "Home NPA Toll Calls" text,
        "Foreign NPA Local Calls" text,
        "Foreign NPA Toll Calls" text,
        "perm HNPA local" text,
        "perm HNPA toll" text,
        "perm FNPA local" text,
        "dp Notes" text,
        PRIMARY KEY ("NPA")
);
COPY import_npa_all FROM '/tmp/npa_report.csv' WITH DELIMITER AS ',' CSV HEADER;

CREATE EXTENSION telephone;

CREATE TABLE telephone_prefixes_nanp (
        telephone_prefix telephone NOT NULL,
        country text,
        state_code text,
        location text,
        rate_center text,
        city_name text,
        county_name text,
        time_zone_ids text[],
        geo_lat numeric,
        geo_long numeric,
        PRIMARY KEY (telephone_prefix)
);

INSERT INTO telephone_prefixes_nanp (
        telephone_prefix, country, state_code, location, time_zone_ids)
SELECT  ('+1 ' || "NPA" || '^')::telephone,
        TRIM("Country") AS country,
        CASE
                WHEN    TRIM("Country") = 'US'
                        THEN    REPLACE(REPLACE(TRIM("Location"), 'PUERTO RICO','PR'), 'USVI','VI')
                ELSE    NULL
        END AS state_code,
        CASE
                WHEN    TRIM("Country") != 'US'
                        THEN    TRIM("Location")
                ELSE    NULL
        END AS location,
        CASE
                WHEN TRIM("Time Zone") = 'E' THEN ARRAY['America/New_York']
                WHEN TRIM("Time Zone") = 'C' THEN ARRAY['America/Chicago']
                WHEN TRIM("Time Zone") = 'P' THEN ARRAY['America/Los_Angeles']
                WHEN TRIM("Time Zone") = 'M' THEN ARRAY['America/Denver']
                WHEN TRIM("Time Zone") = 'A' THEN
                        CASE
                                WHEN TRIM("Location") = 'PUERTO RICO' THEN ARRAY['America/Puerto_Rico']
                                WHEN TRIM("Location") = 'SINT MAARTEN' THEN ARRAY['America/Puerto_Rico']
                                WHEN TRIM("Location") = 'ST. LUCIA' THEN ARRAY['America/Puerto_Rico']
                                WHEN TRIM("Location") = 'DOMINICA' THEN ARRAY['America/Puerto_Rico']
                                WHEN TRIM("Location") = 'ST. VINCENT & GRENADINES' THEN ARRAY['America/Puerto_Rico']
                                WHEN TRIM("Location") = 'ST. KITTS & NEVIS' THEN ARRAY['America/Puerto_Rico']
                                WHEN TRIM("Location") = 'TRINIDAD & TOBAGO' THEN ARRAY['America/Puerto_Rico']
                                WHEN TRIM("Location") = 'ANGUILLA' THEN ARRAY['America/Puerto_Rico']
                                WHEN TRIM("Location") = 'ANTIGUA/BARBUDA' THEN ARRAY['America/Puerto_Rico']
                                WHEN TRIM("Location") = 'GRENADA' THEN ARRAY['America/Puerto_Rico']
                                WHEN TRIM("Location") = 'MONTSERRAT' THEN ARRAY['America/Puerto_Rico']
                                WHEN TRIM("Location") = 'BRITISH VIRGIN ISLANDS' THEN ARRAY['America/Puerto_Rico']
                                WHEN TRIM("Location") = 'USVI' THEN ARRAY['America/Puerto_Rico']
                                WHEN TRIM("Location") = 'BARBADOS' THEN ARRAY['America/Barbados']
                                WHEN TRIM("Location") = 'BERMUDA' THEN ARRAY['Atlantic/Bermuda']
                                WHEN TRIM("Location") = 'NEW BRUNSWICK' THEN ARRAY['America/Moncton']
                                WHEN TRIM("Location") = 'NOVA SCOTIA - PRINCE EDWARD ISLAND' THEN ARRAY['America/Glace_Bay']
                                ELSE ARRAY[TRIM("Time Zone")]
                        END
                WHEN TRIM("Time Zone") = '(UTC+10)' THEN
                        CASE
                                WHEN TRIM("Location") = 'GU' THEN ARRAY['Pacific/Guam']
                                WHEN "NPA" = '670' THEN ARRAY['Pacific/Guam']
                                ELSE ARRAY[TRIM("Time Zone")]
                        END
                WHEN TRIM("Time Zone") = '(UTC-10)' THEN
                        CASE
                                WHEN TRIM("Location") = 'HI' THEN ARRAY['Pacific/Honolulu'] -- No DST
                                ELSE ARRAY[TRIM("Time Zone")]
                        END
                WHEN TRIM("Time Zone") = '(UTC-9)' THEN
                        CASE
                                WHEN TRIM("Location") = 'AK' THEN ARRAY['America/Anchorage']
                                ELSE ARRAY[TRIM("Time Zone")]
                        END
                WHEN TRIM("Time Zone") = 'NA' THEN
                        CASE
                                WHEN TRIM("Location") = 'NEWFOUNDLAND' THEN ARRAY['America/St_Johns']
                                ELSE ARRAY[TRIM("Time Zone")]
                        END
                WHEN LENGTH(TRIM("Time Zone")) <= 3 THEN
                        array_replace(
                        array_replace(
                        array_replace(
                        array_replace(
                        string_to_array(TRIM("Time Zone"), NULL)
                        ,'E','America/New_York')
                        ,'C','America/Chicago')
                        ,'P','America/Los_Angeles')
                        ,'M','America/Denver')
                ELSE ARRAY[TRIM("Time Zone")]
        END AS time_zone_ids
FROM    import_npa_all
WHERE   TRIM("Assigned?") = 'Yes' AND
        TRIM("In Service?") = 'Y';

INSERT INTO telephone_prefixes_nanp (
        telephone_prefix, state_code, rate_center)
SELECT  ('+1 ' || "NPA-NXX" || '^')::telephone,
        TRIM("State"),
        TRIM("RateCenter")
FROM    import_allutlzd;

-- SELECT * FROM telephone_prefixes_nanp LIMIT 99

UPDATE  telephone_prefixes_nanp
SET     city_name   = import_usgs_populated_places.feature_name,
        county_name = import_usgs_populated_places.county_name,
        geo_lat     = import_usgs_populated_places.prim_lat_dec,
        geo_long    = import_usgs_populated_places.prim_long_dec,
        country     = 'US'
FROM    import_usgs_populated_places
WHERE   REPLACE(UPPER(import_usgs_populated_places.feature_name),' ','') = REPLACE(telephone_prefixes_nanp.rate_center,' ','') AND
        import_usgs_populated_places.state_alpha = telephone_prefixes_nanp.state_code AND
        telephone_prefixes_nanp.county_name IS NULL;

UPDATE  telephone_prefixes_nanp
SET     city_name   = import_usgs_populated_places.feature_name,
        county_name = import_usgs_populated_places.county_name,
        geo_lat     = import_usgs_populated_places.prim_lat_dec,
        geo_long    = import_usgs_populated_places.prim_long_dec,
        country     = 'US'
FROM    import_usgs_populated_places
WHERE   REPLACE(UPPER(import_usgs_populated_places.feature_name),' ','') = REPLACE(
        regexp_replace(
        regexp_replace(
        regexp_replace(telephone_prefixes_nanp.rate_center
        ,' NE$','')
        ,' SO$','')
        ,' NW$',''),' ','') AND
        import_usgs_populated_places.state_alpha = telephone_prefixes_nanp.state_code AND
        telephone_prefixes_nanp.county_name IS NULL;

CREATE EXTENSION unaccent;

UPDATE  telephone_prefixes_nanp
SET     city_name   = import_usgs_populated_places.feature_name,
        county_name = import_usgs_populated_places.county_name,
        geo_lat     = import_usgs_populated_places.prim_lat_dec,
        geo_long    = import_usgs_populated_places.prim_long_dec,
        country     = 'US'
FROM    import_usgs_populated_places
WHERE   UPPER(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(unaccent(import_usgs_populated_places.feature_name)
        ,' Bay',' BY')
        ,' Beach',' BCH')
        ,' Bluffs',' BLF')
        ,' Bridge',' BDG')
        ,' Branch',' BRCH')
        ,' Butte',' BTE')
        ,' Creek',' CRK')
        ,' Cove',' CV')
        ,' City',' CY')
        ,' Center',' CTR')
        ,' Corner',' COR')
        ,' Falls',' FLS')
        ,' Forest',' FOR')
        ,'field',' FLD')
        ,' Grove',' GRV')
        ,' Haven',' HVN')
        ,' Heights',' HTS')
        ,' Harbor',' HBR')
        ,' House',' HS')
        ,' Island',' IS')
        ,' Isle',' IS')
        ,' Inlet',' INLT')
        ,' Junction',' JCT')
        ,' Lakes',' LAKE')
        ,' Landing',' LDG')
        ,' Lake',' LK')
        ,' Lagoon',' LGON')
        ,' Meadow',' MDW')
        ,' Meadows',' MDW')
        ,' Mountain',' MTN')
        ,' Mills',' ML')
        ,' Orchard',' ORCH')
        ,' Park',' PK')
        ,' Plains',' PL')
        ,' Point',' PT')
        ,' Springs',' SPG')
        ,' Station',' STATN')
        ,' Square',' SQ')
        ,' Ridge',' RDG')
        ,' Rapids',' RPDS')
        ,' Rock',' RK')
        ,' Rivers',' RVS')
        ,' River',' RIV')
        ,' Valley',' VLY')
        ,' Village',' VLG')
        ,' View',' VW')
        ,'ville',' VL')
        ,' Woods',' WD')
        ,' ','')) =
        REPLACE(
        regexp_replace(
        regexp_replace(
        regexp_replace(telephone_prefixes_nanp.rate_center
        ,' NE$','')
        ,' SO$','')
        ,' NW$',''),' ','') AND
        import_usgs_populated_places.state_alpha = telephone_prefixes_nanp.state_code AND
        telephone_prefixes_nanp.county_name IS NULL;

UPDATE  telephone_prefixes_nanp
SET     city_name   = import_usgs_populated_places.feature_name,
        county_name = import_usgs_populated_places.county_name,
        geo_lat     = import_usgs_populated_places.prim_lat_dec,
        geo_long    = import_usgs_populated_places.prim_long_dec,
        country     = 'US'
FROM    import_usgs_populated_places
WHERE   UPPER(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(unaccent(import_usgs_populated_places.feature_name)
        ,'brook',' BRK')
        ,'land',' LD')
        ,'ford',' FD')
        ,'port',' PT')
        ,'burg',' BG')
        ,' Bluff',' BLF')
        ,' Downs',' DWN')
        ,' Fort',' FT')
        ,' Spring',' SPG')
        ,' Rapids',' RPD')
        ,' Meadow',' MW')
        ,' Mountain',' MT')
        ,' Mill',' ML')
        ,'mouth',' MTH')
        ,' Station',' STA')
        ,' Town',' TN')
        ,' Plain',' PL')
        ,' Hill',' HL')
        ,' Valley',' VLLY')
        ,' Valley Junction',' VLY')
        ,' West',' W')
        ,' ','')) =
        REPLACE(
        regexp_replace(
        regexp_replace(
        regexp_replace(telephone_prefixes_nanp.rate_center
        ,' NE$','')
        ,' SO$','')
        ,' NW$',''),' ','') AND
        import_usgs_populated_places.state_alpha = telephone_prefixes_nanp.state_code AND
        telephone_prefixes_nanp.county_name IS NULL;

UPDATE  telephone_prefixes_nanp
SET     city_name   = import_usgs_populated_places.feature_name,
        county_name = import_usgs_populated_places.county_name,
        geo_lat     = import_usgs_populated_places.prim_lat_dec,
        geo_long    = import_usgs_populated_places.prim_long_dec,
        country     = 'US'
FROM    import_usgs_populated_places
WHERE   UPPER(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(
        REPLACE(unaccent(import_usgs_populated_places.feature_name)
        ,'brook',' BRK')
        ,'land',' LD')
        ,'ford',' FD')
        ,'port',' PT')
        ,'burg',' BG')
        ,'mouth',' MTH')
        ,'field',' FLD')
        ,'stone',' STN')
        ,'ville',' VL')
        ,'wood',' WD')
        ,'water',' WTR')
        ,'ton',' TN')
        ,' Bluff',' BLF')
        ,' Downs',' DWN')
        ,' Fort',' FT')
        ,' Spring',' SPG')
        ,' Rapids',' RPD')
        ,' Meadow',' MW')
        ,' Mountain',' MT')
        ,' Mill',' ML')
        ,' Station',' STA')
        ,' Town',' TN')
        ,' Plain',' PL')
        ,' Hill',' HL')
        ,' Valley',' VLLY')
        ,' Valley Junction',' VLY')
        ,' West',' W')
        ,' Bay',' BY')
        ,' Beach',' BCH')
        ,' Bluffs',' BLF')
        ,' Bridge',' BDG')
        ,' Branch',' BRCH')
        ,' Butte',' BTE')
        ,' Creek',' CRK')
        ,' Cove',' CV')
        ,' City',' CY')
        ,' Center',' CTR')
        ,' Corner',' COR')
        ,' Falls',' FLS')
        ,' Forest',' FOR')
        ,' Grove',' GRV')
        ,' Haven',' HVN')
        ,' Heights',' HTS')
        ,' Harbor',' HBR')
        ,' House',' HS')
        ,' Island',' IS')
        ,' Isle',' IS')
        ,' Inlet',' INLT')
        ,' Junction',' JCT')
        ,' Lakes',' LAKE')
        ,' Landing',' LDG')
        ,' Lake',' LK')
        ,' Lagoon',' LGON')
        ,' Meadow',' MDW')
        ,' Meadows',' MDW')
        ,' Mountain',' MTN')
        ,' Mills',' ML')
        ,' Orchard',' ORCH')
        ,' Park',' PK')
        ,' Plains',' PL')
        ,' Point',' PT')
        ,' Springs',' SPG')
        ,' Station',' STATN')
        ,' Square',' SQ')
        ,' Ridge',' RDG')
        ,' Rapids',' RPDS')
        ,' Rock',' RK')
        ,' Rivers',' RVS')
        ,' River',' RIV')
        ,' Valley',' VLY')
        ,' Village',' VLG')
        ,' View',' VW')
        ,' Woods',' WD')
        ,' ','')) =
        REPLACE(
        regexp_replace(
        regexp_replace(
        regexp_replace(telephone_prefixes_nanp.rate_center
        ,' NE$','')
        ,' SO$','')
        ,' NW$',''),' ','') AND
        import_usgs_populated_places.state_alpha = telephone_prefixes_nanp.state_code AND
        telephone_prefixes_nanp.county_name IS NULL;

-- TODO:  Add non-US NANP logic.

SELECT * FROM  telephone_prefixes_nanp, import_usgs_populated_places
WHERE   substring(REPLACE(UPPER(import_usgs_populated_places.feature_name),' ','') from 1 for 6) =
        substring(REPLACE(telephone_prefixes_nanp.rate_center,' ','') from 1 for 6) AND
        --telephone_prefixes_nanp.rate_center LIKE '% %' AND
        import_usgs_populated_places.state_alpha = telephone_prefixes_nanp.state_code AND
        telephone_prefixes_nanp.county_name IS NULL;

UPDATE  telephone_prefixes_nanp
SET     geo_lat = NULL,
        geo_long = NULL
WHERE   geo_long = 0;

/*
sudo yum install postgis2_95 postgis2_95-client

http://efele.net/maps/tz/world/
tz_world

cd /tmp
unzip ~/Downloads/tz_world.zip
cd world
shp2pgsql -D tz_world.shp > tz_world.sql
*/

CREATE EXTENSION postgis;

-- This will create & load the tz_world table (change the database name):
-- psql -d tel_geo -f tz_world.sql

-- This will take about 20 minutes using 2014 hardware.
WITH unique_loc AS (
        SELECT  DISTINCT geo_lat, geo_long
        FROM    telephone_prefixes_nanp
        WHERE   geo_lat IS NOT NULL AND
                time_zone_ids IS NULL
), unique_loc_tzid AS (
        SELECT  unique_loc.geo_long,
                unique_loc.geo_lat,
                (
                        SELECT  ARRAY[tz_world.tzid]
                        FROM    tz_world
                        WHERE   ST_Contains(tz_world.geom, ST_MakePoint(unique_loc.geo_long, unique_loc.geo_lat))
                ) AS time_zone_ids
        FROM    unique_loc
)
UPDATE  telephone_prefixes_nanp
SET     time_zone_ids = unique_loc_tzid.time_zone_ids
FROM    unique_loc_tzid
WHERE   telephone_prefixes_nanp.geo_long IS NOT NULL AND
        telephone_prefixes_nanp.geo_long = unique_loc_tzid.geo_long AND
        telephone_prefixes_nanp.geo_lat  = unique_loc_tzid.geo_lat AND
        telephone_prefixes_nanp.time_zone_ids IS NULL;

-- SELECT time_zone_ids, COUNT(*) FROM telephone_prefixes_nanp WHERE state_code = 'CA' GROUP BY time_zone_ids ORDER BY 2 DESC;

/*
-- Skip if you are going to use geo_earth.sql.

UPDATE  telephone_prefixes_nanp
SET     time_zone_ids = ARRAY['America/Los_Angeles'] -- Pacific
WHERE   time_zone_ids IS NULL AND
        state_code IN ('CA','NV','WA');

UPDATE  telephone_prefixes_nanp
SET     time_zone_ids = ARRAY['America/Denver'] -- Mountain
WHERE   time_zone_ids IS NULL AND
        state_code IN ('CO','MT','NM','UT','WY');

UPDATE  telephone_prefixes_nanp
SET     time_zone_ids = ARRAY['America/Chicago'] -- Central
WHERE   time_zone_ids IS NULL AND
        state_code IN ('AL','AR','IA','IL','LA','MN','MO','MS','OK','WI');

UPDATE  telephone_prefixes_nanp
SET     time_zone_ids = ARRAY['America/New_York'] -- Eastern
WHERE   time_zone_ids IS NULL AND
        state_code IN ('CT','DC','DE','GA','MA','MD','ME','NC','NH','NJ','NY','OH','PA','RI','SC','VA','VT','WV');

UPDATE  telephone_prefixes_nanp
SET     time_zone_ids = ARRAY['America/Anchorage','America/Adak']
WHERE   time_zone_ids IS NULL AND
        state_code IN ('AK');

UPDATE  telephone_prefixes_nanp
SET     time_zone_ids = ARRAY['America/Los_Angeles','America/Denver']
WHERE   time_zone_ids IS NULL AND
        state_code IN ('ID','NV','OR');

UPDATE  telephone_prefixes_nanp
SET     time_zone_ids = ARRAY['America/Denver','America/Chicago']
WHERE   time_zone_ids IS NULL AND
        state_code IN ('KS','NE','ND','TX');

UPDATE  telephone_prefixes_nanp
SET     time_zone_ids = ARRAY['America/Chicago','America/New_York']
WHERE   time_zone_ids IS NULL AND
        state_code IN ('FL','IN','KY','MI','TN');

UPDATE  telephone_prefixes_nanp
SET     time_zone_ids = ARRAY['America/Denver','America/Phoenix']
WHERE   time_zone_ids IS NULL AND
        state_code IN ('AZ');

UPDATE  telephone_prefixes_nanp
SET     time_zone_ids = ARRAY['Pacific/Honolulu']
WHERE   time_zone_ids IS NULL AND
        state_code IN ('HI');

UPDATE  telephone_prefixes_nanp
SET     time_zone_ids = ARRAY['America/Puerto_Rico']
WHERE   time_zone_ids IS NULL AND
        state_code IN ('PR');
*/

UPDATE  telephone_prefixes_nanp
SET     country     = 'US'
WHERE   country IS NULL AND
        state_code IS NOT NULL;

-- SELECT * FROM telephone_prefixes_nanp WHERE telephone_prefix = '+1 209 847^';

-- SELECT time_zone_ids, COUNT(*) FROM telephone_prefixes_nanp GROUP BY time_zone_ids ORDER BY 2 DESC;

-- SELECT * FROM telephone_prefixes_nanp WHERE time_zone_ids = ARRAY['A'];

-- SELECT * FROM telephone_prefixes_nanp WHERE time_zone_ids = ARRAY['America/Adak'];

-- SELECT * FROM telephone_prefixes_nanp WHERE time_zone_ids IS NULL;

SELECT  tzid
FROM    tz_world
WHERE   ST_Contains(geom, ST_MakePoint(-82.2942613, 26.8503433));

--SELECT  tzid
--FROM    tz_world_mp
--WHERE   ST_Contains(geom, ST_MakePoint(-82.2942613, 26.8503433));

SELECT  *
FROM    telephone_prefixes_nanp
WHERE   telephone_prefix = ANY(telephone_geo_parts_get('+1 (510) 555-0000', FALSE, FALSE)) AND
        time_zone_ids IS NOT NULL
ORDER BY LENGTH(telephone_prefix::text) DESC
LIMIT 1;

TRUNCATE import_usgs_populated_places, import_allutlzd, import_npa_all;
