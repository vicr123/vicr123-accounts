use base64::Engine;
use base64::prelude::BASE64_STANDARD;
use sqlx::PgPool;
use crate::error::Error;
use crate::{generate_salt, VariantMap};

pub mod password_provisioning_method;

pub struct TokenProvisioningManager {
    database: PgPool
}

#[derive(Debug, Clone, Copy, Eq, PartialEq)]
pub enum TokenProvisioningPurpose {
    Login,
    AccountModification,
    Unknown,
}

impl From<&str> for TokenProvisioningPurpose {
    fn from(value: &str) -> Self {
        match value {
            "login" => Self::Login,
            "accountModification" => Self::AccountModification,
            _ => Self::Unknown,
        }
    }
}

impl TokenProvisioningManager {
    pub fn new(database: PgPool) -> Self {
        TokenProvisioningManager {
            database,
        }
    }

    pub async fn provision(
        &self,
        method_name: &str,
        purpose: impl Into<TokenProvisioningPurpose>,
        application: &str,
        options: VariantMap<'_>,
    ) -> Result<VariantMap<'_>, Error> {
        let purpose = purpose.into();
        if purpose == TokenProvisioningPurpose::Unknown {
            return Err(Error::InvalidInput);
        }
        
        let result = match method_name {
            "password" => {
                password_provisioning_method::provision(&self.database, options.clone(), purpose).await?
            }
            _ => return Err(Error::InternalError)
        };
        match result {
            ProvisionResult::Success(user_id) => {
                match purpose {
                    TokenProvisioningPurpose::Login => {
                        // Create a new user token and save it in the database
                        let new_token = BASE64_STANDARD.encode(*generate_salt());
                        sqlx::query("INSERT INTO tokens(userid, token, application) VALUES($1, $2, $3)")
                            .bind(user_id)
                            .bind(&new_token)
                            .bind(application.to_string())
                            .execute(&self.database)
                            .await?;
                        
                        let mut map = VariantMap::new();
                        map.insert("token".into(), new_token.into());
                        return Ok(map);
                    }
                    TokenProvisioningPurpose::AccountModification => {
                        // TODO
                    }
                    TokenProvisioningPurpose::Unknown => {
                        unreachable!()
                    }
                }
            }
            ProvisionResult::Challenge(challenge) => {
                return Ok(challenge);
            }
        }

        Err(Error::InternalError)
    }

    pub fn available_methods(
        &self,
        user_id: i32,
        application: &str,
        purpose: impl Into<TokenProvisioningPurpose>,
    ) -> Vec<&'static str> {
        let purpose = purpose.into();
        let available = vec!["password"];
        available
    }
}

pub enum ProvisionResult<'a> {
    Success(i32),
    Challenge(VariantMap<'a>),
}

