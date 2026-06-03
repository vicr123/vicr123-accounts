use crate::bus::user_object;
use crate::error::Error;
use crate::{generate_shared_otp_key, is_valid_otp_key, send_template_email};
use rand::Rng;
use serde::{Deserialize, Serialize};
use sqlx::{PgPool, Row};
use std::collections::HashMap;
use zbus::object_server::SignalEmitter;
use zbus::{Connection, interface};
use zvariant::{OwnedValue, Type, Value};

#[derive(Debug, Clone, Serialize, Deserialize, Type, Value, OwnedValue)]
pub struct OwnedOtpBackupKeys {
    key: String,
    used: bool,
}

pub struct TwoFactor {
    id: i32,

    enabled: bool,
    secret_key: String,
    backup_keys: Vec<OwnedOtpBackupKeys>,

    database: PgPool,
}

impl TwoFactor {
    pub async fn new(id: i32, database: PgPool) -> Result<Self, Error> {
        let (enabled, secret_key) = match sqlx::query("SELECT * FROM otp WHERE userid=$1")
            .bind(id)
            .fetch_one(&database)
            .await
        {
            Ok(otp_status) => (
                otp_status.try_get("enabled")?,
                otp_status.try_get("otpkey")?,
            ),
            Err(sqlx::Error::RowNotFound) => (false, String::new()),
            Err(e) => return Err(e.into()),
        };

        let backup_keys = read_backup_keys(&database, id).await?;

        Ok(Self {
            id,
            enabled,
            secret_key,
            backup_keys,
            database,
        })
    }

    async fn regenerate_backup_keys_internal(
        &mut self,
        emitter: &SignalEmitter<'_>,
    ) -> Result<(), Error> {
        if !self.enabled {
            // 2FA should be enabled first
            return Err(Error::TwoFactorDisabled);
        }

        let mut transaction = self.database.begin().await?;

        sqlx::query("DELETE FROM otpbackup WHERE userid = $1")
            .bind(self.id)
            .execute(&mut *transaction)
            .await?;
        let backup_keys = (0..10)
            .map(|_| {
                let backup = rand::rng().next_u32();
                OwnedOtpBackupKeys {
                    key: (0..4)
                        .map(|i| format!("{:03}", (backup >> (i * 8)) as u8))
                        .collect(),
                    used: false,
                }
            })
            .collect::<Vec<_>>();

        for key in &backup_keys {
            sqlx::query("INSERT INTO otpbackup(userid, backupkey, used) VALUES($1, $2, false)")
                .bind(self.id)
                .bind(&key.key)
                .execute(&mut *transaction)
                .await?;
        }

        transaction.commit().await?;

        self.backup_keys = backup_keys.clone();
        self.backup_keys_changed(emitter).await.unwrap();
        emitter.backup_keys_changed_2(backup_keys).await.unwrap();

        Ok(())
    }
}

#[interface(name = "com.vicr123.accounts.TwoFactor")]
impl TwoFactor {
    #[zbus(property)]
    pub async fn two_factor_enabled(&self) -> bool {
        self.enabled
    }

    #[zbus(property)]
    pub async fn secret_key(&self) -> String {
        self.secret_key.clone()
    }

    #[zbus(property)]
    pub async fn backup_keys(&self) -> Vec<OwnedOtpBackupKeys> {
        self.backup_keys.clone()
    }

    pub async fn generate_two_factor_key(
        &mut self,
        #[zbus(signal_emitter)] emitter: SignalEmitter<'_>,
    ) -> Result<String, Error> {
        if self.enabled {
            return Err(Error::TwoFactorEnabled);
        }

        let new_key = generate_shared_otp_key();
        sqlx::query(
            "INSERT INTO otp(userid, otpkey, enabled)
                     VALUES($1, $2, $3)
                     ON CONFLICT
                         ON CONSTRAINT otp_pkey
                             DO UPDATE
                                 SET otpkey=$2, enabled=$3 WHERE otp.userid=$1",
        )
        .bind(self.id)
        .bind(&new_key)
        .bind(false)
        .execute(&self.database)
        .await?;

        self.secret_key = new_key.clone();
        self.secret_key_changed(&emitter).await.unwrap();
        emitter.secret_key_changed_2(&new_key).await.unwrap();

        Ok(new_key)
    }

