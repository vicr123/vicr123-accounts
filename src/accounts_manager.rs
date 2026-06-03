use crate::bus::{create_mail_message, user_object};
use crate::error::Error;
use crate::token_provisioning::{TokenProvisioningManager, TokenProvisioningPurpose};
use crate::validation::{validate_email_address, validate_password, validate_username};
use crate::{VariantMap, generate_hashed_password, generate_salt, send_verification_email};
use base64::Engine;
use base64::prelude::BASE64_STANDARD;
use sqlx::{PgPool, Row};
use std::sync::Arc;
use zbus::zvariant::ObjectPath;
use zbus::{Connection, interface};
use zvariant::{Str, Value};

pub struct AccountsManager {
    database: PgPool,
    token_provisioning_manager: Arc<TokenProvisioningManager>,
}

pub async fn user_id_by_username(database: &PgPool, username: &str) -> Result<i32, Error> {
    let query_result = sqlx::query("SELECT id FROM users WHERE username=$1")
        .bind(username)
        .fetch_one(database)
        .await;

    match query_result {
        Ok(result) => Ok(result.try_get::<i32, _>("id")?),
        Err(sqlx::Error::RowNotFound) => Err(Error::NoAccount),
        Err(e) => Err(e.into()),
    }
}

impl AccountsManager {
    pub fn new(database: PgPool, bus: Connection) -> Self {
        let token_provisioning_manager =
            Arc::new(TokenProvisioningManager::new(bus.clone(), database.clone()));

        Self {
            database,
            token_provisioning_manager,
        }
    }
}

#[interface(name = "com.vicr123.accounts.Manager")]
impl AccountsManager {
    async fn user_id_by_username(&self, username: &str) -> Result<u64, Error> {
        user_id_by_username(&self.database, username)
            .await
            .map(|id| id.try_into().unwrap())
    }

    async fn create_user(
        &self,
        username: &str,
        password: &str,
        email: &str,
        #[zbus(connection)] connection: &Connection,
    ) -> Result<ObjectPath, Error> {
        if username.is_empty() || password.is_empty() || email.is_empty() {
            return Err(Error::InvalidInput);
        }
        if !validate_username(username)
            || !validate_password(password)
            || !validate_email_address(email)
        {
            return Err(Error::InvalidInput);
        }

        let hashed_password = generate_hashed_password(password, 10000);

        let new_user_id = sqlx::query(
            "INSERT INTO users(username, password, email) VALUES($1, $2, $3) RETURNING id",
        )
        .bind(username)
        .bind(hashed_password)
        .bind(email)
        .fetch_one(&self.database)
        .await?
        .try_get::<i32, _>("id")?;

        let _ = send_verification_email(&self.database, new_user_id).await;

        let path = user_object(connection, &self.database, new_user_id, async |account| {
            account.path()
        })
        .await?;
        Ok(path)
    }

    async fn user_by_id(
        &self,
        id: u64,
        #[zbus(connection)] connection: &Connection,
    ) -> Result<ObjectPath, Error> {
        let path = user_object(connection, &self.database, id as i32, async |account| {
            account.path()
        })
        .await?;
        Ok(path)
    }

    async fn provision_token(
        &self,
        username: &str,
        password: &str,
        application: &str,
        extra_options: VariantMap<'_>,
    ) -> Result<String, Error> {
        let mut options = VariantMap::new();
        options.insert("username".into(), Value::Str(Str::from(username)));
        options.insert("password".into(), Value::Str(Str::from(password)));
        options.extend(extra_options);

        let response = self
            .token_provisioning_manager
            .provision(
                "password",
                TokenProvisioningPurpose::Login,
                application,
                options,
            )
            .await?;

        Ok(response
            .get("token")
            .expect("Token not found in response")
            .to_string())
    }

    async fn force_provision_token(
        &self,
        user_id: u64,
        application: &str,
        #[zbus(connection)] connection: &Connection,
    ) -> Result<String, Error> {
        if application.is_empty() {
            return Err(Error::InvalidInput);
        }

        let user_id = user_id as i32;
        user_object(connection, &self.database, user_id, async |_| {}).await?;

        let new_token = BASE64_STANDARD.encode(*generate_salt());

        sqlx::query("INSERT INTO tokens(userid, token, application) VALUES($1, $2, $3)")
            .bind(user_id)
            .bind(&new_token)
            .bind(application)
            .execute(&self.database)
            .await?;
        Ok(new_token)
    }

    async fn user_for_token(
        &self,
        token: &str,
        #[zbus(connection)] connection: &Connection,
    ) -> Result<ObjectPath, Error> {
        self.user_for_token_with_purpose(token, "login", connection)
            .await
    }

    async fn user_for_token_with_purpose(
        &self,
        token: &str,
        expected_token_purpose: &str,
        #[zbus(connection)] connection: &Connection,
    ) -> Result<ObjectPath, Error> {
        match self.token_provisioning_manager.verify_token(token).await? {
            None => Err(Error::NoAccount),
            Some(verified_token) => {
                if verified_token.purpose != expected_token_purpose.into() {
                    Err(Error::NoAccount)
                } else {
                    self.user_by_id(verified_token.user_id as u64, connection)
                        .await
                }
            }
        }
    }

    async fn all_users(&self) -> Result<Vec<u64>, Error> {
        Ok(sqlx::query("SELECT * FROM users")
            .fetch_all(&self.database)
            .await?
            .iter()
            .filter_map(|row| row.try_get::<i32, _>("id").ok())
            .map(|id| id as u64)
            .collect())
    }

    async fn token_provisioning_methods(
        &self,
        username: &str,
        application: &str,
    ) -> Result<Vec<&'static str>, Error> {
        self.token_provisioning_methods_with_purpose(username, "login", application)
            .await
    }

    async fn token_provisioning_methods_with_purpose(
        &self,
        username: &str,
        purpose: &str,
        application: &str,
    ) -> Result<Vec<&'static str>, Error> {
        let id = user_id_by_username(&self.database, username).await?;

        // Ensure the account is not disabled
        let password_hash = sqlx::query("SELECT * FROM users WHERE id=$1")
            .bind(id)
            .fetch_one(&self.database)
            .await?
            .try_get::<String, _>("password")?;
        if password_hash.starts_with("!") {
            Err(Error::DisabledAccount)
        } else {
            Ok(self
                .token_provisioning_manager
                .available_methods(id, application, purpose)
                .await)
        }
    }

    async fn provision_token_by_method(
        &self,
        method: &str,
        username: &str,
        application: &str,
        extra_options: VariantMap<'_>,
    ) -> Result<VariantMap, Error> {
        let purpose = extra_options
            .get("purpose")
            .and_then(|purpose| match purpose {
                Value::Str(s) => Some(s.to_string()),
                _ => None,
            })
            .map(|purpose| purpose.into())
            .unwrap_or(TokenProvisioningPurpose::Login);

        let mut options = VariantMap::new();
        options.insert("username".into(), Value::Str(Str::from(username)));
        options.insert("application".into(), Value::Str(Str::from(application)));
        options.extend(extra_options);

        self.token_provisioning_manager
            .provision(method, purpose, application, options)
            .await
    }

    async fn create_mail_message(
        &self,
        to: &str,
        #[zbus(connection)] connection: &Connection,
    ) -> Result<ObjectPath, Error> {
        Ok(create_mail_message(connection, to).await)
    }
}
