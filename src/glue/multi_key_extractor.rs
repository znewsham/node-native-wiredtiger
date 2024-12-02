use std::borrow::Borrow;
use std::collections::HashMap;
use std::ffi::{CStr, CString};
use std::os::raw::c_void;
use std::ptr::{addr_of_mut, null_mut};
use std::sync::{Mutex, MutexGuard, OnceLock};

use crate::external::avcall::{av_alist, avcall_arg_ptr, avcall_call};
use crate::external::avcall_helpers::{avcall_extract_fn, avcall_start_void};
use crate::external::wiredtiger::*;
use crate::glue::query_value::InternalValue;

use super::avcall::populate_av_list_for_packing;
use super::error::GlueError;
use super::query_value::{ExternalValue, Format, QueryValue};
use super::utils::{char_ptr_of_length_to_string, extract_ngrams_for_string, extract_words_for_string, get_fn, get_mandatory_string, get_mandatory_string_trim_ends, get_optional_string, parse_format, unwrap_string_and_dealloc};
use super::wt_item::unpack_wt_item;

#[derive(PartialEq)]
enum ExtractorType {
  NONE,
  NGRAMS,
  WORDS
}

impl ExtractorType {
  fn from_str(str: String) -> Self{
    if str == "ngrams" {
      return ExtractorType::NGRAMS;
    }
    if str == "words" {
      return ExtractorType::WORDS;
    }
    return ExtractorType::NONE;
  }
}

struct MultiKeyExtractorColumnConfig {
  columns: Vec<u32>,
  extractor_type: ExtractorType,
  format: Format,

  // TODO: general config
  ngrams: u32
}

pub struct InternalWiredTigerMultiKeyExtractor {
  extractor: WT_EXTRACTOR,
  column_configs: Vec<MultiKeyExtractorColumnConfig>,
  extract_format: Vec<Format>,
  table_value_format_cstr: CString,
}


fn get_global_hashmap() -> MutexGuard<'static, HashMap<u64, Box<InternalWiredTigerMultiKeyExtractor>>> {
  static MAP: OnceLock<Mutex<HashMap<u64, Box<InternalWiredTigerMultiKeyExtractor>>>> = OnceLock::new();
  MAP.get_or_init(|| Mutex::new(HashMap::new()))
      .lock()
      .expect("Let's hope the lock isn't poisoned")
}


impl InternalWiredTigerMultiKeyExtractor {
  pub fn new(
    extract_key_format_str: String,
    table_value_format: String,
    _index_id: String
  ) -> Result<Self, GlueError> {
    let mut extract_format: Vec<Format> = Vec::new();
    parse_format(extract_key_format_str, &mut extract_format)?;
    return Ok(InternalWiredTigerMultiKeyExtractor {
      column_configs: Vec::new(),
      extract_format,
      table_value_format_cstr: unwrap_string_and_dealloc(table_value_format),
      extractor: WT_EXTRACTOR {
        customize: None,
        extract: Some(InternalWiredTigerMultiKeyExtractor::static_extract),
        terminate: Some(InternalWiredTigerMultiKeyExtractor::static_terminate)
      }
    });
  }



