use sqlx::PgPool;
use zbus::interface;
use crate::error::Error;
use crate::generate_hashed_password;
use crate::validation::validate_password;

pub struct Account {
    id: i32,
    database: PgPool,
}

impl Account {
    pub fn new(id: i32, database: PgPool) -> Self {
        Self { id, database }
    }
}

#[interface(name = "com.vicr123.accounts.User")]
impl Account {
    #[zbus(property)]
    pub async fn id(&self) -> i32 {
        self.id
    }

    #[zbus(property)]
    pub async fn verified(&self) -> bool {
        false
    }

    pub async fn set_password(&self, password: &str) -> Result<(), Error> {
        if password.is_empty() {
            return Err(Error::InvalidInput);
        }

        if !validate_password(password) {
            return Err(Error::InvalidInput);
        }

        let hashed_password = generate_hashed_password(password, 10000);

        sqlx::query("UPDATE accounts SET password = $1 WHERE id = $2")
            .bind(hashed_password)
            .bind(self.id)
            .execute(&self.database)
            .await?;

        if self.verified().await {
            // TODO: Send password change email
        }

        Ok(())
    }
}