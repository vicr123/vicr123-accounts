use std::collections::HashMap;
use mail_send::mail_builder::MessageBuilder;
use rust_embed::Embed;
use serde::{Deserialize, Serialize};
use crate::error::Error;

#[derive(Embed)]
#[folder = "src/mail"]
struct MailTemplates;

#[derive(Serialize, Deserialize)]
struct MailTemplateMeta {
    subject: String,
    text: String,
    html: String
}

pub struct MailTemplate {
    template_name: String,
    locale: String,
    replacements: HashMap<String, String>,
    meta: MailTemplateMeta
}

impl MailTemplate {
    pub fn open(template_name: &str, locale: &str, replacements: HashMap<String, String>) -> Result<MailTemplate, Error> {
        let meta_file = MailTemplates::get(&format!("{locale}/{template_name}/meta.json")).ok_or(Error::EmailError(None))?;
        let meta: MailTemplateMeta = serde_json::from_slice(&meta_file.data).map_err(|_| Error::EmailError(None))?;

        Ok(MailTemplate {
            template_name: template_name.to_string(),
            locale: locale.to_string(),
            replacements,
            meta
        })
    }

    pub fn subject(&self) -> &str {
        &self.meta.subject
    }

    pub fn text_part(&self) -> Result<String, Error> {
        let text_file = MailTemplates::get(&format!("{}/{}/{}", self.locale, self.template_name, self.meta.text))
            .ok_or(Error::EmailError(None))?;
        let text_data = String::from_utf8_lossy(&text_file.data);
        Ok(self.perform_replacements(text_data.to_string()))
    }

    pub fn html_part(&self) -> Result<String, Error> {
        let html_file = MailTemplates::get(&format!("{}/{}/{}", self.locale, self.template_name, self.meta.html))
            .ok_or(Error::EmailError(None))?;
        let html_data = String::from_utf8_lossy(&html_file.data);
        Ok(self.perform_replacements(html_data.to_string()))
    }

    fn perform_replacements(&self, mut input: String) -> String {
        for (key, value) in &self.replacements {
            input = input.replace(&format!("{{{key}}}"), value);
        }
        input
    }
}