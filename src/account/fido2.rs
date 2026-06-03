use crate::bus::user_object;
use crate::error::Error;
use crate::fido::{fido_creds_for_user, helper_path};
use crate::send_template_email;
use serde::{Deserialize, Serialize};
use serde_json::Map;
use sqlx::{PgPool, Row};
use std::collections::HashMap;
use std::process::Stdio;
use tokio::io::AsyncWriteExt;
use tokio::process::Command;
use zbus::{Connection, interface};
use zvariant::{OwnedValue, Type, Value};

pub struct Fido2 {
    id: i32,
    database: PgPool,
    registration_state: Option<RegistrationState>,
}

struct RegistrationState {
    application: String,
    relying_party: String,
    preregister_options: serde_json::Value,
}

#[derive(Debug, Clone, Serialize, Type, Value, OwnedValue)]
pub struct Fido2Key {
    id: i32,
    application: String,
    name: String,
}

impl Fido2 {
    pub async fn new(id: i32, database: PgPool) -> Result<Self, Error> {
        Ok(Self {
            id,
            database,
            registration_state: None,
        })
    }
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]
struct PreregisterPayload {
    existing_creds: Vec<String>,
}

#[interface(name = "com.vicr123.accounts.Fido2")]
impl Fido2 {
    pub async fn prepare_register(
        &mut self,
        application: &str,
        relying_party: &str,
        authenticator_attachment: i32,
        #[zbus(connection)] connection: &Connection,
    ) -> Result<String, Error> {
        if !(0..=2).contains(&authenticator_attachment) {
            return Err(Error::InvalidInput);
        }

        let Some(username) = user_object(connection, &self.database, self.id, async |user| {
            if user.verified().await {
                Some(user.username().await)
            } else {
                None
            }
        })
        .await?
        else {
            return Err(Error::InternalError);
        };

        let mut command = Command::new(helper_path().map_err(|_| Error::FidoSupportUnavailable)?);
        command
            .args([
                "preregister",
                "--rpname",
                application,
                "--rpid",
                relying_party,
                "--username",
                &username,
                "--userid",
                &self.id.to_string(),
            ])
            .stdin(Stdio::piped())
            .stdout(Stdio::piped());

        match authenticator_attachment {
            0 => {
                command.args(["--authattachment", "platform"]);
            }
            1 => {
                command.args(["--authattachment", "cross-platform"]);
            }
            _ => {}
        };

        let payload = PreregisterPayload {
            existing_creds: fido_creds_for_user(self.id, Some(application), &self.database)
                .await?
                .iter()
                .map(|cred| serde_json::to_string(cred).unwrap())
                .collect(),
        };
        let payload = serde_json::to_string(&payload).unwrap();

        let mut child = command.spawn().map_err(|_| Error::FidoSupportUnavailable)?;
        let mut stdin = child.stdin.take().unwrap();
        stdin
            .write_all(payload.as_bytes())
            .await
            .map_err(|_| Error::InternalError)?;
        stdin.flush().await.map_err(|_| Error::InternalError)?;
        drop(stdin);

        let output = child.wait_with_output().await;
        let Ok(output) = output else {
            return Err(Error::InternalError);
        };

        let preregister_options: serde_json::Value =
            serde_json::from_str(&String::from_utf8_lossy(&output.stdout)).map_err(|_| Error::InternalError)?;

        self.registration_state = Some(RegistrationState {
            application: application.to_string(),
            relying_party: relying_party.to_string(),
            preregister_options: preregister_options.clone(),
        });

        Ok(serde_json::to_string(&preregister_options).unwrap())
    }

    pub async fn complete_register(
        &mut self,
        response: &str,
        expect_origins: Vec<String>,
        key_name: &str,
        #[zbus(connection)] connection: &Connection,
    ) -> Result<(), Error> {
        let Some(registration) = self.registration_state.take() else {
            return Err(Error::InvalidInput);
        };

        let response: serde_json::Value =
            serde_json::from_str(response).map_err(|_| Error::InvalidInput)?;

        let mut command = Command::new(helper_path().map_err(|_| Error::FidoSupportUnavailable)?);
        command
            .args([
                "register",
                "--rpname",
                &registration.application,
                "--rpid",
                &registration.relying_party,
                "--userid",
                &self.id.to_string(),
            ])
            .stdin(Stdio::piped())
            .stdout(Stdio::piped());

        let mut payload = Map::new();
        payload.insert(
            "preregisterOptions".to_string(),
            registration.preregister_options,
        );
        payload.insert("response".to_string(), response);
        payload.insert(
            "expectOrigins".to_string(),
            serde_json::Value::from(expect_origins),
        );

        let mut child = command.spawn().map_err(|_| Error::FidoSupportUnavailable)?;
        let mut stdin = child.stdin.take().unwrap();
        stdin
            .write_all(&serde_json::to_vec(&serde_json::Value::from(payload)).unwrap())
            .await
            .map_err(|_| Error::InternalError)?;
        stdin.flush().await.map_err(|_| Error::InternalError)?;
        drop(stdin);

        let output = child.wait_with_output().await;
        let Ok(output) = output else {
            return Err(Error::InternalError);
        };

        sqlx::query("INSERT INTO fido(userid, data, name, application) VALUES($1, $2, $3, $4)")
            .bind(self.id)
            .bind(output.stdout)
            .bind(key_name)
            .bind(&registration.application)
            .execute(&self.database)
            .await?;

        if let Some((email, username, locale)) =
            user_object(connection, &self.database, self.id, async |user| {
                if user.verified().await {
                    Some((user.email().await, user.username().await, user.locale()))
                } else {
                    None
                }
            })
            .await?
        {
            let _ = send_template_email(
                "fido-new-key",
                email,
                &locale,
                HashMap::from([
                    ("user".into(), username),
                    ("key".into(), key_name.into()),
                    ("application".into(), registration.application),
                ]),
            )
            .await;
        }

        Ok(())
    }

    pub async fn get_keys(&self) -> Result<Vec<Fido2Key>, Error> {
        let rows = sqlx::query("SELECT id, name, application FROM fido WHERE userid=$1")
            .bind(self.id)
            .fetch_all(&self.database)
            .await?;
        Ok(rows
            .into_iter()
            .filter_map(|row| {
                Some(Fido2Key {
                    id: row.try_get::<i32, _>("id").ok()?,
                    name: row.try_get::<String, _>("name").ok()?,
                    application: row.try_get::<String, _>("application").ok()?,
                })
            })
            .collect())
    }

    pub async fn delete_key(
        &self,
        id: i32,
        #[zbus(connection)] connection: &Connection,
    ) -> Result<(), Error> {
        let row =
            sqlx::query("DELETE FROM fido WHERE userid=$1 AND id=$2 RETURNING name, application")
                .bind(self.id)
                .bind(id)
                .fetch_one(&self.database)
                .await?;
        let key_name = row.try_get::<String, _>("name")?;
        let application = row.try_get::<String, _>("application")?;

        if let Some((email, username, locale)) =
            user_object(connection, &self.database, self.id, async |user| {
                if user.verified().await {
                    Some((user.email().await, user.username().await, user.locale()))
                } else {
                    None
                }
            })
            .await?
        {
            let _ = send_template_email(
                "fido-remove-key",
                email,
                &locale,
                HashMap::from([
                    ("user".into(), username),
                    ("key".into(), key_name),
                    ("application".into(), application),
                ]),
            )
            .await;
        }

        Ok(())
    }
}
