use std::collections::HashMap;
use std::ffi::CString;
use std::ptr::{addr_of_mut, null_mut};
use std::sync::{Mutex, MutexGuard, OnceLock};
use lazy_static::lazy_static;
use libc::{strcmp, strncmp};

use crate::external::wiredtiger::*;

use super::error::GlueError;
use super::query_value::{Format, QueryValue};
use super::utils::{char_ptr_of_length_to_string, field_is_wt_item, get_fn, get_mandatory_string, get_optional_boolean, parse_format, unwrap_string_and_dealloc};
use super::wt_item::unpack_wt_item;

#[derive(PartialEq, Copy, Clone)]
enum CollatorDirection {
  REVERSE = -1,
  NONE = 0,
  FORWARD = 1
}

impl From<CollatorDirection> for i32 {
  fn from(value: CollatorDirection) -> Self {
    match value {
      CollatorDirection::FORWARD => 1,
      CollatorDirection::NONE => 0,
      CollatorDirection::REVERSE => -1
    }
  }
}

impl From<i32> for CollatorDirection {
  fn from(value: i32) -> Self {
    match value {
      1 => CollatorDirection::FORWARD,
      0 => CollatorDirection::NONE,
      -1 => CollatorDirection::REVERSE,
      _ => CollatorDirection::NONE
    }
  }
}

struct CompoundDirectionalCollatorColumnConfig {
  format: Format,
  direction: CollatorDirection
}

pub struct InternalWiredTigerCompoundDirectionalCollator {
  collator: WT_COLLATOR,
  direction: CollatorDirection,
  column_configs: Vec<CompoundDirectionalCollatorColumnConfig>,
  compare_format: Vec<Format>,
  unique: bool,
  key_format_cstr: CString
}


fn get_global_hashmap() -> MutexGuard<'static, HashMap<u64, Box<InternalWiredTigerCompoundDirectionalCollator>>> {
  static MAP: OnceLock<Mutex<HashMap<u64, Box<InternalWiredTigerCompoundDirectionalCollator>>>> = OnceLock::new();
  MAP.get_or_init(|| Mutex::new(HashMap::new()))
      .lock()
      .expect("Let's hope the lock isn't poisoned")
}


lazy_static! {
  static ref HASHMAP: HashMap<u64, Box<InternalWiredTigerCompoundDirectionalCollator>> = HashMap::new();
}


impl InternalWiredTigerCompoundDirectionalCollator {
  pub fn new(
    key_format: String,
    _index_id: String
  ) -> Result<Self, GlueError> {
    let mut compare_format: Vec<Format> = Vec::new();
    parse_format(key_format.clone(), &mut compare_format)?;
    return Ok(InternalWiredTigerCompoundDirectionalCollator {
      column_configs: Vec::new(),
      unique: false,
      // TODO: both the cstr probably need to be freed on drop
      key_format_cstr: unwrap_string_and_dealloc(key_format),
      direction: CollatorDirection::NONE,
      compare_format,
      collator: WT_COLLATOR {
        customize: None,
        compare: Some(InternalWiredTigerCompoundDirectionalCollator::static_compare),
        terminate: Some(InternalWiredTigerCompoundDirectionalCollator::static_terminate)
      }
    });
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

    let format_str = get_mandatory_string(config_parser, "format")?;
    let direction_str = get_mandatory_string(config_parser, "direction")?;


    let mut formats: Vec<Format> = Vec::new();
    parse_format(format_str, &mut formats)?;

    let column: CompoundDirectionalCollatorColumnConfig = CompoundDirectionalCollatorColumnConfig {
      direction: CollatorDirection::from(direction_str.parse::<i32>().unwrap()),
      format: formats.remove(0)
    };

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
    let mut same_direction: bool = true;
    let mut last_direction: &CollatorDirection = &CollatorDirection::NONE;
    for column in self.column_configs.iter() {
      if &column.direction != last_direction && last_direction != &CollatorDirection::NONE {
        same_direction = false;
        break;
      }
      last_direction = &column.direction;
    }
    if same_direction {
      self.direction = *last_direction;
    }
    if !self.unique {
      let key_format = self.compare_format.last().unwrap();
      self.column_configs.push(CompoundDirectionalCollatorColumnConfig {
        direction: *last_direction,
        // TODO:
        format: Format { format: key_format.format, size: key_format.size }
      });
    }
    Ok(())
  }

  unsafe extern "C" fn static_terminate(
    wt_collator: *mut WT_COLLATOR,
    _wt_session: *mut WT_SESSION
  ) -> i32 {
    let collator_id: u64 = wt_collator as u64;
    get_global_hashmap().remove(&collator_id);
    return 0;
  }

