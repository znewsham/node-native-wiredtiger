use std::{ffi::{CStr, CString}, ptr::{addr_of_mut, null, null_mut}};
use crate::external::wiredtiger::{__wt_config_item_WT_CONFIG_ITEM_TYPE_WT_CONFIG_ITEM_BOOL, __wt_config_item_WT_CONFIG_ITEM_TYPE_WT_CONFIG_ITEM_STRUCT, wiredtiger_config_parser_open, WT_CONFIG_ITEM, WT_CONFIG_PARSER};

use super::{error::*, query_value::{FieldFormat, Format}};

#[inline(always)]
pub fn get_fn<T>(fn_prop: Option<T>) -> Result<T, GlueError> {
  match fn_prop {
    Some(r#fn) => Ok(r#fn),
    None => Err(GlueError::for_glue(GlueErrorCode::ImpossibleNoFn))
  }
}

pub struct OptionalCString {
  cstring: Option<CString>
}

impl OptionalCString {
  pub fn as_ptr(&self) -> *const i8 {
    match &self.cstring {
      None => null_mut(),
      Some(cstr) => cstr.as_ptr()
    }
  }
}

pub fn unwrap_string_or_null(thing: Option<String>) -> OptionalCString {
  return OptionalCString {
    cstring: match thing {
      Some(config) => Some(CString::new(config).unwrap()),
      None => None
    }
  }
}

pub fn copy_cfg_value_trim_ends(cfg: &WT_CONFIG_ITEM) -> Option<String> {
  if cfg.type_ > __wt_config_item_WT_CONFIG_ITEM_TYPE_WT_CONFIG_ITEM_STRUCT || cfg.len == 0 || cfg.str_ == null() {
    return None;
  }
  return Some(char_ptr_of_length_to_string(unsafe { cfg.str_.offset(1) }, cfg.len - 2).unwrap());
}

pub fn copy_cfg_value(cfg: &WT_CONFIG_ITEM) -> Option<String> {
  if cfg.type_ > __wt_config_item_WT_CONFIG_ITEM_TYPE_WT_CONFIG_ITEM_STRUCT || cfg.len == 0 || cfg.str_ == null() {
    return None;
  }
  return Some(char_ptr_of_length_to_string(cfg.str_, cfg.len).unwrap());
}

pub fn get_optional_boolean(config_parser_ptr: *mut WT_CONFIG_PARSER, field_name: &str) -> Result<bool, GlueError> {
  let mut cfg_item: WT_CONFIG_ITEM = WT_CONFIG_ITEM::empty();
  if config_parser_ptr == null_mut() {
    return Err(GlueError::for_glue(GlueErrorCode::UnlikelyNoPtr));
  }
  let config_parser = unsafe { *config_parser_ptr };
  let get = get_fn(config_parser.get)?;
  unsafe { get(config_parser_ptr, unwrap_string_and_dealloc(field_name.to_string()).as_ptr(), addr_of_mut!(cfg_item)) };

  if cfg_item.type_ == __wt_config_item_WT_CONFIG_ITEM_TYPE_WT_CONFIG_ITEM_BOOL {
    return Ok(true)
  }
  return Ok(false);
}

pub fn get_mandatory_string(config_parser_ptr: *mut WT_CONFIG_PARSER, field_name: &str) -> Result<String, GlueError> {
  let mut cfg_item: WT_CONFIG_ITEM = WT_CONFIG_ITEM::empty();
  if config_parser_ptr == null_mut() {
    return Err(GlueError::for_glue(GlueErrorCode::UnlikelyNoPtr));
  }
  let config_parser = unsafe { *config_parser_ptr };
  let get = get_fn(config_parser.get)?;
  unsafe { get(config_parser_ptr, unwrap_string_and_dealloc(field_name.to_string()).as_ptr(), addr_of_mut!(cfg_item)) };
  let res = copy_cfg_value(&cfg_item);
  if res.is_none() {
    return Err(GlueError::for_glue_with_extra(GlueErrorCode::MissingRequiredConfig, field_name.to_string()))
  }
  Ok(res.unwrap())
}

pub fn get_mandatory_string_trim_ends(config_parser_ptr: *mut WT_CONFIG_PARSER, field_name: &str) -> Result<String, GlueError> {
  let mut cfg_item: WT_CONFIG_ITEM = WT_CONFIG_ITEM::empty();
  if config_parser_ptr == null_mut() {
    return Err(GlueError::for_glue(GlueErrorCode::UnlikelyNoPtr));
  }
  let config_parser = unsafe { *config_parser_ptr };
  let get = get_fn(config_parser.get)?;
  unsafe { get(config_parser_ptr, unwrap_string_and_dealloc(field_name.to_string()).as_ptr(), addr_of_mut!(cfg_item)) };
  let res = copy_cfg_value_trim_ends(&cfg_item);
  if res.is_none() {
    return Err(GlueError::for_glue_with_extra(GlueErrorCode::MissingRequiredConfig, field_name.to_string()))
  }
  Ok(res.unwrap())
}

pub fn get_optional_string(config_parser_ptr: *mut WT_CONFIG_PARSER, field_name: &str) -> Result<Option<String>, GlueError> {
  let mut cfg_item: WT_CONFIG_ITEM = WT_CONFIG_ITEM::empty();
  if config_parser_ptr == null_mut() {
    return Err(GlueError::for_glue(GlueErrorCode::UnlikelyNoPtr));
  }
  let config_parser = unsafe { *config_parser_ptr };
  let get = get_fn(config_parser.get)?;
  unsafe { get(config_parser_ptr, unwrap_string_and_dealloc(field_name.to_string()).as_ptr(), addr_of_mut!(cfg_item)) };
  let res = copy_cfg_value(&cfg_item);
  Ok(res)
}

pub fn unwrap_or_throw<T>(thing: Option<T>) -> Result<T, GlueError> {
  match thing {
    Some(a_thing) => Ok(a_thing),
    None => Err(GlueError::for_glue(GlueErrorCode::UnlikelyNoValue))
  }
}

pub fn char_ptr_of_length_to_string(char_ptr: *const i8, length: u64) -> Result<String, GlueError> {
  let mut chars: Vec<u8> = Vec::with_capacity(length as usize + 1);
  for i in 0..length {
    chars.push(unsafe { *char_ptr.offset(i as isize) as u8 });
  }
  chars.push(0);
  let result = CStr::from_bytes_until_nul(&chars).unwrap().to_str();  match result {
    Ok(str) => Ok(str.to_string()),
    Err(_err) => Err(GlueError::for_glue(GlueErrorCode::NoString))
  }
}

pub fn char_ptr_to_string(char_ptr: *const i8) -> Result<String, GlueError> {
  if char_ptr == null() {
    return Ok("".to_string());
  }
  let result = unsafe { CStr::from_ptr(char_ptr) }.to_str();
  match result {
    Ok(str) => Ok(str.to_string()),
    Err(_err) => Err(GlueError::for_glue(GlueErrorCode::NoString))
  }
}


pub fn formats_to_string(formats: &Vec<Format>) -> String {
  let mut str = String::with_capacity(formats.len());
  for format in formats {
    if format.size != 0 {
      str.push_str(format!("{}", format.size).as_str());
    }
    if field_is_wt_item(format.format) {
      str.push('u');
    }
    else {
      str.push(format.format);
    }
  }
  return str;
}

pub fn parse_format(format: String, formats: &mut Vec<Format>) -> Result<(), GlueError> {
  let mut size: u32 = 0;
  for c in format.chars() {

    if c.is_digit(10) {
      size *= 10;
      // we don't care about lengths (for now)
      size += unwrap_or_throw(c.to_digit(10))?;

      continue;
    }
    match c {
      FieldFormat::CHAR_ARRAY => formats.push(Format { format: c, size: std::cmp::max(size as u64, 1) }),
      FieldFormat::BYTE
      | FieldFormat::HALF
      | FieldFormat::INT
      | FieldFormat::INT2
      | FieldFormat::LONG
      | FieldFormat::UBYTE
      | FieldFormat::UHALF
      | FieldFormat::UINT
      | FieldFormat::UINT2
      | FieldFormat::ULONG
      | FieldFormat::RECID => formats.push(Format { format: c, size: 0 }),
      FieldFormat::BITFIELD
      | FieldFormat::STRING
      | FieldFormat::WT_ITEM
      | FieldFormat::WT_UITEM
      | FieldFormat::WT_ITEM_BIGINT
      | FieldFormat::WT_ITEM_DOUBLE
      | FieldFormat::BOOLEAN
      | FieldFormat::PADDING => formats.push(Format { format: c, size: size as u64 }),
      _ => return Err(GlueError::for_glue_with_extra(GlueErrorCode::InvalidFormat, format!(": {}", c)))
    }
    size = 0;
  }
  Ok(())
}

pub fn unwrap_string_and_leak(thing: String) -> *mut i8 {
  CString::new(thing).unwrap().into_raw()
}

pub fn unwrap_string_and_dealloc(thing: String) -> CString {
  return CString::new(thing).unwrap();
}



pub fn deref_ptr<T:Copy>(thing: *mut T) -> Result<T, GlueError> {
  if thing == std::ptr::null_mut() {
    return Err(GlueError::for_glue(GlueErrorCode::UnlikelyNoPtr));
  }
  unsafe {
    Ok(*thing)
  }
}


// pub fn field_is_ptr(field: char) -> bool {
//   return field == FieldFormat::WT_ITEM || field == FieldFormat::WT_UITEM || field == FieldFormat::WT_ITEM_DOUBLE || field == FieldFormat::WT_ITEM_BIGINT || field == FieldFormat::CHAR_ARRAY || field == FieldFormat::STRING;
// }
#[inline(always)]
pub fn field_is_wt_item(field: char) -> bool {
  return field == FieldFormat::WT_ITEM || field == FieldFormat::WT_UITEM || field == FieldFormat::WT_ITEM_DOUBLE || field == FieldFormat::WT_ITEM_BIGINT;
}


pub fn flip_sign(
  buffer: &mut [u8],
  bytes: usize,
  positive: bool
) -> () {
  if positive {
    buffer[0] = buffer[0] ^ 0x80;
  }
  else {
    for i in 0..bytes {
      buffer[i] = buffer[i] ^ 0xff;
    }
  }
}


pub fn extract_formats_from_config(config: String, key_formats: &mut Vec<Format>, value_formats: &mut Vec<Format>) -> Result<(), GlueError> {
  let mut parser_ptr: *mut WT_CONFIG_PARSER = std::ptr::null_mut();
  let len = config.len();
  let str = unwrap_string_and_dealloc(config);
  let error = unsafe { wiredtiger_config_parser_open(
    std::ptr::null_mut(),
    str.as_ptr(),
    len as u64,
    std::ptr::addr_of_mut!(parser_ptr)
  ) };

  if error != 0 {
    return Err(GlueError::for_wiredtiger( error));
  }
  let parser = deref_ptr(parser_ptr)?;
  let mut key_format_item: WT_CONFIG_ITEM = WT_CONFIG_ITEM {
    len: 0,
    str_: std::ptr::null_mut(),
    type_: 0,
    val: 0
  };
  let mut value_format_item: WT_CONFIG_ITEM = WT_CONFIG_ITEM {
    len: 0,
    str_: std::ptr::null_mut(),
    type_: 0,
    val: 0
  };
  unsafe { get_fn(parser.get)?(parser_ptr, unwrap_string_and_dealloc("key_format".to_string()).as_ptr(), std::ptr::addr_of_mut!(key_format_item))};
  unsafe { get_fn(parser.get)?(parser_ptr, unwrap_string_and_dealloc("value_format".to_string()).as_ptr(), std::ptr::addr_of_mut!(value_format_item))};

  parse_format(char_ptr_of_length_to_string(key_format_item.str_, key_format_item.len)?, key_formats)?;
  parse_format(char_ptr_of_length_to_string(value_format_item.str_, value_format_item.len)?, value_formats)?;
  unsafe { get_fn(parser.close)?(parser_ptr) };
  Ok(())
}



pub fn extract_words_for_string(
  cstr: &CStr,
  ngram_store: &mut Vec<CString>
) {
  let str = cstr.to_str().unwrap();
  for token in str.split(" ") {
    ngram_store.push(CString::new(token).unwrap());
  }
}

pub fn extract_ngrams_for_string(
  cstr: &CStr,
  ngram_length: usize,
  ngram_store: &mut Vec<CString>
) {
  let str = cstr.to_str().unwrap();
  for token in str.split(" ") {
    let token_len = token.len();
    if token_len <= ngram_length {
      ngram_store.push(CString::new(token).unwrap());
    }
    else {
      let end_index: usize = token_len - (ngram_length - 1);
      for i in 0..end_index {
        ngram_store.push(CString::new(token[i..(i+3)].to_string()).unwrap());
      }
    }
  }
}
