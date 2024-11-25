
use std::rc::Rc;

use napi::Error;
use crate::glue::connection::*;
use crate::glue::session::InternalWiredTigerSession;

use super::custom_collator::WiredTigerCustomCollator;
use super::utils::unwrap_or_error;


#[napi(js_name = "WiredTigerConnection")]
pub struct WiredTigerConnection {
  connection: Rc<InternalWiredTigerConnection>
}

#[napi]
impl WiredTigerConnection {
  #[napi(constructor)]
  pub fn new(home: String, config: String) -> Result<Self, Error> {
    Ok(WiredTigerConnection { connection: Rc::new(unwrap_or_error(InternalWiredTigerConnection::new(home, config))?) })
  }

  #[napi]
  pub fn close(&self, config: Option<String>) -> Result<(), Error> {
    return unwrap_or_error(self.connection.close(config));
  }

  pub fn open_internal_session(&self, config: Option<String>) -> Result<InternalWiredTigerSession, Error> {
    return Ok(unwrap_or_error(self.connection.open_session(config))?);
  }

  // #[napi]
  // pub fn open_session(&self, env: Env, config: Option<String>) -> Result<WiredTigerSession, Error> {
  //   Ok(WiredTigerSession::new(unwrap_or_error(self.connection.open_session(config))?))
  // }

  #[napi]
  pub fn add_collator(&self, name: String, collator: &mut WiredTigerCustomCollator) -> Result<i32, Error> {
    return unwrap_or_error(self.connection.add_collator(name, collator));
  }

  pub fn get_connection(&self) -> Rc<InternalWiredTigerConnection> {
    return Rc::clone(&self.connection);
  }
}
