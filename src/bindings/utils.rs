use std::borrow::Borrow;
use std::collections::HashMap;
use std::ffi::{CStr, CString};
use std::fmt::Display;

use napi::bindgen_prelude::Array;
use napi::{Env, Error, JsBigInt, JsBoolean, JsBuffer, JsNumber, JsUnknown, ValueType};

use crate::glue::error::*;

use crate::glue::query_value::{ExternalValue, FieldFormat, Format, InternalIndexSpec, Operation, QueryValue};
use crate::external::wiredtiger::wiredtiger_strerror;
use crate::glue::utils::flip_sign;


pub fn wt_error_from_int(error: i32) -> Error {
  let error_str = unsafe { CStr::from_ptr(wiredtiger_strerror(error)).to_str().unwrap() };
  return Error::from_reason(error_str);
}

pub fn error_from_glue_error(error: GlueError) -> Error {
  match error.error_domain {
    ErrorDomain::GLUE => Error::from_reason(glue_strerror(error)),
    ErrorDomain::WIREDTIGER => wt_error_from_int(error.wiredtiger_error)
  }
}

pub fn error_from_displayable_error<T: Display>(err: T) -> Error {
  return Error::from_reason(format!("{}", err));
}

pub fn unwrap_or_displayable_error<T, E: Display>(result: Result<T, E>) -> Result<T, Error> {
  match result {
    Err(err) => Err(error_from_displayable_error(err)),
    Ok(t) => Ok(t)
  }
}

pub fn unwrap_or_error<T>(result: Result<T, GlueError>) -> Result<T, Error> {
  match result {
    Err(error) => Err(error_from_glue_error(error)),
    Ok(t) => Ok(t)
  }
}

pub fn unwrap_or_display_error<R, E: Display>(result: Result<R, E>) -> Result<R, Error>{
  match result {
    Err(error) => Err(Error::from_reason(format!("{}", error))),
    Ok(t) => Ok(t)
  }
}


