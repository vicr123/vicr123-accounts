use crate::error::Error;
use crate::{VariantMap, generate_salt};
use base64::Engine;
use base64::prelude::BASE64_STANDARD;
use jwt_simple::algorithms::{HS256Key, MACLike};
use jwt_simple::claims::Claims;
use jwt_simple::prelude::Duration;
use serde::{Deserialize, Serialize};
use sqlx::{PgPool, Row};
use zbus::Connection;

pub mod fido_provisioning_method;
pub mod password_provisioning_method;

pub struct TokenProvisioningManager {
    bus: Connection,
    database: PgPool,

    jwt_key: HS256Key,
}

pub enum ProvisionResult<'a> {
    Success(i32),
    Challenge(VariantMap<'a>),
}

#[derive(Debug, Clone, Copy, Eq, PartialEq, Serialize, Deserialize)]
pub enum TokenProvisioningPurpose {
    Login,
    AccountModification,
    Unknown,
}

pub struct VerifiedToken {
    pub user_id: i32,
    pub purpose: TokenProvisioningPurpose,
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
impl From<String> for TokenProvisioningPurpose {
    fn from(value: String) -> Self {
        TokenProvisioningPurpose::from(value.as_str())
    }
}

#[derive(Serialize, Deserialize)]
struct AccountModificationTokenClaims {
    purpose: TokenProvisioningPurpose,
}

impl TokenProvisioningManager {
    pub fn new(bus: Connection, database: PgPool) -> Self {
        let jwt_key = HS256Key::generate();

        TokenProvisioningManager {
            bus,
            database,
            jwt_key,
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
                password_provisioning_method::provision(
                    &self.bus,
                    &self.database,
                    options.clone(),
                    purpose,
                )
                .await?
            }
            "fido" => {
                fido_provisioning_method::provision(
                    &self.bus,
                    &self.database,
                    options.clone(),
                    purpose,
                )
                .await?
            }
            _ => return Err(Error::InternalError),
        };

        match result {
            ProvisionResult::Success(user_id) => {
                match purpose {
                    TokenProvisioningPurpose::Login => {
                        // Create a new user token and save it in the database
                        let new_token = BASE64_STANDARD.encode(*generate_salt());
                        sqlx::query(
                            "INSERT INTO tokens(userid, token, application) VALUES($1, $2, $3)",
                        )
                        .bind(user_id)
                        .bind(&new_token)
                        .bind(application.to_string())
                        .execute(&self.database)
                        .await?;

                        let mut map = VariantMap::new();
                        map.insert("token".into(), new_token.into());
                        Ok(map)
                    }
                    TokenProvisioningPurpose::AccountModification => {
                        // Create a short-lived JWT that we can use to perform account modification actions
                        let claims = Claims::with_custom_claims(
                            AccountModificationTokenClaims { purpose },
                            Duration::from_hours(1),
                        )
                        .with_subject(user_id);
                        let token = self.jwt_key.authenticate(claims).unwrap();

                        let mut map = VariantMap::new();
                        map.insert("token".into(), token.into());
                        Ok(map)
                    }
                    TokenProvisioningPurpose::Unknown => {
                        unreachable!()
                    }
                }
            }
            ProvisionResult::Challenge(challenge) => Ok(challenge),
        }
    }

    pub async fn verify_token(&self, token: &str) -> Result<Option<VerifiedToken>, Error> {
        if let Ok(claims) = self
            .jwt_key
            .verify_token::<AccountModificationTokenClaims>(token, None)
        {
            let Some(subject) = claims.subject else {
                return Ok(None);
            };

            let Ok(subject) = subject.parse::<i32>() else {
                return Ok(None);
            };

            return Ok(Some(VerifiedToken {
                user_id: subject,
                purpose: claims.custom.purpose,
            }));
        }

        // Now read the database for tokens
        match sqlx::query("SELECT * FROM tokens WHERE token=$1")
            .bind(token)
            .fetch_one(&self.database)
            .await
        {
            Ok(row) => {
                let user_id = row.try_get::<i32, _>("userid")?;
                Ok(Some(VerifiedToken {
                    user_id,
                    purpose: TokenProvisioningPurpose::Login,
                }))
            }
            Err(sqlx::Error::RowNotFound) => Ok(None),
            Err(e) => Err(e.into()),
        }
    }

    pub async fn available_methods(
        &self,
        user_id: i32,
        application: &str,
        purpose: impl Into<TokenProvisioningPurpose>,
    ) -> Vec<&'static str> {
        let purpose = purpose.into();
        let mut available = vec!["password"];
        if let Ok(true) =
            fido_provisioning_method::available(&self.database, user_id, application, purpose).await
        {
            available.push("fido");
        }
        available
    }
}
