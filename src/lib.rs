use std::collections::HashMap;
use base64::Engine;
use base64::prelude::BASE64_STANDARD;
use rand::distr::{Alphanumeric, SampleString};
use rand::RngExt;
use sha3::Sha3_512;
use sqlx::PgPool;
use zvariant::Value;

pub mod accounts_manager;
pub mod error;
pub mod validation;
pub mod account;
pub mod token_provisioning;
mod bus;

pub type VariantMap<'a> = HashMap<String, Value<'a>>;

pub fn generate_hashed_password(password: &str, iterations: u32) -> String {
    let salt = generate_salt();
    let salt = BASE64_STANDARD.encode(*salt);

    let password = password.as_bytes();
    let salt_bytes = salt.as_bytes();

    let mut key = [0u8; 20];
    pbkdf2::pbkdf2_hmac::<Sha3_512>(password, salt_bytes, iterations, &mut key);
    let key = BASE64_STANDARD.encode(key);
    format!("PBKDF2.SHA3_512.{iterations}.{salt}.{key}")
}

pub fn verify_hashed_password(password: &str, hashed_password: &str) -> bool {
    let parts = hashed_password.split(".").collect::<Vec<_>>();
    if parts.len() != 5 {
        return false;
    }
    if parts[0] != "PBKDF2" {
        return false;
    }
    if parts[1] != "SHA3_512" {
        return false;
    }
    let Ok(iterations) = parts[2].parse::<u32>() else {
        return false;
    };
    let salt = parts[3].as_bytes();
    let Ok(stored_hash) = BASE64_STANDARD.decode(parts[4].as_bytes()) else {
        return false;
    };

    let password = password.as_bytes();
    let mut key = [0u8; 20];
    pbkdf2::pbkdf2_hmac::<Sha3_512>(password, salt, iterations, &mut key);

    if key != *stored_hash {
        return false;
    }

    true
}

pub fn generate_salt() -> Box<[u8; 64]> {
    Box::new(rand::rng().random())
}

pub fn send_verification_email(pool: PgPool, user_id: i32) -> bool {
    // TODO
    true
}

pub fn is_valid_otp_key(otp_key: &str, otp_secret: &str) -> bool {
    // TODO
    true
}

#[test]
fn test_passwords() {
    for i in 0..10 {
        let password = Alphanumeric.sample_string(&mut rand::rng(), rand::rng().random_range(8..=32));
        let hashed_password = generate_hashed_password(&password, 10000);
        assert!(verify_hashed_password(&password, &hashed_password));
    }
    for i in 0..10 {
        let password = Alphanumeric.sample_string(&mut rand::rng(), rand::rng().random_range(8..=32));
        let password2 = Alphanumeric.sample_string(&mut rand::rng(), rand::rng().random_range(8..=32));
        let hashed_password = generate_hashed_password(&password, 10000);
        assert!(!verify_hashed_password(&password2, &hashed_password));
    }
}