    pub async fn enable_two_factor_authentication(
        &mut self,
        otp_key: &str,
        #[zbus(signal_emitter)] emitter: SignalEmitter<'_>,
        #[zbus(connection)] connection: &Connection,
    ) -> Result<(), Error> {
        if self.enabled {
            // 2FA should be disabled first
            return Err(Error::TwoFactorEnabled);
        }

        if !is_valid_otp_key(otp_key, &self.secret_key) {
            return Err(Error::TwoFactorRequired);
        }

        sqlx::query("UPDATE otp SET enabled=true WHERE otp.userid=$1")
            .bind(self.id)
            .execute(&self.database)
            .await?;

        self.enabled = true;
        self.two_factor_enabled_changed(&emitter).await.unwrap();
        emitter.two_factor_enabled_changed_2(true).await.unwrap();

        self.regenerate_backup_keys_internal(&emitter).await?;

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
                "2fa-on",
                email,
                &locale,
                HashMap::from([("user".into(), username)]),
            )
            .await;
        }

        Ok(())
    }

    pub async fn disable_two_factor_authentication(
        &mut self,
        #[zbus(signal_emitter)] emitter: SignalEmitter<'_>,
        #[zbus(connection)] connection: &Connection,
    ) -> Result<(), Error> {
        if !self.enabled {
            // 2FA should be enabled first
            return Err(Error::TwoFactorDisabled);
        }

        sqlx::query("UPDATE otp SET enabled=false WHERE otp.userid=$1")
            .bind(self.id)
            .execute(&self.database)
            .await?;

        self.enabled = false;
        self.two_factor_enabled_changed(&emitter).await.unwrap();
        emitter.two_factor_enabled_changed_2(false).await.unwrap();


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
                "2fa-off",
                email,
                &locale,
                HashMap::from([("user".into(), username)]),
            )
                .await;
        }

        Ok(())
    }

    pub async fn regenerate_backup_keys(
        &mut self,
        #[zbus(signal_emitter)] emitter: SignalEmitter<'_>,
        #[zbus(connection)] connection: &Connection,
    ) -> Result<(), Error> {
        self.regenerate_backup_keys_internal(&emitter).await?;


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
                "2fa-recovery-regenerated",
                email,
                &locale,
                HashMap::from([("user".into(), username)]),
            )
                .await;
        }

        Ok(())
    }

    #[zbus(signal, name = "SecretKeyChanged")]
    async fn secret_key_changed_2(
        emitter: &SignalEmitter<'_>,
        new_secret_key: &str,
    ) -> zbus::Result<()>;

    #[zbus(signal, name = "TwoFactorEnabledChanged")]
    async fn two_factor_enabled_changed_2(
        emitter: &SignalEmitter<'_>,
        enabled: bool,
    ) -> zbus::Result<()>;

    #[zbus(signal, name = "BackupKeysChanged")]
    async fn backup_keys_changed_2(
        emitter: &SignalEmitter<'_>,
        backup_keys: Vec<OwnedOtpBackupKeys>,
    ) -> zbus::Result<()>;
}

async fn read_backup_keys(database: &PgPool, id: i32) -> Result<Vec<OwnedOtpBackupKeys>, Error> {
    Ok(sqlx::query("SELECT * FROM otpbackup WHERE userid=$1")
        .bind(id)
        .fetch_all(database)
        .await?
        .iter()
        .filter_map(|row| {
            let key = row.try_get("backupkey").ok()?;
            let used = row.try_get("used").ok()?;
            Some(OwnedOtpBackupKeys { key, used })
        })
        .collect())
}
