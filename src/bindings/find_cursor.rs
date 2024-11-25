use std::borrow::Borrow;

use napi::bindgen_prelude::Array;
use napi::{Env, Error};
use crate::glue::find_cursor::InternalWiredTigerFindCursor;
use crate::glue::query_value::{Format, QueryValue};
use crate::glue::utils::parse_format;

use super::utils::{populate_values, unwrap_or_error};


#[napi(js_name = "WiredTigerFindCursor")]
pub struct WiredTigerFindCursor {
  cursor: InternalWiredTigerFindCursor,
}

#[napi]
impl WiredTigerFindCursor {
  pub fn new(cursor: InternalWiredTigerFindCursor) -> Self {
    return WiredTigerFindCursor {
      cursor,
    };
  }

  #[napi]
  pub fn close(&mut self) -> Result<(), Error> {
    return unwrap_or_error(self.cursor.close());
  }

  #[napi]
  pub fn next(&mut self) -> Result<bool, Error> {
    unwrap_or_error(self.cursor.init())?;
    return unwrap_or_error(self.cursor.next());
  }

  #[napi]
  pub fn prev(&mut self) -> Result<bool, Error> {
    unwrap_or_error(self.cursor.init())?;
    return unwrap_or_error(self.cursor.prev());
  }

  #[napi]
  pub fn reset(&mut self) -> Result<(), Error> {
    unwrap_or_error(self.cursor.init())?;
    return unwrap_or_error(self.cursor.reset());
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
        self.check_formats(&parsed_formats, self.cursor.get_key_formats())?;
        formats = &parsed_formats;
      },
      None => {
        formats = self.cursor.get_key_formats();
      }
    }

    return populate_values(env, values, formats)
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
        self.check_formats(&parsed_formats, self.cursor.get_value_formats())?;
        formats = &parsed_formats;
      },
      None => {
        formats = self.cursor.get_value_formats();
      }
    }
    return populate_values(env, values, formats)
  }

  #[napi]
  pub fn compare(&self, cursor: &WiredTigerFindCursor) -> Result<i32, Error> {
    return unwrap_or_error(self.cursor.compare(cursor.cursor.borrow()));
  }

  #[napi]
  pub fn equals(&self, cursor: &WiredTigerFindCursor) -> Result<bool, Error> {
    return unwrap_or_error(self.cursor.equals(cursor.cursor.borrow()));
  }

  #[napi(getter)]
  pub fn key_format(&self) -> Result<String, Error> {
    return unwrap_or_error(self.cursor.get_key_format());
  }

  #[napi(getter)]
  pub fn value_format(&self) -> Result<String, Error> {
    return unwrap_or_error(self.cursor.get_value_format());
  }

  pub fn get_internal_cursor(&self) -> &InternalWiredTigerFindCursor {
    return &self.cursor;
  }

}
