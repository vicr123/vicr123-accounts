use crate::accounts_manager::user_id_by_username;
use crate::error::Error;
use crate::token_provisioning::{ProvisionResult, TokenProvisioningPurpose};
use crate::{VariantMap, is_valid_otp_key, verify_hashed_password};
use sqlx::{PgPool, Row};
use zbus::Connection;
use zvariant::Value;
use crate::bus::user_object;

pub async fn provision(
    bus: &Connection,
    database: &PgPool,
    options: VariantMap<'_>,
    purpose: TokenProvisioningPurpose,
) -> Result<ProvisionResult<'static>, Error> {
    let Some(username) = options.get("username").and_then(|username| match username {
        Value::Str(s) => Some(s.to_string()),
        _ => None,
    }) else {
        return Err(Error::InvalidInput);
    };

    let Some(mut password) = options.get("password").and_then(|password| match password {
        Value::Str(s) => Some(s.to_string()),
        _ => None,
    }) else {
        return Err(Error::InvalidInput);
    };

    let id = user_id_by_username(database, &username).await?;
    let user_record = sqlx::query("SELECT * FROM users WHERE id = $1")
        .bind(id)
        .fetch_one(database)
        .await?;

    let password_hash = user_record.try_get::<String, _>("password")?;
    if password_hash.starts_with("!") {
        return Err(Error::DisabledAccount);
    }

    // Now check for password resets
    let have_password_reset = {
        let mut have_password_reset = false;
        let resets = sqlx::query("SELECT * FROM passwordresets WHERE userid = $1")
            .bind(id)
            .fetch_all(database)
            .await?;
        for reset in resets {
            if reset.try_get::<i64, _>("expiry")? <= chrono::Utc::now().timestamp() {
                continue;
            }
            have_password_reset = true;
            let temporary_password = reset.try_get::<String, _>("temporarypassword")?;
            if verify_hashed_password(&password, &temporary_password) {
                let Some(new_password) =
                    options
                        .get("newPassword")
                        .and_then(|new_password| match new_password {
                            Value::Str(s) => Some(s.to_string()),
                            _ => None,
                        })
                else {
                    return Err(Error::PasswordResetRequired);
                };

                user_object(bus, database, id, async |account| -> Result<(), Error> {
                    account.set_password(&new_password).await?;
                    Ok(())
                }).await??;

                // The password has been reset, so delete all the password resets for this user
                let _ = sqlx::query("DELETE FROM passwordresets WHERE userid = $1")
                    .bind(id)
                    .execute(database)
                    .await;

                password = new_password;
            }
        }
        have_password_reset
    };

    if password_hash == "x" {
        // Check if there is already a pending password reset.
        // If there is already a pending password reset, tell the user that their password is incorrect instead.
        return Err(if have_password_reset {
            Error::IncorrectPassword
        } else {
            Error::PasswordResetRequestRequired
        });
    }

    if !verify_hashed_password(&password, &password_hash) {
        return Err(Error::IncorrectPassword);
    }

    // Check TOTP if we're doing this to log in
    if purpose == TokenProvisioningPurpose::Login {
        match sqlx::query("SELECT * FROM otp WHERE userId = $1")
            .bind(id)
            .fetch_one(database)
            .await
        {
            Ok(row) => {
                if row.try_get::<bool, _>("enabled")? {
                    let Some(otp_token) =
                        options
                            .get("otpToken")
                            .and_then(|otp_token| match otp_token {
                                Value::Str(s) => Some(s.to_string()),
                                _ => None,
                            })
                    else {
                        return Err(Error::TwoFactorRequired);
                    };

                    let otp_secret = row.try_get::<String, _>("otpkey")?;
                    if !is_valid_otp_key(&otp_token, &otp_secret) {
                        // Check the backup keys
                        let backup_keys = sqlx::query("SELECT * FROM otpbackup WHERE userid = $1")
                            .bind(id)
                            .fetch_all(database)
                            .await?;
                        let Some(valid_backup_key) = backup_keys.iter().find_map(|key| {
                            if key.try_get::<bool, _>("used").ok()? {
                                None
                            } else {
                                let backup_key = key.try_get::<String, _>("backupkey").ok()?;
                                if backup_key == otp_token {
                                    Some(backup_key)
                                } else {
                                    None
                                }
                            }
                        }) else {
                            return Err(Error::TwoFactorRequired)
                        };

                        // Mark the valid backup key as used
                        sqlx::query("UPDATE otpbackup SET used = true WHERE backupkey = $1 AND userid = $2")
                            .bind(valid_backup_key)
                            .bind(id)
                            .execute(database)
                            .await?;
                    }
                }
            }
            Err(sqlx::Error::RowNotFound) => {
                // No TOTP required to log in
            }
            Err(e) => return Err(e.into()),
        }
    }

    Ok(ProvisionResult::Success(id))
}