  fn recurse_custom_fields(
    &self,
    result_cursor: *mut WT_CURSOR,
    value_values: &Vec<QueryValue>,
    multikeys: &Vec<Vec<CString>>,
    write_index: usize,
    multikey_index: usize,
    temp_values: Vec<QueryValue>// intentionally *NOT* a ptr or ref
  ) -> Result<i32, GlueError> {
    if write_index == self.column_configs.len() {

      // we're done - issue the call and return.
      let mut set_value_list: av_alist = av_alist::new();
      let set_value_list_ptr = addr_of_mut!(set_value_list);
      avcall_start_void(
        set_value_list_ptr,
        avcall_extract_fn(get_fn(unsafe { *result_cursor }.set_key).unwrap() as *const())
      );
      unsafe { avcall_arg_ptr(set_value_list_ptr, result_cursor as *mut c_void) };
      for i in 0..temp_values.len() {
        let value = temp_values.get(i).unwrap();
        let format: Format = Format { format: value.data_type, size: 0 };
        populate_av_list_for_packing(
          value,
          set_value_list_ptr,
          &format
        )?;
      }
      unsafe { avcall_call(set_value_list_ptr) };

      let error = unsafe { get_fn((*result_cursor).insert).unwrap()(result_cursor) };
      if error == WT_NOTFOUND || error == WT_DUPLICATE_KEY {
        return Ok(0);
      }
      return Ok(error);
    }
    let column = self.column_configs.get(write_index).unwrap();
    if column.extractor_type != ExtractorType::NONE {
      // recurse for each and return
      let keys = multikeys.get(multikey_index).unwrap();
      for key in keys {
        let mut new_temp_values: Vec<QueryValue> = temp_values.clone();
        let mut qv: QueryValue = QueryValue::empty();
        qv.set_external_str_box(key.clone())?;
        qv.data_type = column.format.format;
        new_temp_values.push(qv);
        let error = self.recurse_custom_fields(
          result_cursor,
          value_values,
          multikeys,
          write_index + 1,
          multikey_index + 1,
          new_temp_values
        )?;
        if error != 0 {
          return Ok(error);
        }
      }
      return Ok(0);
    }

    // set the value and recurse.

    let mut new_temp_values: Vec<QueryValue> = temp_values;
    let index: &u32 = self.column_configs.get(write_index).unwrap().columns.get(0).unwrap();
    let qv: QueryValue = value_values.get(*index as usize).unwrap().clone();
    new_temp_values.push(qv);
    return self.recurse_custom_fields(
      result_cursor,
      value_values,
      multikeys,
      write_index + 1,
      multikey_index,
      new_temp_values
    );

  }

  fn extract(
    &self,
    wt_session: *mut WT_SESSION,
    _wt_item_key: *const WT_ITEM,
    wt_item_value: *const WT_ITEM,
    wt_result_cursor: *mut WT_CURSOR
  ) -> Result<i32, GlueError> {
    let mut value_values: Vec<QueryValue> = vec![QueryValue::empty(); self.extract_format.len()];
    unpack_wt_item(
      wt_session,
      wt_item_value,
      self.table_value_format_cstr.as_ptr() as *mut i8,
      self.extract_format.borrow(),
      &mut value_values,
      true
    )?;


    let mut multikeys: Vec<Vec<CString>> = Vec::new();

    for column_config in self.column_configs.iter() {
      if column_config.extractor_type == ExtractorType::NGRAMS {
        let mut column_ngrams: Vec<CString> = Vec::new();
        for index  in column_config.columns.iter() {
          let value = value_values.get((*index) as usize).unwrap();
          match &value.internal_value {
            InternalValue::ByteArrayBox(bab) => {
              extract_ngrams_for_string(
                unsafe { CStr::from_ptr(bab.as_ptr() as *const i8) },
                column_config.ngrams as usize,
                &mut column_ngrams
              );
            },
            _ => {}
          }
        }
        multikeys.push(column_ngrams);
      }
      else if column_config.extractor_type == ExtractorType::WORDS {
        let mut column_words: Vec<CString> = Vec::new();
        for index  in column_config.columns.iter() {
          let value = value_values.get((*index) as usize).unwrap();
          match &value.get_external_value_ref() {
            ExternalValue::StrBox(str) => {
              extract_words_for_string(
                str.as_c_str(),
                &mut column_words
              );
            },
            _ => {}
          }
        }
        multikeys.push(column_words);
      }
      else {
        let index = column_config.columns.get(0).unwrap();
        let value = value_values.get_mut(*index as usize).unwrap();
        match &value.internal_value {
          InternalValue::ByteArrayBox(_bab) => {
            value.set_external_reference()?;
          },
          InternalValue::UInt(uint) => {
            value.set_external_unit(*uint)?;
          },
          InternalValue::None() => {
            println!("Should throw an error");
          }
        }
      }
    }
    if multikeys.len() != 0 {
      let initial_temp_values: Vec<QueryValue> = Vec::new();
      for i in 0..value_values.len() {
        let value = value_values.get_mut(i).unwrap();
        value.data_type = self.extract_format.get(i).unwrap().format;
      }
      return self.recurse_custom_fields(
        wt_result_cursor,
        &value_values,
        &multikeys,
        0,
        0,
        initial_temp_values
      );
    }
    else {
      // we only get here if there is no custom per column extractor
      let mut set_value_list: av_alist = av_alist::new();
      let set_value_list_ptr = addr_of_mut!(set_value_list);
      avcall_start_void(
        set_value_list_ptr,
        avcall_extract_fn(get_fn(unsafe {*wt_result_cursor}.set_key).unwrap() as *const())
      );
      unsafe { avcall_arg_ptr(set_value_list_ptr, wt_result_cursor as *mut c_void) };
      for column_config in self.column_configs.iter() {
        let index = column_config.columns.get(0).unwrap();
        populate_av_list_for_packing(
          value_values.get((*index) as usize).unwrap(),
          set_value_list_ptr,
          &column_config.format
        )?;
      }
      unsafe { avcall_call(set_value_list_ptr) };
      Ok(unsafe { get_fn((*wt_result_cursor).insert).unwrap()(wt_result_cursor) })
    }

    // (*wt_result_cursor).set_key.unwrap()(wt_result_cursor, unwrap_string("TODO".to_string()));
    // return (*wt_result_cursor).insert.unwrap()(wt_result_cursor);
  }

