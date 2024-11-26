use std::{os::raw::c_void, ptr::null_mut};

use napi::{bindgen_prelude::{Array, Buffer}, Env, Error};

use crate::{external::wiredtiger::WT_ITEM, glue::{query_value::{Format, QueryValue}, utils::{formats_to_string, parse_format, unwrap_string_and_dealloc}, wt_item::unpack_wt_item}};

use super::{session::Session, utils::{populate_values, unwrap_or_error}};

#[napi]
#[allow(dead_code)]
pub fn struct_unpack(
  env: Env,
  session: Option<&mut Session>,
  buffer: Buffer,
  format: String
) -> Result<Array, Error> {
  let mut wt_item = WT_ITEM::empty();
  wt_item.size = buffer.len() as u64;
  wt_item.data = buffer.as_ptr() as *const c_void;
  let mut formats: Vec<Format> = Vec::new();

  unwrap_or_error(parse_format(format.clone(), &mut formats))?;
  let mut values: Vec<QueryValue> = vec![QueryValue::empty(); formats.len()]; //Vec::with_capacity(formats.len());

  let cleaned_format = formats_to_string(&formats);
  unwrap_or_error(unpack_wt_item(
    if session.is_some() { session.unwrap().get_session_ptr() } else  {null_mut()},
    &raw const wt_item,
    unwrap_string_and_dealloc(cleaned_format).as_ptr() as *mut i8,
    &formats,
    &mut values,
    true
  ))?;

  return populate_values(env, values, &formats)
}