pub fn value_in(value: JsUnknown, format: &Format) -> Result<Option<QueryValue>, Error> {
  let value_type: ValueType = value.get_type()?;
  let mut qv: QueryValue = QueryValue::empty();
  qv.data_type = format.format;
  if value_type == ValueType::Undefined {
    return Ok(None);
  }
  match format.format {
    FieldFormat::PADDING => {
      return Ok(None);
    }
    FieldFormat::CHAR_ARRAY => {
      match value_type {
        ValueType::String => {
          let casted_value = value.coerce_to_string()?;
          let mut padded_cstr: Vec<u8> = casted_value.into_utf8()?.into();
          let missing = format.size as isize - padded_cstr.len() as isize;
          if missing > 0 {
            padded_cstr.reserve_exact(missing as usize);
            for _i in 0..missing {
              padded_cstr.push(0);
            }
          }

          qv.external_value = ExternalValue::ByteArrayBox(padded_cstr);
        },
        _ => return Err(error_from_glue_error(GlueError::for_glue_with_extra(GlueErrorCode::InvalidDataForFormat, format!("format={}, data type={}", format.format, value_type))))
      }
    }
    FieldFormat::STRING => {
      match value_type {
        ValueType::String => {
          let casted_value = value.coerce_to_string()?;
          let cstr = unwrap_or_display_error(CString::new(casted_value.into_utf8()?.as_str().unwrap()))?;

          qv.external_value = ExternalValue::StrBox(cstr);
        },
        _ => return Err(error_from_glue_error(GlueError::for_glue_with_extra(GlueErrorCode::InvalidDataForFormat, format!("format={}, data type={}", format.format, value_type))))
      }
    },
    FieldFormat::INT
    | FieldFormat::INT2
    | FieldFormat::BYTE
    | FieldFormat::HALF => {
      match value_type {
        ValueType::Number => {
          let casted_value: JsNumber = value.coerce_to_number()?;
          qv.external_value = ExternalValue::UInt(casted_value.get_int32()? as u64);
        },
        _ => return Err(error_from_glue_error(GlueError::for_glue_with_extra(GlueErrorCode::InvalidDataForFormat, format!("format={}, data type={}", format.format, value_type))))
      }
    },
    FieldFormat::BITFIELD => {
      match value_type {
        ValueType::Number => {
          let casted_value: JsNumber = value.coerce_to_number()?;
          qv.external_value = ExternalValue::UInt(casted_value.get_uint32()? as u64);
        },
        ValueType::Boolean => {
          let casted_value: JsBoolean = value.coerce_to_bool()?;
          qv.external_value = ExternalValue::UInt(casted_value.get_value()? as u64);
        }
        _ => return Err(error_from_glue_error(GlueError::for_glue_with_extra(GlueErrorCode::InvalidDataForFormat, format!("format={}, data type={}", format.format, value_type))))
      }
    },

    FieldFormat::UINT
    | FieldFormat::UINT2
    | FieldFormat::UBYTE
    | FieldFormat::UHALF => {
      match value_type {
        ValueType::Number => {
          let casted_value: JsNumber = value.coerce_to_number()?;
          qv.external_value = ExternalValue::UInt(casted_value.get_uint32()? as u64);
        },
        _ => return Err(error_from_glue_error(GlueError::for_glue_with_extra(GlueErrorCode::InvalidDataForFormat, format!("format={}, data type={}", format.format, value_type))))
      }
    },
    FieldFormat::WT_ITEM
    | FieldFormat::WT_ITEM_BIGINT
    | FieldFormat::WT_ITEM_DOUBLE
    | FieldFormat::WT_UITEM => {
      // TODO: almost certain I'm doing this "wrong" as in not the rust way - the use of alloc is frowned upon -
      // we should only ever see FieldFormat::WT_ITEM - since that's the official API version, but we'll accept BigInt, Double or a Buffer
      match value_type {
        ValueType::BigInt => {
          let mut casted_value = unsafe { value.cast::<JsBigInt>() };
          let (is_negative, words) = casted_value.get_words()?;
          let byte_count = ( casted_value.word_count * 8) + 1;
          let mut vec_u8 = Vec::with_capacity(byte_count);
          vec_u8.push(if is_negative == true { 0x80 } else { 0 });
          for word in words {
            // words are in LE format, we want them in BE - so each word will be inserted to the front
            let word_ptr = std::ptr::addr_of!(word) as *const u8;
            for i in 0..8 {
              vec_u8.insert(i + 1, unsafe {*(word_ptr.offset(i as isize)) });
            }
          }
          flip_sign(vec_u8.as_mut(), byte_count, !is_negative);
          qv.external_value = ExternalValue::ByteArrayBox(vec_u8);
        },
        ValueType::Number => {
          let casted_value: JsNumber = value.coerce_to_number()?;
          let double: f64 = casted_value.get_double()?;
          let mut double_bytes = double.to_be_bytes();
          let positive = double_bytes[0] & 0x80 != 0x80;
          flip_sign(double_bytes.as_mut(), 8, positive);
          qv.external_value = ExternalValue::ByteArrayBox(double_bytes.into());
        },
        _ => {
          if value.is_buffer().unwrap() {
            let casted_value: JsBuffer = unsafe { value.cast::<JsBuffer>() };

            let buf2: &mut [u8] = &mut casted_value.into_value()?;

            // In some cases (e.g., insertMany) where there is no control loss between calls - we can probably avoid a buffer copy
            // In fact, even with that loss (e.g., set_key -> insert) it seems to work - but my guess is this isn't guaranteed
            qv.external_value = ExternalValue::copy_byte_array(buf2);
          }
          else {
            return Err(error_from_glue_error(GlueError::for_glue_with_extra(GlueErrorCode::InvalidDataForFormat, format!("format={}, data type={}", format.format, value_type))))
          }
        }
      }
    },
    FieldFormat::LONG
    | FieldFormat::ULONG
    | FieldFormat::RECID => {
      match value_type {
        ValueType::Number => {
          let casted_value = value.coerce_to_number()?;
          qv.external_value = ExternalValue::UInt(casted_value.get_int64()? as u64);
        },
        ValueType::BigInt => {
          let casted_value = unsafe { value.cast::<JsBigInt>() };
          qv.external_value = ExternalValue::UInt(casted_value.get_u64()?.0);
        }
        _ => return Err(error_from_glue_error(GlueError::for_glue_with_extra(GlueErrorCode::InvalidDataForFormat, format!("format={}, data type={}", format.format, value_type))))
      }
    },
    _ => return Err(error_from_glue_error(GlueError::for_glue_with_extra(GlueErrorCode::InvalidFormat, format!("{}", format.format))))
  }


  Ok(Some(qv))
}