  fn extract_column_configuration(
    &mut self,
    column_config: &WT_CONFIG_ITEM
  ) -> Result<(), GlueError> {
    let cfg = char_ptr_of_length_to_string(unsafe {column_config.str_.offset(1)}, column_config.len - 2).unwrap();
    let mut config_parser: *mut WT_CONFIG_PARSER = null_mut();

    let cfg_str = unwrap_string_and_dealloc(cfg); // required until end of function
    let result = unsafe {wiredtiger_config_parser_open(
      null_mut(),
      cfg_str.as_ptr(),
      column_config.len - 2,
      addr_of_mut!(config_parser)
    )};
    if result != 0 {
      return Err(GlueError::for_wiredtiger(result));
    }

    let ngram_str = get_optional_string(config_parser, "ngrams")?;
    let columns_str = get_mandatory_string_trim_ends(config_parser, "columns")?;
    let extractor_str = get_optional_string(config_parser, "extractor")?;
    let format_str = get_mandatory_string(config_parser, "format")?;


    let columns: Vec<u32> = columns_str.split(",").map(|index| { index.to_string().parse::<u32>().unwrap() - 1 }).collect();

    let mut formats: Vec<Format> = Vec::new();
    parse_format(format_str, &mut formats)?;

    let mut column: MultiKeyExtractorColumnConfig = MultiKeyExtractorColumnConfig {
      columns,
      extractor_type: ExtractorType::NONE,
      format: formats.remove(0),
      ngrams: 0
    };

    if ngram_str.is_some() {
      column.ngrams = ngram_str.unwrap().parse().unwrap();
    }

    if extractor_str.is_some() {
      column.extractor_type = ExtractorType::from_str(extractor_str.unwrap());
    }

    self.column_configs.push(column);

    unsafe { get_fn((*config_parser).close)?(config_parser) };
    Ok(())
  }

  fn extract_columns_configuration(
    &mut self,
    config_parser: *mut WT_CONFIG_PARSER
  ) -> Result<(), GlueError> {
    let column_count_str = get_mandatory_string(config_parser, "columns")?;
    let column_count:i32 = column_count_str.as_str().parse().unwrap();
    for i in 0..column_count {
      let mut column_config: WT_CONFIG_ITEM = WT_CONFIG_ITEM::empty();
      unsafe {(*config_parser).get.unwrap()(config_parser, unwrap_string_and_dealloc(format!("column{}", i).to_string()).as_ptr(), addr_of_mut!(column_config))};
      self.extract_column_configuration(&column_config)?;
    }
    Ok(())
  }

