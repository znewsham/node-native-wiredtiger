use napi::{bindgen_prelude::Array, Env, Error};

use crate::{external::wiredtiger::WT_NOTFOUND, glue::{cursor_trait::InternalCursorTrait, error::{ErrorDomain, GlueError}, query_value::{Format, QueryValue}, utils::parse_format}};

use super::{types::Document, utils::{check_formats, error_from_glue_error, populate_values, unwrap_or_error}};

pub trait CursorTrait {
  fn get_cursor_impl(&self) -> &impl InternalCursorTrait;
}

pub trait ExtendedCursorTrait: CursorTrait {

  fn get_next_prev(&self, env: Env, key_format_str: Option<String>, value_format_str: Option<String>, result: Result<bool, GlueError>) -> Result<Option<Document>, Error> {
    let cursor = self.get_cursor_impl();
    match result {
      Err(err) => {
        if err.error_domain == ErrorDomain::WIREDTIGER && err.wiredtiger_error == WT_NOTFOUND {
          return Ok(None)
        }
        return Err(error_from_glue_error(err))
      },
      Ok(res) => if res == false { return Ok(None) }
    }

    let key_formats: &Vec<Format>;
    let mut parsed_key_formats: Vec<Format>;
    match key_format_str {
      Some(str) => {
        parsed_key_formats = Vec::new();
        unwrap_or_error(parse_format(str, &mut parsed_key_formats))?;
        check_formats(&parsed_key_formats, cursor.get_key_formats())?;
        key_formats = &parsed_key_formats;
      },
      None => {
        key_formats = cursor.get_key_formats();
      }
    }

    let value_formats: &Vec<Format>;
    let mut parsed_value_formats: Vec<Format>;
    match value_format_str {
      Some(str) => {
        parsed_value_formats = Vec::new();
        unwrap_or_error(parse_format(str, &mut parsed_value_formats))?;
        check_formats(&parsed_value_formats, cursor.get_value_formats())?;
        value_formats = &parsed_value_formats;
      },
      None => {
        value_formats = cursor.get_value_formats();
      }
    }
    let key = unwrap_or_error(cursor.get_key())?;
    let value = unwrap_or_error(cursor.get_value())?;
    let populated_key = populate_values(env, key, key_formats)?;
    let populated_value = populate_values(env, value, value_formats)?;
    let document = Document {
      key: populated_key,
      value: populated_value
    };
    Ok(Some(document))
  }

  fn get_key(&self, env: Env, format_str: Option<String>) -> Result<Array, Error> {
    let cursor = self.get_cursor_impl();
    let values: Vec<QueryValue> = unwrap_or_error(cursor.get_key())?;
    let formats: &Vec<Format>;
    let mut parsed_formats: Vec<Format>;
    match format_str {
      Some(str) => {
        parsed_formats = Vec::new();
        unwrap_or_error(parse_format(str, &mut parsed_formats))?;
        check_formats(&parsed_formats, cursor.get_key_formats())?;
        formats = &parsed_formats;
      },
      None => {
        formats = cursor.get_key_formats();
      }
    }

    return populate_values(env, values, formats)
  }

  fn get_value(&self, env: Env, format_str: Option<String>) -> Result<Array, Error> {
    let cursor = self.get_cursor_impl();
    let values: Vec<QueryValue> = unwrap_or_error(cursor.get_value())?;
    let formats: &Vec<Format>;
    let mut parsed_formats: Vec<Format>;
    match format_str {
      Some(str) => {
        parsed_formats = Vec::new();
        unwrap_or_error(parse_format(str, &mut parsed_formats))?;
        check_formats(&parsed_formats, cursor.get_value_formats())?;
        formats = &parsed_formats;
      },
      None => {
        formats = cursor.get_value_formats();
      }
    }
    return populate_values(env, values, formats)
  }

}
