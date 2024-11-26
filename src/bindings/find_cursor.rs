use napi::bindgen_prelude::Array;
use napi::{Env, Error, JsObject};
use crate::glue::cursor_trait::InternalCursorTrait;
use crate::glue::find_cursor::InternalFindCursor;
use crate::glue::query_value::Format;
use crate::glue::utils::parse_format;

use super::cursor_trait::{CursorTrait, ExtendedCursorTrait};
use super::types::Document;
use super::utils::{check_formats, populate_values, unwrap_or_error};

impl CursorTrait for FindCursor {
  fn get_cursor_impl(&self) -> &impl InternalCursorTrait {
    return &self.cursor;
  }
}

impl ExtendedCursorTrait for FindCursor{}

#[napi]
pub struct FindCursor {
  cursor: InternalFindCursor,
  key_format: Option<Vec<Format>>,
  value_format: Option<Vec<Format>>,
}

#[napi]
impl FindCursor {
  pub fn new(cursor: InternalFindCursor, key_format_str: Option<String>, value_format_str: Option<String>) -> Result<Self, Error> {
    let mut key_format: Vec<Format> = Vec::new();
    if key_format_str.is_some() {
      unwrap_or_error(parse_format(key_format_str.unwrap(), &mut key_format))?;
      check_formats(&key_format, cursor.get_key_formats())?;
    }
    let mut value_format: Vec<Format> = Vec::new();
    if value_format_str.is_some() {
      unwrap_or_error(parse_format(value_format_str.unwrap(), &mut value_format))?;
      check_formats(&value_format, cursor.get_value_formats())?;
    }
    return Ok(FindCursor {
      cursor,
      key_format: if key_format.len() != 0 { Some(key_format) } else { None },
      value_format: if value_format.len() != 0 { Some(value_format) } else { None },
    });
  }

  #[napi]
  pub fn close(&mut self) -> Result<(), Error> {
    return unwrap_or_error(self.cursor.close());
  }

  #[napi]
  pub fn next(&mut self) -> Result<bool, Error> {
    return unwrap_or_error(self.cursor.next());
  }

  #[napi]
  pub fn prev(&mut self) -> Result<bool, Error> {
    return unwrap_or_error(self.cursor.prev());
  }

  #[napi]
  pub fn reset(&mut self) -> Result<(), Error> {
    return unwrap_or_error(self.cursor.reset());
  }

  #[napi]
  pub fn next_batch(&mut self, env: Env, key_format_str: Option<String>, value_format_str: Option<String>, batch_size: Option<i32>) -> Result<Vec<Document>, Error> {
    let internal_documents = unwrap_or_error(self.cursor.next_batch(batch_size))?;
    let key_formats: &Vec<Format>;
    let mut parsed_key_formats: Vec<Format>;
    match key_format_str {
      Some(str) => {
        parsed_key_formats = Vec::new();
        unwrap_or_error(parse_format(str, &mut parsed_key_formats))?;
        check_formats(&parsed_key_formats, self.cursor.get_key_formats())?;
        key_formats = &parsed_key_formats;
      },
      None => {
        match &self.key_format {
          Some(formats) => {
            key_formats = formats;
          },
          None => {
            key_formats = self.cursor.get_key_formats();
          }
        }
      }
    }


    let value_formats: &Vec<Format>;
    let mut parsed_value_formats: Vec<Format>;
    match value_format_str {
      Some(str) => {
        parsed_value_formats = Vec::new();
        unwrap_or_error(parse_format(str, &mut parsed_value_formats))?;
        check_formats(&parsed_value_formats, self.cursor.get_value_formats())?;
        value_formats = &parsed_value_formats;
      },
      None => {
        match &self.value_format {
          Some(formats) => {
            value_formats = formats;
          },
          None => {
            value_formats = self.cursor.get_value_formats();
          }
        }
      }
    }

    return internal_documents.into_iter().map(|id| {
      Ok(Document {
        key: populate_values(env, id.key, key_formats)?,
        value: populate_values(env, id.value, value_formats)?
      })
    }).collect();
  }


  #[napi]
  pub fn get_next(&self, env: Env, key_format_str: Option<String>, value_format_str: Option<String>) -> Result<Option<Document>, Error> {
    let result = self.cursor.next();
    return self.get_next_prev(env, key_format_str, value_format_str, result);
  }

  #[napi]
  pub fn get_prev(&self, env: Env, key_format_str: Option<String>, value_format_str: Option<String>) -> Result<Option<Document>, Error> {
    let result = self.cursor.prev();
    return self.get_next_prev(env, key_format_str, value_format_str, result);
  }

  #[napi(js_name="getKey")]
  pub fn _get_key(&self, env: Env, format_str: Option<String>) -> Result<Array, Error> {
    return self.get_key(env, format_str);
  }

  #[napi(js_name="getValue")]
  pub fn _get_value(&self, env: Env, format_str: Option<String>) -> Result<Array, Error> {
    return self.get_value(env, format_str);
  }

  #[napi]
  pub fn compare(&self, cursor: &FindCursor) -> Result<i32, Error> {
    return unwrap_or_error(self.cursor.compare(&cursor.cursor));
  }

  #[napi]
  pub fn equals(&self, cursor: &FindCursor) -> Result<bool, Error> {
    return unwrap_or_error(self.cursor.equals(&cursor.cursor));
  }

  #[napi(getter)]
  pub fn key_format(&self) -> Result<String, Error> {
    return unwrap_or_error(self.cursor.get_key_format());
  }

  #[napi(getter)]
  pub fn value_format(&self) -> Result<String, Error> {
    return unwrap_or_error(self.cursor.get_value_format());
  }

  pub fn get_internal_cursor(&self) -> &InternalFindCursor {
    return &self.cursor;
  }

}
