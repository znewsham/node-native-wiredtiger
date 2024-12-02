use std::ffi::CString;

use napi::{Env, Error, JsString, JsUnknown};

use crate::glue::{cursor::InternalCursor, cursor_trait::InternalCursorTrait, query_value::{ExternalValue, QueryValue}, session::InternalSession};

use super::{connection::Connection, utils::{extract_values_vec, populate_values_vec, unwrap_or_error, values_vec_from_jsstring}};

#[napi]
pub struct MapTable {
  session: InternalSession,
  cursor: InternalCursor,
  is_string_key: bool,
  is_string_value: bool
}


impl Drop for MapTable {
  fn drop(&mut self) {
    let close = self.cursor.close();
    if close.is_err() {
      println!("Couldn't close: {}", close.unwrap_err());
    }

    let close = self.session.close(None);
    if close.is_err() {
      println!("Couldn't close: {}", close.unwrap_err());
    }

  }
}

#[napi]
impl MapTable {
  #[napi(constructor)]
  pub fn new(connection: &Connection, table_name: String, config: String) -> Result<Self, Error> {
    let session = connection.open_internal_session(None)?;
    unwrap_or_error(session.create(format!("table:{}", table_name), Some(config)))?;
    let cursor = unwrap_or_error(session.open_cursor(format!("table:{}", table_name.clone()), None))?;
    return Ok(MapTable {
      session,
      is_string_key: unwrap_or_error(cursor.get_key_format())? == "S",
      is_string_value: unwrap_or_error(cursor.get_value_format())? == "S",
      cursor
    });
  }

  #[napi]
  pub fn list(&mut self, env: Env, prefix: Vec<JsUnknown>) -> Result<Vec<Vec<JsUnknown>>, Error> {
    let mut lower_values = extract_values_vec(env, prefix, self.cursor.get_key_formats(), false)?;
    let mut upper_values = lower_values.clone();
    unwrap_or_error(self.cursor.set_key(&mut lower_values))?;
    unwrap_or_error(self.cursor.bound("action=set,bound=lower,inclusive".to_string()))?;
    let upper_value_0 = &mut upper_values.get_mut(0).unwrap();
    match upper_value_0.get_external_value_mut() {
      ExternalValue::ByteArrayBox(bab) => {
        let len = bab.len();
        let x = bab.get_mut(len - 1).unwrap();
        *x+=1;
      },
      ExternalValue::StrBox(stb) => {
        let mut v: Vec<u8> = Vec::from(stb.as_bytes());
        let len = v.len();
        let x = v.get_mut(len - 1).unwrap();
        *x += 1;
        v.push(0);
        unwrap_or_error(upper_value_0.set_external_str_box(CString::from_vec_with_nul(v).unwrap()))?;
      },
      ExternalValue::UInt(ui) => {
        *ui+=1;
      },
      ExternalValue::ReferenceInternal() | ExternalValue::None() => {}
    }
    unwrap_or_error(self.cursor.set_key(&mut upper_values))?;
    unwrap_or_error(self.cursor.bound("action=set,bound=upper".to_string()))?;
    let mut keys: Vec<Vec<QueryValue>> = Vec::new();
    loop {
      let next = unwrap_or_error(self.cursor.next())?;
      if !next {
        break;
      }
      let key = unwrap_or_error(self.cursor.get_key())?;
      keys.push(key);
    }
    unwrap_or_error(self.cursor.reset())?;
    let mut result: Vec<Vec<JsUnknown>> = Vec::with_capacity(keys.len());
    for k in keys {
      result.push(populate_values_vec(env, k, self.cursor.get_key_formats())?);
    }

    Ok(result)
  }

  #[napi]
  pub fn get(&mut self, env: Env, key: Vec<JsUnknown>) -> Result<Option<Vec<JsUnknown>>, Error> {
    let mut key_values = extract_values_vec(env, key, self.cursor.get_key_formats(), false)?;
    unwrap_or_error(self.cursor.set_key(&mut key_values))?;
    let found = unwrap_or_error(self.cursor.search())?;
    if !found {
      return Ok(None);
    }
    let value_values = unwrap_or_error(self.cursor.get_value())?;
    unwrap_or_error(self.cursor.reset())?;
    return Ok(Some(populate_values_vec(env, value_values, self.cursor.get_value_formats())?));
  }

