CREATE TABLE version (
    number INT
        CONSTRAINT version_pk
            PRIMARY KEY
);

INSERT INTO version VALUES(1);

CREATE FUNCTION generate_user_id() RETURNS INTEGER
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
        cnt := (SELECT COUNT(*)::INT FROM users WHERE id = num);
        EXIT gen WHEN cnt = 0;
    END LOOP;

    RETURN num;
END
$$;

CREATE TABLE users (
    id        INTEGER DEFAULT generate_user_id() NOT NULL
        CONSTRAINT users_pkey
            PRIMARY KEY,
    username  TEXT
        CONSTRAINT users_username_key
            UNIQUE,
    password  TEXT,
    email     TEXT
        CONSTRAINT users_email_key
            UNIQUE,
    locale    TEXT    DEFAULT 'en',
    verified  BOOLEAN DEFAULT FALSE
);

CREATE TABLE tokens (
    userid      INTEGER NOT NULL
        CONSTRAINT fk_tokens_userid
            REFERENCES users
            ON UPDATE CASCADE ON DELETE CASCADE,
    token       TEXT    NOT NULL
        CONSTRAINT tokens_token_key
            UNIQUE,
    application TEXT    NOT NULL,
    CONSTRAINT pk_tokens
        PRIMARY KEY (userid, token)
);

CREATE TABLE verifications (
    userid             INTEGER NOT NULL
        CONSTRAINT pk_verifications
            PRIMARY KEY
        CONSTRAINT fk_verifications_userid
            REFERENCES users
            ON UPDATE CASCADE ON DELETE CASCADE,
    verificationstring TEXT    NOT NULL
        CONSTRAINT verifications_verificationstring_key
            UNIQUE,
    expiry             BIGINT  NOT NULL
);

CREATE TABLE otp (
    userid  INTEGER NOT NULL
        CONSTRAINT otp_pkey
            PRIMARY KEY
        CONSTRAINT fk_tokens_userid
            REFERENCES users
            ON UPDATE CASCADE ON DELETE CASCADE,
    otpkey  TEXT,
    enabled BOOLEAN DEFAULT FALSE
);

CREATE TABLE otpbackup (
    userid    INTEGER NOT NULL
        CONSTRAINT fk_tokens_userid
            REFERENCES users
            ON UPDATE CASCADE ON DELETE CASCADE,
    backupkey TEXT    NOT NULL,
    used      BOOLEAN DEFAULT FALSE,
    CONSTRAINT pk_otpbackup
        PRIMARY KEY (userid, backupkey)
);

CREATE TABLE passwordresets (
    userid            INTEGER NOT NULL
        CONSTRAINT pk_passwordresets
            PRIMARY KEY
        CONSTRAINT fk_passwordresets_userid
            REFERENCES users
            ON UPDATE CASCADE ON DELETE CASCADE,
    temporarypassword TEXT    NOT NULL
        CONSTRAINT passwordresets_temporarypassword_key
            UNIQUE,
    expiry            BIGINT  NOT NULL
);
