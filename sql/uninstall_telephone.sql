/*
 * Author: The maintainer's name
 * Created at: Wed Oct 21 12:43:52 -0400 2015
 *
 */

--
-- This is a example code genereted automaticaly
-- by pgxn-utils.

SET client_min_messages = warning;

BEGIN;

-- You can use this statements as
-- template for your extension.

DROP OPERATOR #? (text, text);
DROP FUNCTION telephone(text, text);
DROP TYPE telephone CASCADE;
COMMIT;
