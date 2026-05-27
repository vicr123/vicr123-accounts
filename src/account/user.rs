use crate::bus::{create_mail_message, user_object};
use crate::error::Error;
use crate::validation::{validate_email_address, validate_password, validate_username};
use crate::{
    generate_hashed_password, send_template_email, send_verification_email, verify_hashed_password,
};
use sqlx::{PgPool, Row};
use std::collections::HashMap;
use zbus::object_server::SignalEmitter;
use zbus::{Connection, interface};
use zvariant::{ObjectPath, OwnedObjectPath};

pub struct User {
    id: i32,
    verified: bool,
    username: String,
    email: String,

    database: PgPool,
    path: String,
}

impl User {
    pub async fn new(id: i32, database: PgPool, path: String) -> Result<Self, Error> {
        let row = sqlx::query("SELECT * FROM users WHERE id=$1")
            .bind(id)
            .fetch_one(&database)
            .await?;

        let username = row.try_get::<String, _>("username")?;
        let email = row.try_get::<String, _>("email")?;
        let verified = row.try_get::<bool, _>("verified")?;

        Ok(Self {
            id,
            verified,
            username,
            email,
            database,
            path,
        })
    }

    pub fn path(&self) -> ObjectPath<'static> {
        ObjectPath::try_from(self.path.clone()).unwrap()
    }

    pub fn locale(&self) -> String {
        "en".to_string()
    }
}

#[interface(name = "com.vicr123.accounts.User")]
impl User {
    #[zbus(property)]
    pub async fn id(&self) -> u64 {
        self.id as u64
    }

    #[zbus(property)]
    pub async fn username(&self) -> String {
        self.username.clone()
    }

    #[zbus(property)]
    pub async fn email(&self) -> String {
        self.email.clone()
    }

    #[zbus(property)]
    pub async fn verified(&self) -> bool {
        self.verified
    }

    pub async fn set_username(
        &mut self,
        username: &str,
        #[zbus(signal_emitter)] emitter: SignalEmitter<'_>,
    ) -> Result<(), Error> {
        if username.is_empty() {
            return Err(Error::InvalidInput);
        }

        if !validate_username(username) {
            return Err(Error::InvalidInput);
        }

        sqlx::query("UPDATE users SET username=$1 WHERE id=$2")
            .bind(username)
            .bind(self.id)
            .execute(&self.database)
            .await?;

        let old_username = std::mem::replace(&mut self.username, username.to_string());
        self.username_changed(&emitter).await.unwrap();
        emitter
            .username_changed_2(&old_username, username)
            .await
            .unwrap();

        Ok(())
    }

    #[zbus(signal, name = "UsernameChanged")]
    async fn username_changed_2(
        emitter: &SignalEmitter<'_>,
        old_username: &str,
        new_username: &str,
    ) -> zbus::Result<()>;

    pub async fn set_password(&self, password: &str) -> Result<(), Error> {
        if password.is_empty() {
            return Err(Error::InvalidInput);
        }

        if !validate_password(password) {
            return Err(Error::InvalidInput);
        }

        let hashed_password = generate_hashed_password(password, 10000);

        sqlx::query("UPDATE users SET password = $1 WHERE id = $2")
            .bind(hashed_password)
            .bind(self.id)
            .execute(&self.database)
            .await?;

        if self.verified().await {
            // TODO: Send password change email
            let _ = send_template_email(
                "passwordchange",
                self.email.clone(),
                &self.locale(),
                HashMap::from([("user".into(), self.username.clone())]),
            )
            .await;
        }

        Ok(())
    }

    pub async fn set_email(
        &mut self,
        email: &str,
        #[zbus(signal_emitter)] emitter: SignalEmitter<'_>,
    ) -> Result<(), Error> {
        if !validate_email_address(email) {
            return Err(Error::InvalidInput);
        }

        sqlx::query("UPDATE users SET email=$1, verified=false WHERE id=$2")
            .bind(email)
            .bind(self.id)
            .execute(&self.database)
            .await?;

        self.email = email.to_string();
        self.verified = false;

        self.verified_changed(&emitter).await.unwrap();
        emitter.verified_changed_2(false).await.unwrap();
        self.email_changed(&emitter).await.unwrap();
        emitter.email_changed_2(email).await.unwrap();

        Ok(())
    }

    pub async fn resend_verification_email(&self) -> Result<(), Error> {
        send_verification_email(&self.database, self.id).await
    }

    pub async fn verify_email(
        &mut self,
        verification_code: &str,
        #[zbus(signal_emitter)] emitter: SignalEmitter<'_>,
    ) -> Result<(), Error> {
        if verification_code.is_empty() {
            return Err(Error::InvalidInput);
        }

        let mut transaction = self.database.begin().await?;

        let affected_rows = sqlx::query(
            "DELETE FROM verifications
                     WHERE userid = $1 AND verificationstring = $2 AND expiry > $3",
        )
        .bind(self.id)
        .bind(verification_code.to_string())
        .bind(chrono::Utc::now().timestamp())
        .execute(&mut *transaction)
        .await?
        .rows_affected();

        if affected_rows == 0 {
            return Err(Error::VerificationCodeIncorrect);
        }

        sqlx::query("UPDATE users SET verified=true WHERE id=$1")
            .bind(self.id)
            .execute(&mut *transaction)
            .await?;
        transaction.commit().await?;

        self.verified = true;
        self.verified_changed(&emitter).await.unwrap();
        emitter.verified_changed_2(true).await.unwrap();

        Ok(())
    }

    #[zbus(signal, name = "EmailChanged")]
    async fn email_changed_2(emitter: &SignalEmitter<'_>, new_email: &str) -> zbus::Result<()>;

    #[zbus(signal, name = "VerifiedChanged")]
    async fn verified_changed_2(emitter: &SignalEmitter<'_>, verified: bool) -> zbus::Result<()>;

    pub async fn verify_password(&self, password: &str) -> Result<bool, Error> {
        if password.is_empty() {
            return Err(Error::InvalidInput);
        }

        let row = sqlx::query("SELECT * FROM users WHERE id=$1")
            .bind(self.id)
            .fetch_one(&self.database)
            .await?;

        let password_hash = row.try_get::<String, _>("password")?;
        if password_hash.starts_with("!") {
            return Err(Error::DisabledAccount);
        }

        Ok(verify_hashed_password(password, &password_hash))
    }

    pub async fn erase_password(&self) -> Result<(), Error> {
        sqlx::query("UPDATE users SET password='x' WHERE id=$1")
            .bind(self.id)
            .execute(&self.database)
            .await?;

        Ok(())
    }

    pub async fn set_email_verified(
        &mut self,
        verified: bool,
        #[zbus(signal_emitter)] emitter: SignalEmitter<'_>,
    ) -> Result<(), Error> {
        sqlx::query("UPDATE users SET verified=$1 WHERE id=$2")
            .bind(verified)
            .bind(self.id)
            .execute(&self.database)
            .await?;

        self.verified = verified;
        self.verified_changed(&emitter).await.unwrap();
        emitter.verified_changed_2(verified).await.unwrap();

        Ok(())
    }

    pub async fn create_mail_message(
        &self,
        #[zbus(connection)] connection: &Connection,
    ) -> Result<ObjectPath<'_>, Error> {
        if !self.verified {
            return Err(Error::AccountEmailNotVerified);
        }

        Ok(create_mail_message(connection, &self.email).await)
    }
}
