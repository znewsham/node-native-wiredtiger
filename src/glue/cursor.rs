
use std::borrow::BorrowMut;
use std::mem::transmute;
use std::os::raw::c_void;
use std::ptr::addr_of_mut;

use crate::external::avcall::{__avrword, __avword, av_alist, avcall_arg_ptr, avcall_call};
use crate::external::avcall_helpers::{avcall_start_int, avcall_start_void};
use crate::external::wiredtiger::*;

use super::avcall::{populate_av_list_for_packing, populate_av_list_for_unpacking};
use super::cursor_trait::InternalCursorTrait;
use super::query_value::{FieldFormat, Format, QueryValue};
use super::utils::{char_ptr_to_string, deref_ptr, get_fn, parse_format, unwrap_or_throw, unwrap_string_and_leak};
use super::error::*;


struct ActualCursor {
  get_key: unsafe extern "C" fn() -> __avrword,
  get_value: unsafe extern "C" fn() -> __avrword,
  set_key: unsafe extern "C" fn() -> __avrword,
  set_value: unsafe extern "C" fn() -> __avrword,
  reset: unsafe extern "C" fn(cursor: *mut WT_CURSOR) -> ::std::os::raw::c_int,
  search: unsafe extern "C" fn(cursor: *mut WT_CURSOR) -> ::std::os::raw::c_int
}

unsafe extern "C" fn fake_avcall() -> __avword {
  return 0;
}

unsafe extern "C" fn fake_simple (_cursor: *mut WT_CURSOR) -> ::std::os::raw::c_int {
  return 0;
}


pub fn extract_cursor_getter(
  fn_call: *const ()
) -> unsafe extern "C" fn(_cursor: *mut WT_CURSOR) -> ::std::os::raw::c_int {
  let set_key: unsafe extern "C" fn(_cursor: *mut WT_CURSOR) -> ::std::os::raw::c_int = unsafe { transmute(fn_call) };
  return set_key;
}

pub fn extract_cursor_avcall(
  fn_call: *const ()
) -> unsafe extern "C" fn() -> __avword {
  let set_key: unsafe extern "C" fn() -> __avword = unsafe { transmute(fn_call) };
  return set_key;
}

impl ActualCursor {
  fn empty() -> Self {
    return ActualCursor {
      get_key: fake_avcall,
      get_value: fake_avcall,
      set_key: fake_avcall,
      set_value: fake_avcall,
      search: fake_simple,
      reset: fake_simple
    };
  }
}

pub struct InternalCursor {
  // yuck. pub because the session needs it for joining :shrug:
  pub cursor: *mut WT_CURSOR,
  key_formats: Vec<Format>,
  value_formats: Vec<Format>,
  actual_cursor: ActualCursor,

  // used to track the joined cursors
  cursors: Vec<InternalCursor>
}

