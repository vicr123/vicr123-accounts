use tracing::error;
use zbus::message::Header;
use zbus::names::ErrorName;
use zbus::{DBusError, Message};

#[derive(Debug)]
pub enum Error {
    NoAccount,
    InternalError,
    QueryError(sqlx::Error),
    IncorrectPassword,
    PasswordResetRequired,
    DisabledAccount,
    TwoFactorEnabled,
    TwoFactorDisabled,
    TwoFactorRequired,
    VerificationCodeIncorrect,
    InvalidInput,
    PasswordResetRequestRequired,
    FidoSupportUnavailable,
    AccountEmailNotVerified,
    EmailError(Option<mail_send::Error>),
}

impl From<sqlx::Error> for Error {
    fn from(err: sqlx::Error) -> Self {
        Error::QueryError(err)
    }
}

impl From<mail_send::Error> for Error {
    fn from(value: mail_send::Error) -> Self {
        Error::EmailError(Some(value))
    }
}

impl DBusError for Error {
    fn create_reply(&self, msg: &Header<'_>) -> zbus::Result<Message> {
        if let Some(member) = msg.member() && let Some(path) = msg.path() && let Some(interface) = msg.interface() {
            error!("Reply to {path} {interface}.{member}: {self:?}");
        }
        let name = self.name();
        Message::error(msg, name)?.build(&())
    }

    fn name(&self) -> ErrorName<'_> {
        ErrorName::from_static_str(match self {
            Error::NoAccount => "com.vicr123.accounts.Error.NoAccount",
            Error::InternalError => "com.vicr123.accounts.Error.InternalError",
            Error::QueryError(_) => "com.vicr123.accounts.Error.QueryError",
            Error::IncorrectPassword => "com.vicr123.accounts.Error.IncorrectPassword",
            Error::PasswordResetRequired => "com.vicr123.accounts.Error.PasswordResetRequired",
            Error::DisabledAccount => "com.vicr123.accounts.Error.DisabledAccount",
            Error::TwoFactorEnabled => "com.vicr123.accounts.Error.TwoFactorEnabled",
            Error::TwoFactorDisabled => "com.vicr123.accounts.Error.TwoFactorDisabled",
            Error::TwoFactorRequired => "com.vicr123.accounts.Error.TwoFactorRequired",
            Error::VerificationCodeIncorrect => "com.vicr123.accounts.Error.VerificationCodeIncorrect",
            Error::InvalidInput => "com.vicr123.accounts.Error.InvalidInput",
            Error::PasswordResetRequestRequired => "com.vicr123.accounts.Error.PasswordResetRequestRequired",
            Error::FidoSupportUnavailable => "com.vicr123.accounts.Error.FidoSupportUnavailable",
            Error::AccountEmailNotVerified => "com.vicr123.accounts.Error.AccountEmailNotVerified",
            Error::EmailError(_) => "com.vicr123.accounts.Error.EmailError",
        })
        .unwrap()
    }

    fn description(&self) -> Option<&str> {
        Some(match self {
            Error::NoAccount => "The user account does not exist",
            Error::InternalError => "Internal Error",
            Error::QueryError(_) => "Could not execute the query on the database",
            Error::IncorrectPassword => "The password is incorrect",
            Error::PasswordResetRequired => "A password reset is required",
            Error::DisabledAccount => "The account is disabled",
            Error::TwoFactorEnabled => "Two Factor Authentication is already enabled",
            Error::TwoFactorDisabled => "Two Factor Authentication is already disabled",
            Error::TwoFactorRequired => "Two Factor Authentication is required",
            Error::VerificationCodeIncorrect => "The Verification code is incorrect",
            Error::InvalidInput => "The input is invalid",
            Error::PasswordResetRequestRequired => "A password reset must be requested",
            Error::FidoSupportUnavailable => "FIDO U2F support is not available",
            Error::AccountEmailNotVerified => "Account Email is not verified",
            Error::EmailError(_) => "Unable to send the email",
        })
    }
}
