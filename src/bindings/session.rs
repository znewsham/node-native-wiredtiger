use std::{collections::HashMap, sync::{Mutex, MutexGuard, OnceLock}};

use napi::{bindgen_prelude::{FromNapiValue, This, ToNapiValue}, Env, Error, JsObject, Ref};

use crate::{external::wiredtiger::WT_SESSION, glue::session::InternalSession};
use super::{connection::Connection, cursor::Cursor, utils::unwrap_or_error};


fn get_global_hashmap() -> MutexGuard<'static, HashMap<u64, Ref<()>>> {
  static MAP: OnceLock<Mutex<HashMap<u64, Ref<()>>>> = OnceLock::new();
  MAP.get_or_init(|| Mutex::new(HashMap::new()))
      .lock()
      .expect("Let's hope the lock isn't poisoned")
}

#[napi]
pub struct Session {
  session: InternalSession
}

impl Drop for Session {
  fn drop(&mut self) {
    let addr = self.session.get_session_ptr() as u64;
    get_global_hashmap().remove(&addr);
  }
}

#[napi]
impl Session {

  #[napi(constructor)]
  pub fn new(env: Env, this: This<JsObject>, conn: &Connection, config: Option<String>) -> Result<Session, Error> {
    let session: InternalSession = conn.open_internal_session(config)?;
    get_global_hashmap().insert(session.get_session_ptr() as u64, env.create_reference(this)?);
    return Ok(Session { session });
  }

  pub fn new_internal(env: Env, session: InternalSession) -> Result<JsObject, Error> {
    let session_ptr = session.get_session_ptr();
    let wt = Session { session };
    let ret = unsafe { Session::to_napi_value(env.raw(), wt) };
    let this = unsafe { JsObject::from_napi_value(env.raw(), ret.unwrap()) }?;
    get_global_hashmap().insert(session_ptr as u64, env.create_reference(&this)?);

    return Ok(this);
  }

  pub fn get_internal_session(&self) -> &InternalSession {
    return &self.session;
  }

  pub fn get_session_ptr(&self) -> *mut WT_SESSION {
    return self.session.get_session_ptr();
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
  pub fn open_cursor(&self, cursor_spec: String, config: Option<String>) -> Result<Cursor, Error> {
    Ok(Cursor::new(unwrap_or_error(self.session.open_cursor(cursor_spec, config))?))
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
  pub fn join(&self, join_cursor: &Cursor, ref_cursor: &Cursor, config: Option<String>) -> Result<(), Error> {
    return unwrap_or_error(self.session.join(join_cursor.get_internal_cursor(), ref_cursor.get_internal_cursor(), config));
  }
}
