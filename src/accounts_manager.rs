use crate::account::Account;
use crate::error::Error;
use crate::validation::{validate_email_address, validate_password, validate_username};
use crate::{generate_hashed_password, send_verification_email};
use sqlx::Row;
use zbus::zvariant::ObjectPath;
use zbus::{Connection, interface};

pub struct AccountsManager {
    database: sqlx::PgPool,
    bus: Connection,
}

impl AccountsManager {
    pub fn new(database: sqlx::PgPool, bus: Connection) -> Self {
        Self { database, bus }
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
        let query_result = sqlx::query("SELECT id FROM users WHERE username=$1")
            .bind(username)
            .fetch_one(&self.database)
            .await;

        match query_result {
            Ok(result) => Ok(result.try_get::<i32, _>("id")?.try_into().unwrap()),
            Err(sqlx::Error::RowNotFound) => Err(Error::NoAccount),
            Err(e) => Err(e.into()),
        }
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

    async fn all_users(&self) -> String {
        "Hello".to_string()
    }
}
