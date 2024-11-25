use std::{collections::HashMap, sync::{Mutex, MutexGuard, OnceLock}};

use napi::{bindgen_prelude::This, Env, Error, JsObject, Ref};

use crate::{external::wiredtiger::WT_SESSION, glue::session::InternalWiredTigerSession};
use super::{connection::WiredTigerConnection, cursor::WiredTigerCursor, utils::unwrap_or_error};


fn get_global_hashmap() -> MutexGuard<'static, HashMap<u64, Ref<()>>> {
  static MAP: OnceLock<Mutex<HashMap<u64, Ref<()>>>> = OnceLock::new();
  MAP.get_or_init(|| Mutex::new(HashMap::new()))
      .lock()
      .expect("Let's hope the lock isn't poisoned")
}

#[napi]
pub struct WiredTigerSession {
  session: InternalWiredTigerSession
}

#[napi]
impl WiredTigerSession {

  #[napi(constructor)]
  pub fn new(env: Env, this: This<JsObject>, conn: &WiredTigerConnection, config: Option<String>) -> Result<WiredTigerSession, Error> {
    let session: InternalWiredTigerSession = conn.open_internal_session(config)?;
    get_global_hashmap().insert(session.get_session_ptr() as u64, env.create_reference(this)?);
    return Ok(WiredTigerSession { session });
  }
  pub fn new_internal(env: Env, this: This<JsObject>, session: InternalWiredTigerSession) -> Result<WiredTigerSession, Error> {
    get_global_hashmap().insert(session.get_session_ptr() as u64, env.create_reference(this)?);
    let ret = WiredTigerSession { session };
    return Ok(ret);
  }

  pub fn get_this(env: Env, session: *mut WT_SESSION) -> Result<Option<This<JsObject>>, Error> {
    let addr = session as u64;
    let map = get_global_hashmap();
    let optional_ref = map.get(&addr);
    match optional_ref {
      None => Ok(None),
      Some(theref) => Ok(Some(env.get_reference_value(theref)?))
    }
  }

  #[napi]
  pub fn create(&self, uri: String, config: Option<String>) -> Result<(), Error> {
    return unwrap_or_error(self.session.create(uri, config));
  }

  #[napi]
  pub fn close(&self, config: Option<String>) -> Result<(), Error> {
    return unwrap_or_error(self.session.close( config));
  }

  #[napi]
  pub fn open_cursor(&self, cursor_spec: String, config: Option<String>) -> Result<WiredTigerCursor, Error> {
    Ok(WiredTigerCursor::new(unwrap_or_error(self.session.open_cursor(cursor_spec, config))?))
  }

  #[napi]
  pub fn begin_transaction(&self, config: Option<String>) -> Result<(), Error> {
    return unwrap_or_error(self.session.begin_transaction( config));
  }

  #[napi]
  pub fn commit_transaction(&self, config: Option<String>) -> Result<(), Error> {
    return unwrap_or_error(self.session.commit_transaction( config));
  }

  #[napi]
  pub fn rollback_transaction(&self, config: Option<String>) -> Result<(), Error> {
    return unwrap_or_error(self.session.rollback_transaction( config));
  }

  #[napi]
  pub fn prepare_transaction(&self, config: Option<String>) -> Result<(), Error> {
    return unwrap_or_error(self.session.prepare_transaction( config));
  }


  #[napi]
  pub fn join(&self, join_cursor: &WiredTigerCursor, ref_cursor: &WiredTigerCursor, config: Option<String>) -> Result<(), Error> {
    return unwrap_or_error(self.session.join(join_cursor.get_internal_cursor(), ref_cursor.get_internal_cursor(), config));
  }
}