pub fn extract_values(values: Array, formats: &Vec<Format>, include_padding: bool) -> Result<Vec<QueryValue>, Error> {
  let mut converted: Vec<QueryValue> = vec![];
  if values.len() as usize != formats.len() {
    return Err(error_from_glue_error(GlueError::for_glue_with_extra(GlueErrorCode::InvalidLength, format!("Length {} is invalid for format size {}", values.len(), formats.len()).to_string())));
  }
  let mut seen_padding = false;
  for i in 0..values.len() {
    let item: Option<JsUnknown> = values.get(i)?;
    let qv = value_in(
      item.unwrap(),
      formats.get(i as usize).unwrap()
    )?;
    if qv.is_some() {
      converted.push(qv.unwrap());
    }
    else if include_padding == true {
      converted.push(QueryValue::empty());
    }
    else if seen_padding == true {
      return Err(Error::from_reason("Can't have a padding field in the middle of the format"));
    }
    else {
      seen_padding = true;
    }
  }

  return Ok(converted);
}

pub fn value_out(env: Env, value: &QueryValue, format: &Format) -> Result<JsUnknown, Error> {
  match format.format {
    FieldFormat::STRING => {
      let str = unwrap_or_error(value.get_str())?;
      Ok(env.create_string(str.to_str().unwrap())?.into_unknown())
    },
    FieldFormat::CHAR_ARRAY => {
      let bytes = unwrap_or_error(value.get_byte_array())?;
      // TODO: this feels wrong - presumably
      let mut str_bytes = bytes.clone();
      str_bytes.push(0);

      // let cstr = unsafe { CStr::from_bytes_with_nul_unchecked(&str_bytes) }.to_str().unwrap();
      // return Ok(env.create_string("char array")?.into_unknown());
      let res = CString::from(CStr::from_bytes_until_nul(&str_bytes).unwrap());
      return Ok(env.create_string(res.to_str().unwrap())?.into_unknown());
    },
    FieldFormat::INT
    | FieldFormat::INT2
    | FieldFormat::BYTE
    | FieldFormat::HALF => Ok(env.create_int32(*unwrap_or_error(value.get_uint())? as i32)?.into_unknown()),

    FieldFormat::UINT
    | FieldFormat::UINT2
    | FieldFormat::BITFIELD
    | FieldFormat::UBYTE
    | FieldFormat::UHALF => Ok(env.create_uint32(*unwrap_or_error(value.get_uint())? as u32)?.into_unknown()),

    FieldFormat::WT_ITEM
    | FieldFormat::WT_UITEM => {
      Ok(env.create_buffer_copy(unwrap_or_error(value.get_byte_array())?)?.into_unknown())
    },

    FieldFormat::LONG => {
      Ok(env.create_bigint_from_i64(*unwrap_or_error(value.get_uint())? as i64)?.into_unknown()?)
    }

    FieldFormat::ULONG
    | FieldFormat::RECID => {
      Ok(env.create_bigint_from_u64(*unwrap_or_error(value.get_uint())?)?.into_unknown()?)
    },
    FieldFormat::BOOLEAN => {
      let value = *unwrap_or_error(value.get_uint())?;
      Ok(env.get_boolean(value == 1)?.into_unknown())
    },

    FieldFormat::WT_ITEM_BIGINT => {
      let mut bytes: Vec<u8> = unwrap_or_error(value.get_byte_array())?.clone();
      let byte_length = bytes.len();
      let sign_bit: bool = bytes[0] & 0x80 == 0x80;

      flip_sign(bytes.as_mut(), byte_length, sign_bit);
      let word_count = ((byte_length - 1) / 8) as usize;
      let mut words: Vec<u64> = Vec::with_capacity(word_count);
      for i in 0..word_count {
        let mut word: u64 = 0;
        for a in 0..8 {
          word = word << 8;
          word = word | (bytes[i * 8 + (8 - a)] as u64);
        }
        words.insert(0, word);
      }
      return Ok(env.create_bigint_from_words(!sign_bit, words)?.into_unknown()?);
    }

    FieldFormat::WT_ITEM_DOUBLE => {
      let mut double_bytes: [u8; 8] = [0,0,0,0,0,0,0,0];
      let bytes = unwrap_or_error(value.get_byte_array())?;
      for i in 0..8 {
        double_bytes[i] = bytes[i];
      }
      let positive = double_bytes[0] & 0x80 == 0x80;
      flip_sign(double_bytes.as_mut(), 8, positive);
      Ok(env.create_double(f64::from_be_bytes(double_bytes))?.into_unknown())
    }
    _ => return Err(error_from_glue_error(GlueError::for_glue_with_extra(GlueErrorCode::InvalidDataForFormat, format!("format={}, data type={}", format.format, value.data_type))))
  }
}

