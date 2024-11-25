
use std::borrow::BorrowMut;
use std::os::raw::c_void;
use std::ptr::addr_of_mut;

use crate::external::avcall::{av_alist, avcall_arg_ptr, avcall_call};
use crate::external::avcall_helpers::{avcall_extract_fn, avcall_start_int, avcall_start_void};
use crate::external::wiredtiger::*;

use super::avcall::populate_av_list_for_packing_unpacking;
use super::query_value::{Format, QueryValue};
use super::utils::{char_ptr_to_string, deref_ptr, get_fn, parse_format, unwrap_or_throw, unwrap_string_and_leak};
use super::error::*;

pub struct InternalWiredTigerCursor {
  // yuck. pub because the session needs it for joining :shrug:
  pub cursor: *mut WT_CURSOR,
  pub key_formats: Vec<Format>,
  pub value_formats: Vec<Format>,

  // used to track the joined cursors
  cursors: Vec<InternalWiredTigerCursor>
}

impl InternalWiredTigerCursor {
  pub fn new(cursor: *mut WT_CURSOR) -> Result<Self, GlueError> {
    let key_formats: Vec<Format> = Vec::new();
    let value_formats: Vec<Format> = Vec::new();
    let mut internal_cursor = InternalWiredTigerCursor { cursor, key_formats, value_formats, cursors: Vec::new() };
    parse_format(char_ptr_to_string(internal_cursor.get_cursor()?.key_format)?, internal_cursor.key_formats.borrow_mut())?;
    parse_format(char_ptr_to_string(internal_cursor.get_cursor()?.value_format)?, internal_cursor.value_formats.borrow_mut())?;
    Ok(internal_cursor)
  }

  pub fn empty() -> Self {
    let key_formats: Vec<Format> = Vec::new();
    let value_formats: Vec<Format> = Vec::new();
    return InternalWiredTigerCursor { cursor: std::ptr::null_mut(), key_formats, value_formats, cursors: Vec::new() };
  }

  fn get_cursor(&self) -> Result<WT_CURSOR, GlueError> {
    deref_ptr(self.cursor)
  }

  fn passthrough(&self, the_fn: Option<unsafe extern "C" fn(cursor: *mut WT_CURSOR) -> ::std::os::raw::c_int>) -> Result<(), GlueError> {
    let result = unsafe {
      get_fn(the_fn)?(self.cursor)
    };

    if result == 0 {
      return Ok(());
    }
    Err(GlueError::for_wiredtiger( result))
  }

  fn populate_get(
    &self,
    getter_fn: Option<unsafe extern "C" fn(cursor: *mut WT_CURSOR, ...) -> ::std::os::raw::c_int>,
    formats: &Vec<Format>
  ) -> Result<Vec<QueryValue>, GlueError>{
    let mut arg_list: av_alist = av_alist::new();

    let mut ret: i32 = GlueErrorCode::UnlikelyNoRun as i32;
    avcall_start_int(
      std::ptr::addr_of_mut!(arg_list),
      avcall_extract_fn(get_fn(getter_fn)? as *const()),
      std::ptr::addr_of_mut!(ret)
    );

    unsafe {
      avcall_arg_ptr(
        std::ptr::addr_of_mut!(arg_list),
        self.cursor as *mut c_void
      );
    }

    let mut values: Vec<QueryValue> = Vec::with_capacity(formats.len());
    for format in formats.iter() {
      let mut qv = QueryValue::empty();
      qv.data_type = format.format;
      values.push(qv);
    }
    for i in 0..formats.len() {
      populate_av_list_for_packing_unpacking(
        unwrap_or_throw(values.get_mut(i as usize))?,
        std::ptr::addr_of_mut!(arg_list),
        unwrap_or_throw(formats.get(i))?,
        false
      )?;
    }
    unsafe {
      avcall_call(std::ptr::addr_of_mut!(arg_list));
    };
    for i in 0..formats.len() {
      let format: &Format = formats.get(i).unwrap();
      let value: &mut QueryValue = unwrap_or_throw(values.get_mut(i as usize))?;
      value.finalize_read(format)?;
    }


    if ret == 0 {
      return Ok(values);
    }
    Err(GlueError::for_wiredtiger( ret))
  }

  fn populate_set(
    &self,
    set_fn: Option<unsafe extern "C" fn(cursor: *mut WT_CURSOR, ...)>,
    values: &mut Vec<QueryValue>,
    formats: &Vec<Format>
  ) -> Result<(), GlueError> {
    let mut arg_list: av_alist = av_alist::new();

    avcall_start_void(
      std::ptr::addr_of_mut!(arg_list),
      avcall_extract_fn(get_fn(set_fn)? as *const())
    );

    unsafe {
      avcall_arg_ptr(
        std::ptr::addr_of_mut!(arg_list),
        self.cursor as *mut c_void
      );
    }

    for i in 0..formats.len() {
      populate_av_list_for_packing_unpacking(
        unwrap_or_throw(values.get_mut(i as usize))?,
        std::ptr::addr_of_mut!(arg_list),
        unwrap_or_throw(formats.get(i))?,
        true
      )?;
    }
    unsafe {
      avcall_call(std::ptr::addr_of_mut!(arg_list));
    };
    Ok(())
  }

