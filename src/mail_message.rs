use crate::error::Error;
use crate::send_mail_message;
use mail_send::mail_builder::MessageBuilder;
use zbus::{interface, Connection};
use zvariant::OwnedObjectPath;

pub struct MailMessage {
    to: String,
    subject: String,
    from_address: String,
    from: String,
    text_content: String,
    html_content: String,

    path: OwnedObjectPath,
}

impl MailMessage {
    pub fn new(path: OwnedObjectPath, to: &str) -> Self {
        Self {
            to: to.to_string(),
            html_content: Default::default(),
            text_content: Default::default(),
            subject: Default::default(),
            from: Default::default(),
            from_address: Default::default(),
            path,
        }
    }
}

#[interface(name = "com.vicr123.accounts.MailMessage")]
impl MailMessage {
    #[zbus(property)]
    pub fn subject(&self) -> String {
        self.subject.clone()
    }

    #[zbus(property)]
    pub fn set_subject(&mut self, subject: String) {
        self.subject = subject;
    }

    #[zbus(property)]
    pub fn from(&self) -> (String, String) {
        (self.from_address.clone(), self.from.clone())
    }

    #[zbus(property)]
    pub fn set_from(&mut self, from: (String, String)) {
        let (from_address, from) = from;
        self.from_address = from_address;
        self.from = from;
    }

    #[zbus(property)]
    pub fn html_content(&self) -> String {
        self.html_content.clone()
    }

    #[zbus(property)]
    pub fn set_html_content(&mut self, html_content: String) {
        self.html_content = html_content;
    }

    #[zbus(property)]
    pub fn text_content(&self) -> String {
        self.text_content.clone()
    }

    #[zbus(property)]
    pub fn set_text_content(&mut self, text_content: String) {
        self.text_content = text_content;
    }

    pub async fn send(&self, #[zbus(connection)] connection: &Connection) -> Result<(), Error> {
        let message = MessageBuilder::new()
            .from((self.from.clone(), self.from_address.clone()))
            .to(self.to.clone())
            .subject(self.subject.clone())
            .html_body(self.html_content.clone())
            .text_body(self.text_content.clone());

        send_mail_message(message).await?;

        let path = self.path.clone();
        let connection = connection.clone();

        tokio::spawn(async move {
            let _ = connection
                .object_server()
                .remove::<Self, _>(&path)
                .await;
        });
        
        Ok(())
    }

    pub async fn discard(&self, #[zbus(connection)] connection: &Connection) {
        let path = self.path.clone();
        let connection = connection.clone();

        tokio::spawn(async move {
            let _ = connection
                .object_server()
                .remove::<Self, _>(&path)
                .await;
        });
    }
}
