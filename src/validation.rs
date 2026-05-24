use std::sync::LazyLock;
use regex::Regex;

static USERNAME_REGEX: LazyLock<Regex> = LazyLock::new(|| Regex::new(r#"^[A-Za-z0-9 \-_.&,!\[\]{}()"'~`@#$%^*?/\\]+$"#).unwrap());
static EMAIL_REGEX: LazyLock<Regex> = LazyLock::new(|| Regex::new(r#"[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,4}"#).unwrap());

pub fn validate_username(username: &str) -> bool {
    !username.is_empty() && username.len() <= 32 && USERNAME_REGEX.is_match(username)
}

pub fn validate_password(password: &str) -> bool {
    !password.is_empty() && password.len() <= 256
}

pub fn validate_email_address(email: &str) -> bool {
    !email.is_empty() && EMAIL_REGEX.is_match(email)
}