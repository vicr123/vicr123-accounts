use sqlx::PgPool;
use zbus::interface;

pub struct Account {
    id: i32,
    pool: PgPool,
}

impl Account {
    pub fn new(id: i32, pool: PgPool) -> Self {
        Self { id, pool }
    }
}

#[interface(name = "com.vicr123.accounts.User")]
impl Account {
    #[zbus(property)]
    async fn id(&self) -> i32 {
        self.id
    }
}