impl InternalCursorTrait for InternalCursor {
  fn next(&self) -> Result<bool, GlueError> {
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

  fn prev(&self) -> Result<bool, GlueError> {
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

  fn reset(&mut self) -> Result<(), GlueError> {
    return self.passthrough_raw(self.actual_cursor.reset);
  }

  fn close(&mut self) -> Result<(), GlueError> {
    return self.passthrough(self.get_cursor()?.close);
  }

  fn get_key(&self) -> Result<Vec<QueryValue>, GlueError> {
    self.populate_get(self.actual_cursor.get_key, &self.key_formats)
  }

  fn get_value(&self) -> Result<Vec<QueryValue>, GlueError> {
    self.populate_get(self.actual_cursor.get_value, &self.value_formats)
  }



  fn get_key_format(&self) -> Result<String, GlueError> {
    char_ptr_to_string(self.get_cursor()?.key_format)
  }

  fn get_value_format(&self) -> Result<String, GlueError> {
    char_ptr_to_string(self.get_cursor()?.value_format)
  }

  fn get_key_formats(&self) -> &Vec<Format> {
    return &self.key_formats;
  }

  fn get_value_formats(&self) -> &Vec<Format> {
    return &self.value_formats;
  }

  fn compare(&self, cursor: &impl InternalCursorTrait) -> Result<i32, GlueError> {
    let mut compare: i32 = 0;
    let result = unsafe {
      get_fn(self.get_cursor()?.compare)?(self.cursor, cursor.get_raw_cursor_ptr(), addr_of_mut!(compare))
    };

    if result == 0 {
      return Ok(compare);
    }
    Err(GlueError::for_wiredtiger( result))
  }

  fn equals(&self, cursor: &impl InternalCursorTrait) -> Result<bool, GlueError> {
    let mut equals: i32 = 0;
    let result = unsafe {
      get_fn(self.get_cursor()?.equals)?(self.cursor, cursor.get_raw_cursor_ptr(), addr_of_mut!(equals))
    };

    if result == 0 {
      return Ok(equals == 1);
    }
    Err(GlueError::for_wiredtiger( result))
  }

  fn get_raw_cursor_ptr(&self) -> *mut WT_CURSOR {
    return self.cursor;
  }
}



impl InternalCursor {
  pub fn new(cursor_ptr: *mut WT_CURSOR) -> Result<Self, GlueError> {
    let key_formats: Vec<Format> = Vec::new();
    let value_formats: Vec<Format> = Vec::new();
    let cursor = deref_ptr(cursor_ptr).unwrap();
    let mut internal_cursor = InternalCursor {
      cursor: cursor_ptr,
      key_formats,
      value_formats,
      cursors: Vec::new(),
      actual_cursor: ActualCursor {
        get_key: extract_cursor_avcall(get_fn(cursor.get_key).unwrap() as *const()),
        set_key: extract_cursor_avcall(get_fn(cursor.set_key).unwrap() as *const()),
        get_value: extract_cursor_avcall(get_fn(cursor.get_value).unwrap() as *const()),
        set_value: extract_cursor_avcall(get_fn(cursor.set_value).unwrap() as *const()),
        search: extract_cursor_getter(get_fn(cursor.search).unwrap() as *const()),
        reset: extract_cursor_getter(get_fn(cursor.reset).unwrap() as *const()),
      }
    };
    parse_format(char_ptr_to_string(internal_cursor.get_cursor()?.key_format)?, internal_cursor.key_formats.borrow_mut())?;
    parse_format(char_ptr_to_string(internal_cursor.get_cursor()?.value_format)?, internal_cursor.value_formats.borrow_mut())?;
    Ok(internal_cursor)
  }

  pub fn empty() -> Self {
    let key_formats: Vec<Format> = Vec::new();
    let value_formats: Vec<Format> = Vec::new();
    return InternalCursor { cursor: std::ptr::null_mut(), key_formats, value_formats, cursors: Vec::new(), actual_cursor: ActualCursor::empty() };
  }

  #[inline(always)]
  fn get_cursor(&self) -> Result<WT_CURSOR, GlueError> {
    deref_ptr(self.cursor)
  }

  #[inline(always)]
  fn passthrough(&self, the_fn: Option<unsafe extern "C" fn(cursor: *mut WT_CURSOR) -> ::std::os::raw::c_int>) -> Result<(), GlueError> {
    let result = unsafe {
      get_fn(the_fn)?(self.cursor)
    };

    if result == 0 {
      return Ok(());
    }
    Err(GlueError::for_wiredtiger( result))
  }

  #[inline(always)]
  fn passthrough_raw(&self, the_fn: unsafe extern "C" fn(cursor: *mut WT_CURSOR) -> ::std::os::raw::c_int) -> Result<(), GlueError> {
    let result = unsafe {
      the_fn(self.cursor)
    };

    if result == 0 {
      return Ok(());
    }
    Err(GlueError::for_wiredtiger( result))
  }

  #[inline(always)]
  fn populate_get_n(
    &self,
    getter_fn: unsafe extern "C" fn(...) -> i32,
    formats: &Vec<Format>,
    values: &mut Vec<QueryValue>
  ) -> i32 {
    if formats.len() == 1 {
      return unsafe {
        getter_fn(
          self.cursor,
          values.get_mut(0).unwrap().get_read_ptr(formats.get(0).unwrap())
        )
      }
    }
    else if formats.len() == 2 {
      return unsafe {
        getter_fn(
          self.cursor,
          values.get_mut(0).unwrap().get_read_ptr(formats.get(0).unwrap()),
          values.get_mut(1).unwrap().get_read_ptr(formats.get(1).unwrap())
        )
      }
    }
    else if formats.len() == 3 {
      return unsafe {
        getter_fn(
          self.cursor,
          values.get_mut(0).unwrap().get_read_ptr(formats.get(0).unwrap()),
          values.get_mut(1).unwrap().get_read_ptr(formats.get(1).unwrap()),
          values.get_mut(2).unwrap().get_read_ptr(formats.get(2).unwrap())
        )
      }
    }
    else if formats.len() == 4 {
     return unsafe {
        getter_fn(
          self.cursor,
          values.get_mut(0).unwrap().get_read_ptr(formats.get(0).unwrap()),
          values.get_mut(1).unwrap().get_read_ptr(formats.get(1).unwrap()),
          values.get_mut(2).unwrap().get_read_ptr(formats.get(2).unwrap()),
          values.get_mut(3).unwrap().get_read_ptr(formats.get(3).unwrap())
        )
      }
    }
    else if formats.len() == 5 {
      return unsafe {
        getter_fn(
          self.cursor,
          values.get_mut(0).unwrap().get_read_ptr(formats.get(0).unwrap()),
          values.get_mut(1).unwrap().get_read_ptr(formats.get(1).unwrap()),
          values.get_mut(2).unwrap().get_read_ptr(formats.get(2).unwrap()),
          values.get_mut(3).unwrap().get_read_ptr(formats.get(3).unwrap()),
          values.get_mut(4).unwrap().get_read_ptr(formats.get(4).unwrap())
        )
      }
    }
    return 0;
  }

  #[inline(always)]
  fn populate_get(
    &self,
    getter_fn: unsafe extern "C" fn() -> __avword,
    formats: &Vec<Format>
  ) -> Result<Vec<QueryValue>, GlueError>{
    let mut arg_list: av_alist = av_alist::new();

    let mut ret: i32 = GlueErrorCode::UnlikelyNoRun as i32;

    let mut values: Vec<QueryValue> = Vec::with_capacity(formats.len());
    for format in formats.iter() {
      let mut qv = QueryValue::empty();
      qv.data_type = format.format;
      values.push(qv);
    }
    if values.len() <= 5 {
      ret = self.populate_get_n(unsafe { transmute(getter_fn as *const()) }, formats, &mut values);
    }
    else {
      avcall_start_int(
        std::ptr::addr_of_mut!(arg_list),
        getter_fn,
        std::ptr::addr_of_mut!(ret)
      );

      unsafe {
        avcall_arg_ptr(
          std::ptr::addr_of_mut!(arg_list),
          self.cursor as *mut c_void
        );
      }
      for i in 0..formats.len() {
        populate_av_list_for_unpacking(
          unwrap_or_throw(values.get_mut(i as usize))?,
          std::ptr::addr_of_mut!(arg_list),
          unwrap_or_throw(formats.get(i))?
        )?;
      }
      unsafe {
        avcall_call(std::ptr::addr_of_mut!(arg_list));
      };
    }
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

  #[inline(always)]
  fn populate_set_n(
    &self,
    setter_fn: unsafe extern "C" fn(...) -> i32,
    formats: &Vec<Format>,
    values: &Vec<QueryValue>
  ) -> Result<i32, GlueError> {
    if formats.len() == 1 {
      return Ok(unsafe {
        setter_fn(
          self.cursor,
          values.get(0).unwrap().ptr_or_value_single(formats.get(0).unwrap())?
        )
      })
    }
    else if formats.len() == 2 {
      return Ok(unsafe {
        setter_fn(
          self.cursor,
          values.get(0).unwrap().ptr_or_value_single(formats.get(0).unwrap())?,
          values.get(1).unwrap().ptr_or_value_single(formats.get(1).unwrap())?
        )
      })
    }
    else if formats.len() == 3 {
      return Ok(unsafe {
        setter_fn(
          self.cursor,
          values.get(0).unwrap().ptr_or_value_single(formats.get(0).unwrap())?,
          values.get(1).unwrap().ptr_or_value_single(formats.get(1).unwrap())?,
          values.get(2).unwrap().ptr_or_value_single(formats.get(2).unwrap())?
        )
      })
    }
    else if formats.len() == 4 {
     return Ok(unsafe {
        setter_fn(
          self.cursor,
          values.get(0).unwrap().ptr_or_value_single(formats.get(0).unwrap())?,
          values.get(1).unwrap().ptr_or_value_single(formats.get(1).unwrap())?,
          values.get(2).unwrap().ptr_or_value_single(formats.get(2).unwrap())?,
          values.get(3).unwrap().ptr_or_value_single(formats.get(3).unwrap())?
        )
      })
    }
    else if formats.len() == 5 {
      return Ok(unsafe {
        setter_fn(
          self.cursor,
          values.get(0).unwrap().ptr_or_value_single(formats.get(0).unwrap())?,
          values.get(1).unwrap().ptr_or_value_single(formats.get(1).unwrap())?,
          values.get(2).unwrap().ptr_or_value_single(formats.get(2).unwrap())?,
          values.get(3).unwrap().ptr_or_value_single(formats.get(3).unwrap())?,
          values.get(4).unwrap().ptr_or_value_single(formats.get(4).unwrap())?
        )
      })
    }
    return Ok(0);
  }

  #[inline(always)]
  fn populate_set(
    &self,
    set_fn: unsafe extern "C" fn() -> __avword,
    values: &Vec<QueryValue>,
    formats: &Vec<Format>
  ) -> Result<(), GlueError> {
    let mut arg_list: av_alist = av_alist::new();

    if formats.len() <= 5 {
      self.populate_set_n(unsafe { transmute(set_fn as *const()) }, formats, values)?;
    }
    else {
      avcall_start_void(
        std::ptr::addr_of_mut!(arg_list),
        set_fn
      );

      unsafe {
        avcall_arg_ptr(
          std::ptr::addr_of_mut!(arg_list),
          self.cursor as *mut c_void
        );
      }

      for i in 0..formats.len() {
        let format = unwrap_or_throw(formats.get(i))?;
        if format.format == FieldFormat::PADDING {
          // when setting the key of an index cursor, there will be a padding byte at the end that should be ignored (it represents the row ID)
          continue;
        }
        populate_av_list_for_packing(
          unwrap_or_throw(values.get(i as usize))?,
          std::ptr::addr_of_mut!(arg_list),
          format
        )?;
      }
      unsafe {
        avcall_call(std::ptr::addr_of_mut!(arg_list));
      };
    }
    Ok(())
  }

  pub fn set_key(&self, values: &Vec<QueryValue>) -> Result<(), GlueError> {
    self.populate_set(self.actual_cursor.set_key, values, &self.key_formats)
  }

  pub fn set_value(&self, values: &mut Vec<QueryValue>) -> Result<(), GlueError> {
    self.populate_set(self.actual_cursor.set_value, values, &self.value_formats)
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

  pub fn search(&self) -> Result<bool, GlueError> {
    let result = self.passthrough_raw(self.actual_cursor.search);
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
  pub fn own_joined_cursor(&mut self, cursor: InternalCursor) -> &mut InternalCursor {
    self.cursors.push(cursor);
    return self.cursors.last_mut().unwrap();
  }
}
