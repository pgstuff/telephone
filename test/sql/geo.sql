CREATE TABLE telephone_geo_numbers (
    phone_prefix telephone PRIMARY KEY,
    area_name text
);

CREATE TABLE telephone_example_numbers (
    phone_number telephone PRIMARY KEY,
    example_name text
);

INSERT INTO telephone_geo_numbers (phone_prefix, area_name) VALUES ('+1^', 'North American Numbering Plan');
INSERT INTO telephone_geo_numbers (phone_prefix, area_name) VALUES ('+1 510^', 'Oakland');
INSERT INTO telephone_geo_numbers (phone_prefix, area_name) VALUES ('+1 800 555 0199', 'Fictitious Phone Numer');
INSERT INTO telephone_geo_numbers (phone_prefix, area_name) VALUES ('+44^', 'United Kingdom');

INSERT INTO telephone_example_numbers (phone_number, example_name) VALUES ('+1 800 555 0198', 'USA Unlisted Area Code Example');
INSERT INTO telephone_example_numbers (phone_number, example_name) VALUES ('+1 510 555 0199', 'USA Landline Example');
INSERT INTO telephone_example_numbers (phone_number, example_name) VALUES ('+1 800 555 0199', 'USA Toll-free Example');
INSERT INTO telephone_example_numbers (phone_number, example_name) VALUES ('+44 20 7946 0000', 'London Example');

SELECT  DISTINCT ON (telephone_example_numbers.phone_number)
        *
FROM    telephone_example_numbers LEFT OUTER JOIN
        telephone_geo_numbers ON
        telephone_geo_numbers.phone_prefix = ANY(telephone_geo_parts_get(telephone_example_numbers.phone_number, TRUE, TRUE))
ORDER BY telephone_example_numbers.phone_number, LENGTH(telephone_geo_numbers.phone_prefix::text) DESC
