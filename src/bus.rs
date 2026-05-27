use std::sync::{Arc, LazyLock};
use crate::error::Error;
use sqlx::{PgPool, Row};
use tokio::sync::RwLock;
use zbus::Connection;
use zbus::object_server::{InterfaceDeref, InterfaceRef};
use zvariant::ObjectPath;
use crate::account::register_account_interfaces;
use crate::account::user::User;
use crate::mail_message::MailMessage;

static MAIL_MESSAGE_COUNTER: LazyLock<Arc<RwLock<u32>>> = LazyLock::new(|| Arc::new(RwLock::new(0)));

pub async fn user_object<TReturn>(
    bus: &Connection,
    database: &PgPool,
    id: i32,
    callback: impl AsyncFnOnce(InterfaceDeref<'_, User>) -> TReturn + Send,
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

    if let Ok(account) = bus.object_server().interface::<_, User>(&path).await {
        return Ok(callback(account.get().await).await);
    }

    register_account_interfaces(&path, id, bus, database).await?;

    let user_interface = bus
        .object_server()
        .interface::<_, User>(&path)
        .await
        .expect("Failed to retrieve account interface");
    Ok(callback(user_interface.get().await).await)
}

pub async fn create_mail_message<'a>(bus: &Connection, to: &str) -> ObjectPath<'a> {
    let mut counter = MAIL_MESSAGE_COUNTER.write().await;
    *counter += 1;

    let path = ObjectPath::try_from(format!("/com/vicr123/accounts/mail/Message{counter}")).unwrap();

    let mail_message = MailMessage::new(path.clone().into(), to);
    bus.object_server().at(&path, mail_message).await.unwrap();

    path
}