pub fn check_formats(parsed_formats: &Vec<Format>, default_formats: &Vec<Format>) -> Result<(), Error> {
  if parsed_formats.len() != default_formats.len() {
    let err = Error::from_reason(format!("Format mismatch provided:{} != {}", parsed_formats.len(), default_formats.len()));
    return Err(err);
  }
  Ok(())
}

pub fn populate_values(env: Env, values: Vec<QueryValue>, formats: &Vec<Format>) -> Result<Array, Error> {
  let mut values_out: Array = env.create_array(values.len() as u32).unwrap();
  for i in 0..values.len() {
    let item: &QueryValue = values.get(i).unwrap();
    values_out.set(i as u32, value_out(
      env,
      item,
      formats.get(i as usize).unwrap()
    )?)?;
  }

  return Ok(values_out);
}


#[napi(object)]
pub struct IndexSpec {
  pub index_name: Option<String>,
  pub operation: Operation,
  pub sub_conditions: Option<Vec<IndexSpec>>,
  pub query_values: Option<Array>
}

pub fn parse_index_specs(index_specs: Vec<IndexSpec>, index_formats: &HashMap<String, Vec<Format>>, table_key_format: &Vec<Format>) -> Result<Vec<InternalIndexSpec>, Error> {
  let mut converted_specs:Vec<InternalIndexSpec> = Vec::with_capacity(index_specs.len());
  for index_spec in index_specs {
    let index_name = match index_spec.index_name.borrow() {
      Some(name) => name.to_string(),
      None => "".to_string()
    };
    converted_specs.push(InternalIndexSpec {
      index_name: index_spec.index_name,
      operation: index_spec.operation,
      sub_conditions: match index_spec.sub_conditions {
        Some(sub) => Some(parse_index_specs(sub, index_formats, table_key_format)?),
        None => None
      },
      query_values: match index_spec.query_values {
        Some(values) => {
          let formats: &Vec<Format>;
          if index_formats.len() == 0 {
            formats = table_key_format;
          }
          else {
            let index_format = index_formats.get(&index_name);
            match index_format {
              Some(format) => {
                formats = format;
              },
              None => {
                formats = table_key_format;
                // return Err(error_from_glue_error(GlueError::for_glue_with_extra(GlueErrorCode::AccessEmptyBox, format!("Couldn't find the format for index: {}", index_name))));
              }
            }
          }
          Some(extract_values(values, formats, false)?)
        },
        None => None
      }
    });
  }

  Ok(converted_specs)
}
