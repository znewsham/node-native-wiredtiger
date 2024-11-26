
use std::rc::Rc;

use napi::Error;
use crate::glue::connection::*;
use crate::glue::session::InternalSession;

use super::custom_collator::CustomCollator;
use super::custom_extractor::CustomExtractor;
use super::utils::unwrap_or_error;


#[napi]
pub struct Connection {
  connection: Rc<InternalConnection>
}

#[napi]
impl Connection {
  #[napi(constructor)]
  pub fn new(home: String, config: String) -> Result<Self, Error> {
    Ok(Connection { connection: Rc::new(unwrap_or_error(InternalConnection::new(home, config))?) })
  }

  #[napi]
  pub fn close(&self, config: Option<String>) -> Result<(), Error> {
    return unwrap_or_error(self.connection.close(config));
  }

  pub fn open_internal_session(&self, config: Option<String>) -> Result<InternalSession, Error> {
    return Ok(unwrap_or_error(self.connection.open_session(config))?);
  }

  // #[napi]
  // pub fn open_session(&self, env: Env, config: Option<String>) -> Result<WiredTigerSession, Error> {
  //   Ok(WiredTigerSession::new(unwrap_or_error(self.connection.open_session(config))?))
  // }

  #[napi]
  pub fn add_collator(&self, name: String, collator: &mut CustomCollator) -> Result<i32, Error> {
    return unwrap_or_error(self.connection.add_collator(name, collator));
  }

  #[napi]
  pub fn add_extractor(&self, name: String, extractor: &mut CustomExtractor) -> Result<i32, Error> {
    return unwrap_or_error(self.connection.add_extractor(name, extractor));
  }

  pub fn get_connection(&self) -> Rc<InternalConnection> {
    return Rc::clone(&self.connection);
  }
}
