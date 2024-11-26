use napi::bindgen_prelude::Array;
use napi::{Env, Error};
use std::borrow::Borrow;
use crate::glue::cursor::InternalCursor;
use super::cursor_trait::{CursorTrait, ExtendedCursorTrait};
use super::types::Document;
use crate::glue::cursor_trait::InternalCursorTrait;
use crate::glue::query_value::QueryValue;

use super::utils::{extract_values, unwrap_or_error};

#[napi]
pub struct Cursor {
  cursor: InternalCursor,
  stored_key: Option<Vec<QueryValue>>,
  stored_value: Option<Vec<QueryValue>>
}

impl CursorTrait for Cursor {
  fn get_cursor_impl(&self) -> &impl InternalCursorTrait {
    return &self.cursor;
  }
}

impl ExtendedCursorTrait for Cursor {}

#[napi]
impl Cursor {
  pub fn new(cursor: InternalCursor) -> Self {
    return Cursor {
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

  #[napi]
  pub fn get_next(&self, env: Env, key_format_str: Option<String>, value_format_str: Option<String>) -> Result<Option<Document>, Error> {
    let result = self.cursor.next();
    return self.get_next_prev(env, key_format_str, value_format_str, result);
  }

  #[napi]
  pub fn prev(&self) -> Result<bool, Error> {
    return unwrap_or_error(self.cursor.prev());
  }

  #[napi]
  pub fn get_prev(&self, env: Env, key_format_str: Option<String>, value_format_str: Option<String>) -> Result<Option<Document>, Error> {
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
    let mut converted_values = extract_values(key_values, self.cursor.key_formats.borrow(), false)?;
    unwrap_or_error(self.cursor.set_key(&mut converted_values))?;
    self.stored_key = Some(converted_values);
    Ok(())
  }

  #[napi(js_name="getKey")]
  pub fn _get_key(&self, env: Env, format_str: Option<String>) -> Result<Array, Error> {
    return self.get_key(env, format_str);
  }

  #[napi]
  pub fn set_value(&mut self, value_values: Array) -> Result<(), Error> {
    let mut converted_values = extract_values(value_values, self.cursor.value_formats.borrow(), false)?;
    unwrap_or_error(self.cursor.set_value(&mut converted_values))?;
    self.stored_value = Some(converted_values);
    Ok(())
  }

  #[napi(js_name="getValue")]
  pub fn _get_value(&self, env: Env, format_str: Option<String>) -> Result<Array, Error> {
    return self.get_value(env, format_str);
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
  pub fn compare(&self, cursor: &Cursor) -> Result<i32, Error> {
    return unwrap_or_error(self.cursor.compare(cursor.cursor.borrow()));
  }

  #[napi]
  pub fn equals(&self, cursor: &Cursor) -> Result<bool, Error> {
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

  pub fn get_internal_cursor(&self) -> &InternalCursor {
    return &self.cursor;
  }

}
