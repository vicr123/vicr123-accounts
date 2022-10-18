BEGIN;

CREATE FUNCTION generate_fido_id() RETURNS INTEGER
    LANGUAGE plpgsql
AS
$$
DECLARE
    num INT := 0;
    cnt INT := 0;
BEGIN
    <<gen>>
    LOOP
        num = FLOOR(RANDOM() * 10000000 + 1000000)::INT;
        cnt := (SELECT COUNT(*)::INT FROM fido WHERE id = num);
        EXIT gen WHEN cnt = 0;
    END LOOP;

    RETURN num;
END
$$;

CREATE TABLE fido
(
    id        INTEGER DEFAULT generate_fido_id() NOT NULL
        CONSTRAINT fido_pkey
            PRIMARY KEY,
    userid      integer
        constraint fido_users_id_fk
            references users (id),
    data        bytea,
    name        varchar,
    application varchar
);

DELETE FROM version;
INSERT INTO version VALUES(2);

COMMIT;