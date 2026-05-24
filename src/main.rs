use sqlx::postgres::{PgConnectOptions, PgPoolOptions};
use std::error::Error;
use tracing::info;
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

    let bus = zbus::connection::Builder::session()?.build().await?;

    let manager = AccountsManager::new(database);
    bus.object_server()
        .at("/com/vicr123/accounts", manager)
        .await?;

    bus.request_name("com.vicr123.accounts").await?;

    info!("Registered on the bus as com.vicr123.accounts");

    loop {
        std::future::pending::<()>().await;
    }
}
