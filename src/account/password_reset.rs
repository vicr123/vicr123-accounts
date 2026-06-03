use crate::error::Error;
use crate::{VariantMap, generate_hashed_password, send_template_email};
use base64::Engine;
use base64::prelude::BASE64_STANDARD;
use jwt_simple::prelude::Serialize;
use rand::RngExt;
use sqlx::{PgPool, Row};
use std::collections::HashMap;
use zbus::interface;
use zvariant::{OwnedValue, Type, Value};

#[derive(Debug, Clone, Serialize, Type, Value, OwnedValue)]
pub struct ResetMethod {
    r#type: String,
    challenge: VariantMap<'static>,
}

pub struct PasswordReset {
    id: i32,
    database: PgPool,
}

impl PasswordReset {
    pub async fn new(id: i32, database: PgPool) -> Result<Self, Error> {
        Ok(Self { id, database })
    }

    pub async fn issue_password_reset(&self) -> Result<(), Error> {
        let database = self.database.clone();
        let id = self.id.clone();
        tokio::spawn(async move {
            let Ok(user) = sqlx::query("SELECT * FROM users WHERE id=$1")
                .bind(id)
                .fetch_one(&database)
                .await
            else {
                return;
            };

            let Ok(email) = user.try_get::<String, _>("email") else {
                return;
            };
            let Ok(username) = user.try_get::<String, _>("username") else {
                return;
            };

            let mut random_bytes = [0u8; 24];
            rand::rng().fill(&mut random_bytes);
            let password = BASE64_STANDARD.encode(random_bytes);
            let hashed_password = generate_hashed_password(&password, 10000);

            if sqlx::query(
                "INSERT INTO passwordResets(userId, temporaryPassword, expiry)
                         VALUES($1, $2, $3)
                            ON CONFLICT
                                ON CONSTRAINT pk_passwordresets
                                    DO UPDATE
                                        SET temporaryPassword = $2, expiry = $3",
            )
            .bind(id)
            .bind(hashed_password)
            .bind((chrono::Utc::now() + chrono::Duration::minutes(30)).timestamp())
            .execute(&database)
            .await
            .is_err()
            {
                return;
            };

            let _ = send_template_email(
                "recover",
                email,
                "en",
                HashMap::from([
                    ("user".to_string(), username),
                    ("password".to_string(), password),
                ]),
            )
            .await;
        });

        Ok(())
    }
}

#[interface(name = "com.vicr123.accounts.PasswordReset")]
impl PasswordReset {
    pub async fn reset_methods(&self) -> Result<Vec<ResetMethod>, Error> {
        let user = sqlx::query("SELECT * FROM users WHERE id=$1")
            .bind(self.id)
            .fetch_one(&self.database)
            .await?;

        let mut methods = Vec::new();
        if let Ok(email) = user.try_get::<String, _>("email") {
            'email: {
                let mut email_parts = email.split("@");
                let Some(user) = email_parts.next() else {
                    break 'email;
                };
                let Some(domain) = email_parts.next() else {
                    break 'email;
                };
                let user = &user[..2];
                let domain = &domain[..1];

                methods.push(ResetMethod {
                    r#type: "email".to_string(),
                    challenge: VariantMap::from([
                        ("user".to_string(), Value::from(user.to_string())),
                        ("domain".to_string(), Value::from(domain.to_string())),
                    ]),
                })
            }
        }
        Ok(methods)
    }

    pub async fn reset_password(
        &self,
        r#type: &str,
        challenge: VariantMap<'_>,
    ) -> Result<(), Error> {
        let user = sqlx::query("SELECT * FROM users WHERE id=$1")
            .bind(self.id)
            .fetch_one(&self.database)
            .await?;

        if r#type == "email" {
            let email = user
                .try_get::<String, _>("email")
                .map_err(|_| Error::InvalidInput)?;
            let Some(challenge_email) = challenge.get("email").and_then(|email| match email {
                Value::Str(s) => Some(s.to_string()),
                _ => None,
            }) else {
                return Err(Error::InvalidInput);
            };

            if email == challenge_email {
                // Issue the password reset
                let _ = self.issue_password_reset().await;
            }
            return Ok(());
        }

        Err(Error::InvalidInput)
    }
}
