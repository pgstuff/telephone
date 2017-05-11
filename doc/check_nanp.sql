-- https://www.nationalnanpa.com/reports/reports_cocodes_assign.html
-- All States / utilized codes (zip)

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

SELECT  ('+1 ' || "NPA" || '^')::telephone
FROM    import_npa_all
WHERE   TRIM("Assigned?") = 'Yes' AND
        TRIM("In Service?") = 'Y';

SELECT  ('+1 ' || "NPA-NXX" || '^')::telephone
FROM    import_allutlzd;
