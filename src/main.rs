use sqlx::postgres::{PgConnectOptions, PgPoolOptions};
use std::env::VarError;
use std::error::Error;
use std::io::Write;
use std::process::Command;
use std::str::FromStr;
use tracing::{error, info};
use vicr123_accounts::accounts_manager::AccountsManager;

#[tokio::main]
async fn main() -> Result<(), Box<dyn Error>> {
    tracing_subscriber::fmt().with_target(false).init();

    let database = PgPoolOptions::new()
        .max_connections(5)
        .connect_with(
            PgConnectOptions::new()
                .host(
                    &std::env::var("ACCOUNTS_DB_HOSTNAME")
                        .expect("ACCOUNTS_DB_HOSTNAME must be set"),
                )
                .database(
                    &std::env::var("ACCOUNTS_DB_DATABASE")
                        .expect("ACCOUNTS_DB_DATABASE must be set"),
                )
                .username(
                    &std::env::var("ACCOUNTS_DB_USERNAME")
                        .expect("ACCOUNTS_DB_USERNAME must be set"),
                )
                .password(
                    &std::env::var("ACCOUNTS_DB_PASSWORD")
                        .expect("ACCOUNTS_DB_PASSWORD must be set"),
                )
                .port(
                    std::env::var("ACCOUNTS_DB_PORT")
                        .ok()
                        .and_then(|port| port.parse().ok())
                        .unwrap_or(5432),
                ),
        )
        .await?;

    let bus = match std::env::var("DBUS_BUS") {
        Ok(path) if path == "dedicated" => {
            std::fs::create_dir_all("/var/vicr123-accounts")?;
            
            let mut config_file = tempfile::NamedTempFile::new()?;
            config_file.write_all(include_bytes!("dbus-config.conf"))?;

            let command = Box::new(
                Command::new("dbus-daemon")
                    .arg("--nofork")
                    .arg(format!("--config-file={}", config_file.path().display()))
                    .spawn()?,
            );
            Box::leak(command);

            tokio::time::sleep(std::time::Duration::from_secs(1)).await;

            zbus::connection::Builder::address(zbus::address::Address::from_str(
                "unix:path=/var/vicr123-accounts/vicr123-accounts-bus",
            )?)?
            .build()
            .await?
        }
        Ok(path) => {
            zbus::connection::Builder::address(zbus::address::Address::from_str(&path)?)?
                .build()
                .await?
        }
        Err(VarError::NotPresent) => zbus::connection::Builder::session()?.build().await?,
        Err(e) => {
            error!("Invalid value for DBUS_BUS");
            return Err(Box::new(e) as Box<dyn Error>);
        }
    };

    let manager = AccountsManager::new(database, bus.clone());
    bus.object_server()
        .at("/com/vicr123/accounts", manager)
        .await?;

    bus.request_name("com.vicr123.accounts").await?;

    info!("Registered on the bus as com.vicr123.accounts");

    loop {
        std::future::pending::<()>().await;
    }
}
