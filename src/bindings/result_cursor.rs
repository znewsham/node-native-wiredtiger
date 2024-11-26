use napi::bindgen_prelude::{Array, This};
use napi::{Env, Error, JsObject, Ref, bindgen_prelude::{ToNapiValue,FromNapiValue}};
use std::borrow::Borrow;
use std::collections::HashMap;
use std::sync::{Mutex, MutexGuard, OnceLock};
use crate::external::wiredtiger::WT_CURSOR;
use crate::glue::cursor::InternalCursor;
use crate::glue::cursor_trait::InternalCursorTrait;
use crate::glue::query_value::QueryValue;

use super::utils::{extract_values, unwrap_or_error};


fn get_global_hashmap() -> MutexGuard<'static, HashMap<u64, Ref<()>>> {
  static MAP: OnceLock<Mutex<HashMap<u64, Ref<()>>>> = OnceLock::new();
  MAP.get_or_init(|| Mutex::new(HashMap::new()))
      .lock()
      .expect("Let's hope the lock isn't poisoned")
}

#[napi(js_name = "ResultCursor")]
pub struct ResultCursor {
  cursor: InternalCursor,
  stored_key: Option<Vec<QueryValue>>,
  stored_value: Option<Vec<QueryValue>>
}

impl Drop for ResultCursor {
  fn drop(&mut self) {
    let addr = self.cursor.cursor as u64;
    get_global_hashmap().remove(&addr);
  }
}

#[napi]
impl ResultCursor {
  pub fn new(cursor: InternalCursor) -> Self {
    return ResultCursor {
      cursor,
      stored_key: None,
      stored_value: None
    };
  }

  #[napi]
  pub fn close(&mut self) -> Result<(), Error> {
    self.stored_key = None;
    self.stored_value = None;
    return unwrap_or_error(self.cursor.close());
  }

  #[napi]
  pub fn next(&self) -> Result<bool, Error> {
    return unwrap_or_error(self.cursor.next());
  }

  pub fn get_this(env: Env, cursor: *mut WT_CURSOR) -> Result<Option<This<JsObject>>, Error> {
    let addr = cursor as u64;
    let map = get_global_hashmap();
    let optional_ref = map.get(&addr);
    match optional_ref {
      None => Ok(None),
      Some(theref) => Ok(Some(env.get_reference_value(theref)?))
    }
  }

  pub fn new_internal(env: Env, cursor: InternalCursor) -> Result<JsObject, Error> {
    let cursor_ptr = cursor.cursor;
    let wt = ResultCursor::new(cursor);
    let ret = unsafe { ResultCursor::to_napi_value(env.raw(), wt) };
    let this = unsafe { JsObject::from_napi_value(env.raw(), ret.unwrap()) }?;
    get_global_hashmap().insert(cursor_ptr as u64, env.create_reference(&this)?);

    return Ok(this);
  }


  #[napi]
  pub fn reset(&mut self) -> Result<(), Error> {
    self.stored_key = None;
    self.stored_value = None;
    Ok(())
  }

  #[napi]
  pub fn set_key(&mut self, key_values: Array) -> Result<(), Error> {
    let mut converted_values = extract_values(key_values, self.cursor.key_formats.borrow(), false)?;
    unwrap_or_error(self.cursor.set_key(&mut converted_values))?;
    self.stored_key = Some(converted_values);
    Ok(())
  }

  #[napi]
  pub fn insert(&self) -> Result<(), Error> {
    return unwrap_or_error(self.cursor.insert());
  }

  #[napi]
  pub fn search_near(&self) -> Result<i32, Error> {
    return unwrap_or_error(self.cursor.search_near());
  }

  #[napi(getter)]
  pub fn key_format(&self) -> Result<String, Error> {
    return unwrap_or_error(self.cursor.get_key_format());
  }

  pub fn get_internal_cursor(&self) -> &InternalCursor {
    return &self.cursor;
  }

}
