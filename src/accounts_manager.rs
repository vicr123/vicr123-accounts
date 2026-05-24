use crate::error::Error;
use sqlx::Row;
use sqlx::postgres::PgRow;
use zbus::interface;

pub struct AccountsManager {
    database: sqlx::PgPool,
}

impl AccountsManager {
    pub fn new(database: sqlx::PgPool) -> Self {
        Self { database }
    }
}

#[interface(name = "com.vicr123.accounts.Manager")]
impl AccountsManager {
    async fn user_id_by_username(&self, username: String) -> Result<u64, Error> {
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

    async fn all_users(&self) -> String {
        "Hello".to_string()
    }
}
