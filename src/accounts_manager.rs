use crate::account::Account;
use crate::error::Error;
use crate::token_provisioning::{TokenProvisioningManager, TokenProvisioningPurpose};
use crate::validation::{validate_email_address, validate_password, validate_username};
use crate::{VariantMap, generate_hashed_password, generate_salt, send_verification_email};
use base64::Engine;
use base64::prelude::BASE64_STANDARD;
use sqlx::{PgPool, Row};
use std::collections::HashMap;
use std::sync::Arc;
use zbus::zvariant::ObjectPath;
use zbus::{Connection, interface};
use zvariant::{Str, Value};

pub struct AccountsManager {
    database: PgPool,
    bus: Connection,
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
        let token_provisioning_manager = Arc::new(TokenProvisioningManager::new(database.clone()));
        Self {
            database,
            bus,
            token_provisioning_manager,
        }
    }

    async fn user_object(&self, id: i32) -> Result<ObjectPath, Error> {
        let path = ObjectPath::try_from(format!("/com/vicr123/accounts/User{id}")).unwrap();

        // Ensure the user account exists
        let count = sqlx::query("SELECT COUNT(*) FROM users WHERE id=$1")
            .bind(id)
            .fetch_one(&self.database)
            .await?
            .try_get::<i64, _>(0)?;
        if count == 0 {
            return Err(Error::NoAccount);
        }

        if self
            .bus
            .object_server()
            .interface::<_, Account>(&path)
            .await
            .is_err()
        {
            let account = Account::new(id, self.database.clone());
            let _ = self.bus.object_server().at(&path, account).await;
        }
        Ok(path)
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

        send_verification_email(self.database.clone(), new_user_id);

        self.user_object(new_user_id).await
    }

    async fn user_by_id(&self, id: u64) -> Result<ObjectPath, Error> {
        self.user_object(id as i32).await
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
    ) -> Result<String, Error> {
        if application.is_empty() {
            return Err(Error::InvalidInput);
        }

        let user_id = user_id as i32;
        self.user_object(user_id).await?;

        let new_token = BASE64_STANDARD.encode(*generate_salt());

        sqlx::query("INSERT INTO tokens(userid, token, application) VALUES($1, $2, $3)")
            .bind(user_id)
            .bind(&new_token)
            .bind(application)
            .execute(&self.database)
            .await?;
        Ok(new_token)
    }

    async fn all_users(&self) -> String {
        "Hello".to_string()
    }
}
