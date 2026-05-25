use crate::account::two_factor::TwoFactor;
use crate::account::user::User;
use crate::error::Error;
use sqlx::PgPool;
use zbus::Connection;
use zvariant::ObjectPath;

pub mod two_factor;
pub mod user;

pub async fn register_account_interfaces(
    path: &ObjectPath<'_>,
    id: i32,
    bus: &Connection,
    database: &PgPool,
) -> Result<(), Error> {
    let user_interface = User::new(id, database.clone(), path.to_string()).await?;
    bus.object_server()
        .at(path, user_interface)
        .await
        .expect("Failed to register user interface");

    let two_factor_interface = TwoFactor::new(id, database.clone(), path.to_string()).await?;
    bus.object_server()
        .at(path, two_factor_interface)
        .await
        .expect("Failed to register two factor interface");

    Ok(())
}
