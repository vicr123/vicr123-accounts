use crate::accounts_manager::user_id_by_username;
use crate::error::Error;
use crate::fido::{fido_creds_for_user, helper_path};
use crate::token_provisioning::{ProvisionResult, TokenProvisioningPurpose};
use crate::{VariantMap, extract_bytes, extract_string};
use base64::Engine;
use base64::prelude::BASE64_STANDARD;
use serde_json::{Map, Value};
use sqlx::{PgPool, Row};
use std::collections::HashMap;
use std::process::Stdio;
use tokio::io::AsyncWriteExt;
use tokio::process::Command;
use zbus::Connection;

pub async fn provision(
    bus: &Connection,
    database: &PgPool,
    options: VariantMap<'_>,
    purpose: TokenProvisioningPurpose,
) -> Result<ProvisionResult<'static>, Error> {
    let username = extract_string(&options, "username").ok_or(Error::InvalidInput)?;
    let application = extract_string(&options, "application").ok_or(Error::InvalidInput)?;

    if application.is_empty() || username.is_empty() {
        return Err(Error::InvalidInput);
    }

    let id = user_id_by_username(database, &username).await?;

    let rp_name = extract_string(&options, "rpname").ok_or(Error::InvalidInput)?;
    let rp_id = extract_string(&options, "rpid").ok_or(Error::InvalidInput)?;

    if options.contains_key("response") {
        let expect_origins = options
            .get("extraOrigins")
            .and_then(|value| match value {
                zvariant::Value::Array(s) => Some(
                    s.iter()
                        .filter_map(|v| match v {
                            zvariant::Value::Str(s) => Some(s.as_str().to_string()),
                            _ => None,
                        })
                        .collect::<Vec<_>>(),
                ),
                _ => None,
            })
            .ok_or(Error::InvalidInput)?;
        let response = extract_bytes(&options, "response").ok_or(Error::InvalidInput)?;
        let preget_options = extract_bytes(&options, "pregetOptions").ok_or(Error::InvalidInput)?;

        let x = String::from_utf8_lossy(&response);

        let mut command = Command::new(helper_path().map_err(|_| Error::FidoSupportUnavailable)?);
        command
            .args(["get", "--rpname", &rp_name, "--rpid", &rp_id])
            .stdin(Stdio::piped())
            .stdout(Stdio::piped());

        let mut payload = Map::new();
        payload.insert(
            "existingCreds".into(),
            Value::from(
                fido_creds_for_user(id, Some(&application), database)
                    .await?
                    .iter()
                    .map(|cred| serde_json::to_value(cred).unwrap())
                    .collect::<Vec<_>>(),
            ),
        );
        payload.insert("expectOrigins".into(), Value::from(expect_origins));
        payload.insert("response".into(), String::from_utf8_lossy(&response).into());
        payload.insert(
            "pregetOptions".into(),
            String::from_utf8_lossy(&preget_options).into(),
        );

        let p = serde_json::to_string(&Value::from(payload.clone())).unwrap();

        let mut child = command.spawn().map_err(|_| Error::FidoSupportUnavailable)?;
        let mut stdin = child.stdin.take().unwrap();
        stdin
            .write_all(&serde_json::to_vec(&Value::from(payload)).unwrap())
            .await
            .map_err(|_| Error::InternalError)?;
        stdin.flush().await.map_err(|_| Error::InternalError)?;
        drop(stdin);

        let output = child.wait_with_output().await;
        let Ok(output) = output else {
            return Err(Error::InternalError);
        };

        let output = String::from_utf8(output.stdout).map_err(|_| Error::InternalError)?;
        let result: Value = serde_json::from_str(&output).map_err(|_| Error::InternalError)?;
        let used_cred = result
            .get("UsedCred")
            .and_then(|v| v.as_str())
            .ok_or(Error::InternalError)?;
        let new_cred = result
            .get("NewCred")
            .and_then(|v| v.as_str())
            .ok_or(Error::InternalError)?;

        sqlx::query("UPDATE fido SET data=$1 WHERE data=$2 AND userid=$3")
            .bind(new_cred.as_bytes())
            .bind(used_cred.as_bytes())
            .bind(id)
            .execute(database)
            .await?;

        Ok(ProvisionResult::Success(id))
    } else {
        let mut command = Command::new(helper_path().map_err(|_| Error::FidoSupportUnavailable)?);
        command
            .args(["preget", "--rpname", &rp_name, "--rpid", &rp_id])
            .stdin(Stdio::piped())
            .stdout(Stdio::piped());

        let mut payload = Map::new();
        payload.insert(
            "existingCreds".into(),
            Value::from(
                fido_creds_for_user(id, Some(&application), database)
                    .await?
                    .iter()
                    .map(|cred| BASE64_STANDARD.encode(serde_json::to_vec(cred).unwrap()))
                    .collect::<Vec<_>>(),
            ),
        );

        let mut child = command.spawn().map_err(|_| Error::FidoSupportUnavailable)?;
        let mut stdin = child.stdin.take().unwrap();
        stdin
            .write_all(&serde_json::to_vec(&Value::from(payload)).unwrap())
            .await
            .map_err(|_| Error::InternalError)?;
        stdin.flush().await.map_err(|_| Error::InternalError)?;
        drop(stdin);

        let output = child.wait_with_output().await;
        let Ok(output) = output else {
            return Err(Error::InternalError);
        };

        let mut challenge = VariantMap::new();
        challenge.insert("options".into(), output.stdout.into());

        Ok(ProvisionResult::Challenge(challenge))
    }
}

pub async fn available(
    database: &PgPool,
    userid: i32,
    application: &str,
    _purpose: TokenProvisioningPurpose,
) -> Result<bool, Error> {
    let result =
        sqlx::query("SELECT COUNT(*) AS count FROM fido WHERE application=$1 AND userid=$2")
            .bind(application)
            .bind(userid)
            .fetch_one(database)
            .await?;
    Ok(result.try_get::<i64, _>("count")? > 0)
}
