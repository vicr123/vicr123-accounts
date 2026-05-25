use base64::Engine;
use base64::prelude::BASE64_STANDARD;
use rand::RngExt;
use rand::distr::{Alphanumeric, SampleString};
use sha3::Sha3_512;
use sqlx::PgPool;
use std::collections::HashMap;
use hmac::{Hmac, KeyInit, Mac};
use sha1::Sha1;
use zvariant::Value;

pub mod account;
pub mod accounts_manager;
mod bus;
pub mod error;
pub mod token_provisioning;
pub mod validation;

pub type VariantMap<'a> = HashMap<String, Value<'a>>;

pub fn generate_hashed_password(password: &str, iterations: u32) -> String {
    let salt = generate_salt();

    let password = password.as_bytes();

    let key = pbkdf2::pbkdf2_hmac_array::<Sha3_512, 512>(password, salt.as_slice(), iterations);
    let key = BASE64_STANDARD.encode(key);
    let salt = BASE64_STANDARD.encode(*salt);
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
    let Ok(salt) = BASE64_STANDARD.decode(parts[3].as_bytes()) else {
        return false;
    };
    let Ok(stored_hash) = BASE64_STANDARD.decode(parts[4].as_bytes()) else {
        return false;
    };

    let password = password.as_bytes();
    let key = pbkdf2::pbkdf2_hmac_array::<Sha3_512, 512>(password, &salt, iterations);

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
    let current_number = chrono::Utc::now().timestamp() / 30;

    otp_key == calculate_otp_key(otp_secret, current_number as u64)
        || otp_key == calculate_otp_key(otp_secret, (current_number + 1) as u64)
        || otp_key == calculate_otp_key(otp_secret, (current_number - 1) as u64)
}

fn calculate_otp_key(shared_key: &str, offset: u64) -> String {
    let mut decoded_key = vec![0_u8; shared_key.len() * 5 / 8];

    let mut next = 0;
    for char in shared_key.to_uppercase().as_bytes() {
        let bits = if char.is_ascii_digit() {
            char - b'2' + 26
        } else {
            char - b'A'
        };

        for i in (0..=4).rev() {
            decoded_key[next / 8] |= (bits >> i & 0b1) << (7 - (next % 8));
            next += 1;
        }
    }

    let decoded_key = decoded_key.iter().map(|b| u8::from_be(*b)).collect::<Vec<_>>();

    let mut hmac = Hmac::<Sha1>::new_from_slice(&decoded_key).unwrap();
    hmac.update(&offset.to_be_bytes());
    let hmac = hmac.finalize().into_bytes();

    let truncation_start = (hmac[hmac.len() - 1] & 0xf) as usize;
    let number = ((hmac[truncation_start] & 0x7f) as u32) << 24
        | (hmac[truncation_start + 1] as u32) << 16
        | (hmac[truncation_start + 2] as u32) << 8
        | hmac[truncation_start + 3] as u32;

    format!("{:06}", number % 1000000)
}

pub fn generate_shared_otp_key() -> String {
    let valid_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
    let mut rng = rand::rng();
    (0..32)
        .map(|_| valid_chars.chars().nth(rng.random_range(0..valid_chars.len())).unwrap())
        .collect()
}

#[test]
fn test_passwords() {
    for i in 0..10 {
        let password =
            Alphanumeric.sample_string(&mut rand::rng(), rand::rng().random_range(8..=32));
        let hashed_password = generate_hashed_password(&password, 10000);
        assert!(verify_hashed_password(&password, &hashed_password));
    }
    for i in 0..10 {
        let password =
            Alphanumeric.sample_string(&mut rand::rng(), rand::rng().random_range(8..=32));
        let password2 =
            Alphanumeric.sample_string(&mut rand::rng(), rand::rng().random_range(8..=32));
        let hashed_password = generate_hashed_password(&password, 10000);
        assert!(!verify_hashed_password(&password2, &hashed_password));
    }
}

#[test]
fn test_calculate_otp_key() {
    // RFC 4226 HOTP test secret: "12345678901234567890" encoded as Base32.
    let shared_key = "GEZDGNBVGY3TQOJQGEZDGNBVGY3TQOJQ";

    assert_eq!(calculate_otp_key(shared_key, 0), "755224");
    assert_eq!(calculate_otp_key(shared_key, 1), "287082");
    assert_eq!(calculate_otp_key(shared_key, 2), "359152");
    assert_eq!(calculate_otp_key(shared_key, 3), "969429");
    assert_eq!(calculate_otp_key(shared_key, 4), "338314");
    assert_eq!(calculate_otp_key(shared_key, 5), "254676");
    assert_eq!(calculate_otp_key(shared_key, 6), "287922");
    assert_eq!(calculate_otp_key(shared_key, 7), "162583");
    assert_eq!(calculate_otp_key(shared_key, 8), "399871");
    assert_eq!(calculate_otp_key(shared_key, 9), "520489");
}