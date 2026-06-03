use crate::error::Error;
use jwt_simple::prelude::{Deserialize, Serialize};
use sqlx::{PgPool, Row};
use std::env::VarError;

#[derive(Clone, Debug, Serialize, Deserialize)]
#[serde(rename_all = "PascalCase")]
pub struct SecurityKey {
    pub name: Option<String>,
    pub user_id: i32,
    pub public_key: String,
    pub counter: i32,
    pub cred_type: String,
    pub registration_date: String,
    pub aa_guid: String,
    pub credential_id: String,
    pub user_handle: String,
}

pub fn helper_path() -> Result<String, VarError> {
    std::env::var("FIDO_HELPER_PATH")
}

pub async fn fido_creds_for_user(
    id: i32,
    application: Option<&str>,
    database: &PgPool,
) -> Result<Vec<SecurityKey>, Error> {
    let records = match application {
        None => sqlx::query("SELECT data FROM fido WHERE userid=$1").bind(id),
        Some(application) => {
            sqlx::query("SELECT data FROM fido WHERE userid=$1 AND application=$2")
                .bind(id)
                .bind(application)
        }
    }
    .fetch_all(database)
    .await?;

    Ok(records
        .iter()
        .filter_map(|record| record.try_get::<Vec<u8>, _>("data").ok())
        .filter_map(|data| serde_json::from_slice::<SecurityKey>(&data).ok())
        .collect())
}
