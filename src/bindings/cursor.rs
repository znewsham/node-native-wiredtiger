use napi::bindgen_prelude::Array;
use napi::{Env, Error, JsUnknown};
use std::borrow::Borrow;
use crate::external::wiredtiger::WT_NOTFOUND;
use crate::glue::cursor::InternalWiredTigerCursor;
use crate::glue::error::{ErrorDomain, GlueError};
use crate::glue::query_value::{Format, QueryValue};
use crate::glue::utils::parse_format;

use super::utils::{error_from_glue_error, extract_values, populate_values, unwrap_or_error};


#[napi(js_name = "WiredTigerCursor")]
pub struct WiredTigerCursor {
  cursor: InternalWiredTigerCursor,
  stored_key: Option<Vec<QueryValue>>,
  stored_value: Option<Vec<QueryValue>>
}

#[napi]
impl WiredTigerCursor {
  pub fn new(cursor: InternalWiredTigerCursor) -> Self {
    return WiredTigerCursor {
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

  fn get_next_prev(&self, env: Env, key_format_str: Option<String>, value_format_str: Option<String>, result: Result<bool, GlueError>) -> Result<JsUnknown, Error> {
    match result {
      Err(err) => {
        if err.error_domain == ErrorDomain::WIREDTIGER && err.wiredtiger_error == WT_NOTFOUND {
          return Ok(env.get_null()?.into_unknown())
        }
        return Err(error_from_glue_error(err))
      },
      Ok(res) => if res == false { return Ok(env.get_null()?.into_unknown()) }
    }

    let key_formats: &Vec<Format>;
    let mut parsed_key_formats: Vec<Format>;
    match key_format_str {
      Some(str) => {
        parsed_key_formats = Vec::new();
        unwrap_or_error(parse_format(str, &mut parsed_key_formats))?;
        self.check_formats(&parsed_key_formats, self.cursor.key_formats.borrow())?;
        key_formats = &parsed_key_formats;
      },
      None => {
        key_formats = self.cursor.key_formats.borrow();
      }
    }

    let value_formats: &Vec<Format>;
    let mut parsed_value_formats: Vec<Format>;
    match value_format_str {
      Some(str) => {
        parsed_value_formats = Vec::new();
        unwrap_or_error(parse_format(str, &mut parsed_value_formats))?;
        self.check_formats(&parsed_value_formats, self.cursor.value_formats.borrow())?;
        value_formats = &parsed_value_formats;
      },
      None => {
        value_formats = self.cursor.value_formats.borrow();
      }
    }
    let key = unwrap_or_error(self.cursor.get_key())?;
    let value = unwrap_or_error(self.cursor.get_value())?;
    let populated_key = populate_values(env, key, key_formats)?;
    let populated_value = populate_values(env, value, value_formats)?;
    let mut array = env.create_array(2)?;
    array.set(0, populated_key)?;
    array.set(1, populated_value)?;
    Ok(array.coerce_to_object()?.into_unknown())
  }

  #[napi(ts_return_type = "unknown[] | null")]
  pub fn get_next(&self, env: Env, key_format_str: Option<String>, value_format_str: Option<String>) -> Result<JsUnknown, Error> {
    let result = self.cursor.next();
    return self.get_next_prev(env, key_format_str, value_format_str, result);
  }

  #[napi]
  pub fn prev(&self) -> Result<bool, Error> {
    return unwrap_or_error(self.cursor.prev());
  }

  #[napi(ts_return_type = "unknown[] | null")]
  pub fn get_prev(&self, env: Env, key_format_str: Option<String>, value_format_str: Option<String>) -> Result<JsUnknown, Error> {
    let result = self.cursor.prev();
    return self.get_next_prev(env, key_format_str, value_format_str, result);
  }

  #[napi]
  pub fn reset(&mut self) -> Result<(), Error> {
    self.stored_key = None;
    self.stored_value = None;
    return unwrap_or_error(self.cursor.reset());
  }

  #[napi]
  pub fn set_key(&mut self, key_values: Array) -> Result<(), Error> {
    let mut converted_values = extract_values(key_values, self.cursor.key_formats.borrow())?;
    unwrap_or_error(self.cursor.set_key(&mut converted_values))?;
    self.stored_key = Some(converted_values);
    Ok(())
  }

  fn check_formats<>(&self, parsed_formats: &Vec<Format>, default_formats: &Vec<Format>) -> Result<(), Error> {
    if parsed_formats.len() != default_formats.len() {
      let err = Error::from_reason(format!("Format mismatch provided:{} != {}", parsed_formats.len(), default_formats.len()));
      return Err(err);
    }
    Ok(())
  }

  #[napi]
  pub fn get_key(&self, env: Env, format_str: Option<String>) -> Result<Array, Error> {
    let values: Vec<QueryValue> = unwrap_or_error(self.cursor.get_key())?;
    let formats: &Vec<Format>;
    let mut parsed_formats: Vec<Format>;
    match format_str {
      Some(str) => {
        parsed_formats = Vec::new();
        unwrap_or_error(parse_format(str, &mut parsed_formats))?;
        self.check_formats(&parsed_formats, self.cursor.key_formats.borrow())?;
        formats = &parsed_formats;
      },
      None => {
        formats = self.cursor.key_formats.borrow();
      }
    }

    return populate_values(env, values, formats)
  }

  #[napi]
  pub fn set_value(&mut self, value_values: Array) -> Result<(), Error> {
    let mut converted_values = extract_values(value_values, self.cursor.value_formats.borrow())?;
    unwrap_or_error(self.cursor.set_value(&mut converted_values))?;
    self.stored_value = Some(converted_values);
    Ok(())
  }

  #[napi]
  pub fn get_value(&self, env: Env, format_str: Option<String>) -> Result<Array, Error> {
    let values: Vec<QueryValue> = unwrap_or_error(self.cursor.get_value())?;
    let formats: &Vec<Format>;
    let mut parsed_formats: Vec<Format>;
    match format_str {
      Some(str) => {
        parsed_formats = Vec::new();
        unwrap_or_error(parse_format(str, &mut parsed_formats))?;
        self.check_formats(&parsed_formats, self.cursor.value_formats.borrow())?;
        formats = &parsed_formats;
      },
      None => {
        formats = self.cursor.key_formats.borrow();
      }
    }
    return populate_values(env, values, formats)
  }

  #[napi]
  pub fn insert(&self) -> Result<(), Error> {
    return unwrap_or_error(self.cursor.insert());
  }

  #[napi]
  pub fn update(&self) -> Result<(), Error> {
    return unwrap_or_error(self.cursor.update());
  }

  #[napi]
  pub fn remove(&self) -> Result<(), Error> {
    return unwrap_or_error(self.cursor.remove());
  }

  #[napi]
  pub fn search(&self) -> Result<(), Error> {
    return unwrap_or_error(self.cursor.search());
  }

  #[napi]
  pub fn search_near(&self) -> Result<i32, Error> {
    return unwrap_or_error(self.cursor.search_near());
  }

  #[napi]
  pub fn compare(&self, cursor: &WiredTigerCursor) -> Result<i32, Error> {
    return unwrap_or_error(self.cursor.compare(cursor.cursor.borrow()));
  }

  #[napi]
  pub fn equals(&self, cursor: &WiredTigerCursor) -> Result<bool, Error> {
    return unwrap_or_error(self.cursor.equals(cursor.cursor.borrow()));
  }

  #[napi]
  pub fn bound(&self, config: String) -> Result<(), Error> {
    return unwrap_or_error(self.cursor.bound(config));
  }

  #[napi(getter)]
  pub fn key_format(&self) -> Result<String, Error> {
    return unwrap_or_error(self.cursor.get_key_format());
  }

  #[napi(getter)]
  pub fn value_format(&self) -> Result<String, Error> {
    return unwrap_or_error(self.cursor.get_value_format());
  }

  pub fn get_internal_cursor(&self) -> &InternalWiredTigerCursor {
    return &self.cursor;
  }

}