  #[napi]
  pub fn set(&mut self, env: Env, key: Vec<JsUnknown>, value: Vec<JsUnknown>) -> Result<(), Error> {
    let mut key_values = extract_values_vec(env, key, self.cursor.get_key_formats(), false)?;
    let mut value_values = extract_values_vec(env, value, self.cursor.get_value_formats(), false)?;
    unwrap_or_error(self.cursor.set_key(&mut key_values))?;
    unwrap_or_error(self.cursor.set_value(&mut value_values))?;
    unwrap_or_error(self.cursor.insert())?;
    unwrap_or_error(self.cursor.reset())?;
    Ok(())
  }

  #[napi]
  pub fn delete(&mut self, env: Env, key: Vec<JsUnknown>) -> Result<bool, Error> {
    let mut key_values = extract_values_vec(env, key, self.cursor.get_key_formats(), false)?;
    unwrap_or_error(self.cursor.set_key(&mut key_values))?;
    let found = unwrap_or_error(self.cursor.search())?;
    if !found {
      return Ok(false)
    }
    unwrap_or_error(self.cursor.remove())?;
    unwrap_or_error(self.cursor.reset())?;
    Ok(true)
  }

  #[napi]
  /// A helper function - returns an error if the key_format != "S" or the value_format != "S"
  pub fn get_string_string(&mut self, env: Env, key: JsString) -> Result<Option<JsString>, Error> {
    if !self.is_string_key || !self.is_string_value {
      return Err(Error::from_reason(format!("Invalid key/value format: {}/{}", unwrap_or_error(self.cursor.get_key_format())?, unwrap_or_error(self.cursor.get_value_format())?)));
    }
    let mut key_values = values_vec_from_jsstring(env, key)?;
    unwrap_or_error(self.cursor.set_key(&mut key_values))?;
    let found = unwrap_or_error(self.cursor.search())?;
    if !found {
      return Ok(None);
    }
    let value_values = unwrap_or_error(self.cursor.get_value())?;
    let str = unwrap_or_error(value_values.get(0).unwrap().get_byte_array())?;

    let res = env.create_string_latin1(str)?;
    unwrap_or_error(self.cursor.reset())?;
    return Ok(Some(res));
    // let res = value_out(env, value_values.get(0).unwrap(), &Format { format: 'S', size: 0 });

    // return Ok(Some(populate_values_vec(env, value_values, self.cursor.get_value_formats())?));
  }

  #[napi]
  /// A helper function - returns an error if the key_format != "S" or the value_format != "S"
  pub fn set_string_string(&mut self, env: Env, key: JsString, value: JsString) -> Result<(), Error> {
    if !self.is_string_key || !self.is_string_value {
      return Err(Error::from_reason(format!("Invalid key/value format: {}/{}", unwrap_or_error(self.cursor.get_key_format())?, unwrap_or_error(self.cursor.get_value_format())?)));
    }
    let mut key_values = values_vec_from_jsstring(env, key)?;
    let mut value_values = values_vec_from_jsstring(env, value)?;
    unwrap_or_error(self.cursor.set_key(&mut key_values))?;
    unwrap_or_error(self.cursor.set_value(&mut value_values))?;
    unwrap_or_error(self.cursor.insert())?;
    unwrap_or_error(self.cursor.reset())?;
    Ok(())
  }

  #[napi]
  /// A helper function - returns an error if the key_format != "S"
  pub fn delete_string(&mut self, env: Env, key: JsString) -> Result<bool, Error> {
    if self.is_string_key {
      return Err(Error::from_reason(format!("Invalid key format: {}", unwrap_or_error(self.cursor.get_key_format())?)));
    }
    let mut key_values = values_vec_from_jsstring(env, key)?;
    unwrap_or_error(self.cursor.set_key(&mut key_values))?;
    let found = unwrap_or_error(self.cursor.search())?;
    if !found {
      return Ok(false)
    }
    unwrap_or_error(self.cursor.remove())?;
    unwrap_or_error(self.cursor.reset())?;
    Ok(true)
  }
}
