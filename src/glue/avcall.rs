use std::os::raw::c_void;

use crate::external::avcall::{avcall_arg_ptr ,av_alist};

use super::error::GlueError;
use super::query_value::*;

pub fn populate_av_list_for_packing(
  qv: &QueryValue,
  arg_list: *mut av_alist,
  format: &Format
) -> Result<(), GlueError> {
  if format.format == FieldFormat::PADDING {
    return Ok(());
  }
  unsafe {
    avcall_arg_ptr(
      arg_list,
      qv.ptr_or_value_single(format)? as *mut c_void
    )
  };
  return Ok(());
}

pub fn populate_av_list_for_unpacking(
  qv: &mut QueryValue,
  arg_list: *mut av_alist,
  format: &Format
) -> Result<(), GlueError> {
  unsafe {
    avcall_arg_ptr(
      arg_list,
      qv.get_read_ptr(format) as *mut c_void
    );
  }
  return Ok(());
}
