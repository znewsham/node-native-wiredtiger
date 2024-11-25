use std::os::raw::c_void;

use crate::external::avcall::{avcall_arg_ptr ,av_alist};

use super::error::GlueError;
use super::query_value::*;
use super::utils::field_is_wt_item;

pub fn populate_av_list_for_packing_unpacking(
  qv: &mut QueryValue,
  arg_list: *mut av_alist,
  format: &Format,
  for_write: bool
) -> Result<(), GlueError> {
  if for_write && format.format == FieldFormat::PADDING {
    return Ok(());
    // generally we shouldn't call it with a padding byte
    // this might bite us in the ass when implementing unique indexes and we need to pass in a null char to this
    // return Err(GlueError::for_glue(GlueErrorCode::NoError))
  }
  if field_is_wt_item(format.format) || format.format == FieldFormat::PADDING {
    // when writing, valueArray should be considered const and not modified
    // setting valueArray.wtItem.* relies on the shape of wtItem and queryValue being identical. Gross.
    // for reading - there is no such requirement and we dont want the cost of allocating a new WT_ITEM vector
    if for_write {
      unsafe {
        avcall_arg_ptr(
          arg_list,
          qv.setup_wt_item_for_write()? as *mut c_void
        )
      };

      return Ok(());
    }
    else {
      // we fake that a QueryValue is a WT_ITEM (they're at least the same size)
      // in populate we'll pull the data out and rebuild this item correctly
      unsafe {
        avcall_arg_ptr(
          arg_list,
          qv.get_read_ptr(format) as *mut c_void
        );
      }
      return Ok(());
    }
  }
  else {
    if for_write {
      unsafe {
        avcall_arg_ptr(
          arg_list,
          qv.ptr_or_value()? as *mut c_void
        )
      };
      return Ok(());
    }
    else {
      unsafe {
        avcall_arg_ptr(
          arg_list,
          qv.get_read_ptr(format) as *mut c_void
        );
      }
      return Ok(());
      // avcall_arg_ptr(*argList, void*, &(*valueArray)[i].value.valuePtr);
      // (*valueArray)[i].dataType = format.format;
    }
  }
}
