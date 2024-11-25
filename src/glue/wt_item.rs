use crate::{external::{avcall::{av_alist, avcall_arg_ptr, avcall_arg_uint, avcall_call}, avcall_helpers::{avcall_extract_fn, avcall_start_int}, wiredtiger::{wiredtiger_struct_unpack, WT_CONFIG_ITEM, WT_ITEM, WT_SESSION}}, glue::error::GlueErrorCode};
use std::{os::raw::c_void, ptr::{addr_of_mut, null_mut}};

use super::{avcall::populate_av_list_for_packing_unpacking, error::GlueError, query_value::{Format, QueryValue}};



impl WT_CONFIG_ITEM {
  pub fn empty() -> Self {
    return WT_CONFIG_ITEM {
      len: 0,
      str_: null_mut(),
      type_: 0,
      val: 0
    };
  }
}

impl WT_ITEM {
  pub fn empty() -> Self {
    return WT_ITEM {
      data: null_mut(),
      flags: 0,
      mem: null_mut(),
      memsize: 0,
      size: 0
    };
  }
}

pub fn unpack_wt_item(
  session: *mut WT_SESSION,
  item: *const WT_ITEM,
  cleaned_format: *mut i8,
  formats: &Vec<Format>,
  query_values: &mut Vec<QueryValue>,
  finalize: bool
) -> Result<i32, GlueError> {

  let mut arg_list: av_alist = av_alist::new();
  let arg_list_ptr = addr_of_mut!(arg_list);
  let mut error: i32 = GlueErrorCode::UnlikelyNoRun as i32;
  avcall_start_int(
    arg_list_ptr,
    avcall_extract_fn(wiredtiger_struct_unpack as *const()),
    addr_of_mut!(error)
  );
  unsafe {
    avcall_arg_ptr(arg_list_ptr, session as *mut c_void);
    avcall_arg_ptr(arg_list_ptr, (*item).data as *mut c_void);
    avcall_arg_uint(arg_list_ptr, (*item).size as u32);
    avcall_arg_ptr(arg_list_ptr, cleaned_format as *mut c_void);
  }
  for i in 0..formats.len(){
    populate_av_list_for_packing_unpacking(
      query_values.get_mut(i).unwrap(),
      arg_list_ptr,
      formats.get(i).unwrap(),
      false
    )?;
  }

  unsafe { avcall_call(arg_list_ptr) };
  if error != 0 {
    return Ok(error);
  }
  if finalize {
    for i in 0..query_values.len() {
      query_values.get_mut(i).unwrap().finalize_read(formats.get(i).unwrap())?;
    }
  }
  return Ok(error);
}