  unsafe extern "C" fn static_terminate(
    wt_extractor: *mut WT_EXTRACTOR,
    _wt_session: *mut WT_SESSION
  ) -> i32 {
    let extractor_id = wt_extractor as u64;
    get_global_hashmap().remove(&extractor_id);
    return 0;
  }

  pub unsafe extern "C" fn static_customize(
    extractor: *mut WT_EXTRACTOR,
    session: *mut WT_SESSION,
    uri: *const ::std::os::raw::c_char,
    appcfg: *mut WT_CONFIG_ITEM,
    customp: *mut *mut WT_EXTRACTOR
  ) -> i32 {
    let result = InternalWiredTigerMultiKeyExtractor::static_customize_internal(
      extractor,
      session,
      uri,
      appcfg,
      customp
    );
    if result.is_err() {
      println!("Error: {}", result.unwrap_err());
      return -1;
    }
    return result.unwrap();
  }
  pub fn static_customize_internal(
    _extractor: *mut WT_EXTRACTOR,
    _session: *mut WT_SESSION,
    _uri: *const ::std::os::raw::c_char,
    appcfg_ptr: *mut WT_CONFIG_ITEM,
    customp: *mut *mut WT_EXTRACTOR
  ) -> Result<i32, GlueError> {
    let appcfg = unsafe {*appcfg_ptr};
    if appcfg.type_ != __wt_config_item_WT_CONFIG_ITEM_TYPE_WT_CONFIG_ITEM_STRUCT {
      return Ok(-1);
    }
    let cfg = char_ptr_of_length_to_string(unsafe {appcfg.str_.offset(1)}, appcfg.len - 2).unwrap();
    let mut config_parser: *mut WT_CONFIG_PARSER = null_mut();

    let cfg_str = unwrap_string_and_dealloc(cfg); // required until the parser is closed.
    let error = unsafe {wiredtiger_config_parser_open(
      null_mut(),
      cfg_str.as_ptr(),
      appcfg.len - 2,
      addr_of_mut!(config_parser)
    )};
    if error != 0 {
      return Ok(error);
    }

    let mut extractor: Box<InternalWiredTigerMultiKeyExtractor> = Box::new(InternalWiredTigerMultiKeyExtractor::new(
      get_mandatory_string(config_parser, "key_extract_format")?,
      get_mandatory_string(config_parser, "table_value_format")?,
      get_mandatory_string(config_parser, "index_id")?,
    )?);
    extractor.extract_columns_configuration(config_parser)?;

    unsafe {(*customp) = std::ptr::addr_of_mut!(extractor.extractor) };
    get_global_hashmap().insert(unsafe {*customp} as u64, extractor);
    unsafe { get_fn((*config_parser).close)?(config_parser) };
    return Ok(0);
  }

  unsafe extern "C" fn static_extract(
    wt_extractor: *mut WT_EXTRACTOR,
    wt_session: *mut WT_SESSION,
    wt_item_key: *const WT_ITEM,
    wt_item_value: *const WT_ITEM,
    wt_result_cursor: *mut WT_CURSOR
  ) -> i32 {
    let addr:u64 = wt_extractor as u64;
    let mut hm: MutexGuard<'_, HashMap<u64, Box<InternalWiredTigerMultiKeyExtractor>>> = get_global_hashmap();
    let extractor_ref = hm.get_mut(&addr).unwrap();
    let result = extractor_ref.extract(wt_session, wt_item_key, wt_item_value, wt_result_cursor);
    if result.is_err() {
      println!("Error: {}", result.unwrap_err());
      return -1;
    }
    return result.unwrap();
  }

}