  pub unsafe extern "C" fn static_customize(
    collator: *mut WT_COLLATOR,
    session: *mut WT_SESSION,
    uri: *const ::std::os::raw::c_char,
    appcfg: *mut WT_CONFIG_ITEM,
    customp: *mut *mut WT_COLLATOR
  ) -> i32 {
    let result = InternalWiredTigerCompoundDirectionalCollator::static_customize_internal(
      collator,
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
    _collator: *mut WT_COLLATOR,
    _session: *mut WT_SESSION,
    _uri: *const ::std::os::raw::c_char,
    appcfg_ptr: *mut WT_CONFIG_ITEM,
    customp: *mut *mut WT_COLLATOR
  ) -> Result<i32, GlueError> {
    let appcfg = unsafe {*appcfg_ptr};
    if appcfg.type_ != __wt_config_item_WT_CONFIG_ITEM_TYPE_WT_CONFIG_ITEM_STRUCT {
      return Ok(-1);
    }
    let cfg = char_ptr_of_length_to_string(unsafe {appcfg.str_.offset(1)}, appcfg.len - 2).unwrap();
    let mut config_parser: *mut WT_CONFIG_PARSER = null_mut();

    let cstr = unwrap_string_and_dealloc(cfg); // required until end of function

    let error = unsafe {wiredtiger_config_parser_open(
      null_mut(),
      cstr.as_ptr(),
      appcfg.len - 2,
      addr_of_mut!(config_parser)
    )};
    if error != 0 {
      return Ok(error);
    }

    let mut collator = Box::new(InternalWiredTigerCompoundDirectionalCollator::new(
      get_mandatory_string(config_parser, "key_format")?,
      get_mandatory_string(config_parser, "index_id")?,
    )?);
    collator.unique = get_optional_boolean(config_parser, "unique")?;
    collator.extract_columns_configuration(config_parser)?;

    unsafe {(*customp) = std::ptr::addr_of_mut!(collator.collator) };
    get_global_hashmap().insert(unsafe {*customp} as u64, collator);
    unsafe { get_fn((*config_parser).close)?(config_parser) };
    return Ok(0);
  }

  fn compare(
    &self,
    wt_session: *mut WT_SESSION,

    // these keys are the entire key - e.g., any columns we specify PLUS the id field.
    key1: *const WT_ITEM,
    key2: *const WT_ITEM,
    compare: *mut i32
  ) -> Result<i32, GlueError> {
    if self.direction == CollatorDirection::FORWARD {
      unsafe { *compare = wiredtiger_lex_compare(key1, key2); }
      return Ok(0);
    }
    if self.direction == CollatorDirection::REVERSE {
      unsafe { *compare = -wiredtiger_lex_compare(key1, key2); }
      return Ok(0);
    }


    let mut key1_values: Vec<QueryValue> = vec![QueryValue::empty(); self.compare_format.len()];
    let mut key2_values: Vec<QueryValue> = vec![QueryValue::empty(); self.compare_format.len()];

    let mut error = unpack_wt_item(
      wt_session,
      key1,
      self.key_format_cstr.as_ptr() as *mut i8,
      &self.compare_format,
      &mut key1_values,
      false
    )?;
    if error != 0 {
      return Err(GlueError::for_wiredtiger(error));
    }
    error = unpack_wt_item(
      wt_session,
      key2,
      self.key_format_cstr.as_ptr() as *mut i8,
      &self.compare_format,
      &mut key2_values,
      false
    )?;
    if error != 0 {
      return Err(GlueError::for_wiredtiger(error));
    }

    for i in 0..self.column_configs.len() {
      let column = self.column_configs.get(i).unwrap();
      let direction = &column.direction;
      let format_char = column.format.format;
      let mut cmp;
      let key1_value = key1_values.get(i).unwrap();
      let key2_value = key2_values.get(i).unwrap();
      let key1_ptr = key1_value.get_read_ptr_for_read(&column.format);
      let key2_ptr = key2_value.get_read_ptr_for_read(&column.format);
      if format_char == 'S' {
        cmp = unsafe { strcmp(key1_ptr as *const i8, key2_ptr as *const i8) };
        if cmp > 0 {
          cmp = 1;
        }
        else if cmp < 0 {
          cmp = -1;
        }
      }
      else if format_char == 's' {
        cmp = unsafe { strncmp(key1_ptr as *const i8, key2_ptr as *const i8, column.format.size as usize) };
        if cmp > 0 { cmp = 1; }
        else if cmp < 0 { cmp = -1; }
      }
      else if field_is_wt_item(format_char) {
        let item1: *const WT_ITEM = key1_ptr as *const WT_ITEM;
        let item2: *const WT_ITEM = key2_ptr as *const WT_ITEM;

        // CHECK: does & work here? Or are we saved by the unsafe?
        cmp = unsafe { wiredtiger_lex_compare(item1, item2) };
      }
      else {
        // numeric comparison
        // TODO - I don't think this is quite right with signed vs unsigned
        cmp = ((key1_ptr as u64) - (key2_ptr as u64)) as i32;
        if cmp > 0 { cmp = 1; }
        else if cmp < 0 { cmp = -1; }
      }

      if cmp != 0 {
        unsafe { *compare = if direction == &CollatorDirection::FORWARD { cmp } else { -cmp }};
        return Ok(0);
      }
    }
    unsafe { *compare = 0 };
    return Ok(0);
  }

  unsafe extern "C" fn static_compare(
    wt_collator: *mut WT_COLLATOR,
    wt_session: *mut WT_SESSION,
    v1: *const WT_ITEM,
    v2: *const WT_ITEM,
    compare: *mut i32
  ) -> i32 {
    let addr:u64 = wt_collator as u64;
    let mut hm: MutexGuard<'_, HashMap<u64, Box<InternalWiredTigerCompoundDirectionalCollator>>> = get_global_hashmap();
    let collator_ref = hm.get_mut(&addr).unwrap();
    let result = collator_ref.compare(wt_session, v1, v2, compare);
    if result.is_err() {
      println!("Error: {}", result.unwrap_err());
      return -1;
    }
    return result.unwrap();
  }

}