  pub fn set_key(&self, values: &mut Vec<QueryValue>) -> Result<(), GlueError> {
    self.populate_set(self.get_cursor()?.set_key, values, &self.key_formats)
  }

  pub fn get_key(&self) -> Result<Vec<QueryValue>, GlueError> {
    self.populate_get(self.get_cursor()?.get_key, &self.key_formats)
  }

  pub fn set_value(&self, values: &mut Vec<QueryValue>) -> Result<(), GlueError> {
    self.populate_set(self.get_cursor()?.set_value, values, &self.value_formats)
  }

  pub fn get_value(&self) -> Result<Vec<QueryValue>, GlueError> {
    self.populate_get(self.get_cursor()?.get_value, &self.value_formats)
  }

  pub fn insert(&self) -> Result<(), GlueError> {
    return self.passthrough(self.get_cursor()?.insert);
  }

  pub fn update(&self) -> Result<(), GlueError> {
    return self.passthrough(self.get_cursor()?.update);
  }

  pub fn remove(&self) -> Result<(), GlueError> {
    return self.passthrough(self.get_cursor()?.remove);
  }

  pub fn next(&self) -> Result<bool, GlueError> {
    let result = self.passthrough(self.get_cursor()?.next);
    match result {
      Err(err) => match err.error_domain {
        ErrorDomain::WIREDTIGER => {
          if err.wiredtiger_error == WT_NOTFOUND {
            return Ok(false);
          }
          return Err(err);
        },
        _ => Err(err)
      },
      _ => Ok(true)
    }
  }

  pub fn prev(&self) -> Result<bool, GlueError> {
    let result = self.passthrough(self.get_cursor()?.prev);
    match result {
      Err(err) => match err.error_domain {
        ErrorDomain::WIREDTIGER => {
          if err.wiredtiger_error == WT_NOTFOUND {
            return Ok(false);
          }
          return Err(err);
        },
        _ => Err(err)
      },
      _ => Ok(true)
    }
  }

  pub fn reset(&self) -> Result<(), GlueError> {
    return self.passthrough(self.get_cursor()?.reset);
  }

  pub fn close(&self) -> Result<(), GlueError> {
    return self.passthrough(self.get_cursor()?.close);
  }

  pub fn search(&self) -> Result<(), GlueError> {
    return self.passthrough(self.get_cursor()?.search);
  }

  pub fn search_near(&self) -> Result<i32, GlueError> {
    let mut exact: i32 = 0;
    let result = unsafe {
      get_fn(self.get_cursor()?.search_near)?(self.cursor, addr_of_mut!(exact))
    };

    if result == 0 {
      return Ok(exact);
    }
    Err(GlueError::for_wiredtiger( result))
  }

  pub fn compare(&self, cursor: &InternalWiredTigerCursor) -> Result<i32, GlueError> {
    let mut compare: i32 = 0;
    let result = unsafe {
      get_fn(self.get_cursor()?.compare)?(self.cursor, cursor.cursor, addr_of_mut!(compare))
    };

    if result == 0 {
      return Ok(compare);
    }
    Err(GlueError::for_wiredtiger( result))
  }

  pub fn equals(&self, cursor: &InternalWiredTigerCursor) -> Result<bool, GlueError> {
    let mut equals: i32 = 0;
    let result = unsafe {
      get_fn(self.get_cursor()?.equals)?(self.cursor, cursor.cursor, addr_of_mut!(equals))
    };

    if result == 0 {
      return Ok(equals == 1);
    }
    Err(GlueError::for_wiredtiger( result))
  }

  pub fn bound(&self, config: String) -> Result<(), GlueError> {
    let result = unsafe {
      // TODO: figure out if this can be dealloc'd or if we need to cache it like we do with set_key at the binding layer
      get_fn(self.get_cursor()?.bound)?(self.cursor, unwrap_string_and_leak(config))
    };

    if result == 0 {
      return Ok(());
    }
    Err(GlueError::for_wiredtiger( result))
  }

  pub fn get_key_format(&self) -> Result<String, GlueError> {
    char_ptr_to_string(self.get_cursor()?.key_format)
  }

  pub fn get_value_format(&self) -> Result<String, GlueError> {
    char_ptr_to_string(self.get_cursor()?.value_format)
  }

  pub fn get_key_formats(&self) -> &Vec<Format> {
    return &self.key_formats;
  }

  pub fn get_value_formats(&self) -> &Vec<Format> {
    return &self.value_formats;
  }

  pub fn own_joined_cursor(&mut self, cursor: InternalWiredTigerCursor) -> &mut InternalWiredTigerCursor {
    self.cursors.push(cursor);
    return self.cursors.last_mut().unwrap();
  }
}
