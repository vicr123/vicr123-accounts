use crate::account::Account;
use crate::error::Error;
use sqlx::{PgPool, Row};
use zbus::Connection;
use zbus::object_server::{InterfaceDeref, InterfaceRef};
use zvariant::ObjectPath;

pub async fn user_object<TReturn>(
    bus: &Connection,
    database: &PgPool,
    id: i32,
    callback: impl AsyncFnOnce(InterfaceDeref<'_, Account>) -> TReturn + Send,
) -> Result<TReturn, Error> {
    let path = ObjectPath::try_from(format!("/com/vicr123/accounts/User{id}")).unwrap();

    // Ensure the user account exists
    let count = sqlx::query("SELECT COUNT(*) FROM users WHERE id=$1")
        .bind(id)
        .fetch_one(database)
        .await?
        .try_get::<i64, _>(0)?;
    if count == 0 {
        return Err(Error::NoAccount);
    }

    if let Ok(account) = bus.object_server().interface::<_, Account>(&path).await {
        return Ok(callback(account.get().await).await);
    }

    let account = Account::new(id, database.clone(), path.to_string());
    bus.object_server()
        .at(&path, account)
        .await
        .expect("Failed to register account object");

    let account = bus
        .object_server()
        .interface::<_, Account>(&path)
        .await
        .expect("Failed to retrieve account interface");
    Ok(callback(account.get().await).await)
}
