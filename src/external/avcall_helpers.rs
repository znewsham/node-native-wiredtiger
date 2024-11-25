
/*
  avcall_start(
    &(LIST),
    (LIST)._av_alist_flexarray._av_m_args,
    &(LIST)._av_alist_flexarray._av_m_args[__AV_ALIST_WORDS],
    (__avrword(*)())(FUNC),
    0,
    __AVvoid,
    __AV_START_FLAGS
  )
*/

use std::{mem::transmute, os::raw::c_void, ptr::null_mut};

use super::avcall::{__AV_alist_flags___AV_CLEANUP, __AV_alist_flags___AV_FLOAT_ARGS, __AV_alist_flags___AV_FLOAT_RETURN, __AV_alist_flags___AV_STRUCT_ARGS, __AV_alist_flags___AV_STRUCT_RETURN, __AVtype, __AVtype___AVint, __AVtype___AVvoid, __avrword, av_alist, av_alist__bindgen_ty_1, av_alist__bindgen_ty_2, avcall_start};

const __AV_START_FLAGS: __AVtype = __AV_alist_flags___AV_STRUCT_RETURN
  | __AV_alist_flags___AV_FLOAT_RETURN
  | __AV_alist_flags___AV_STRUCT_ARGS
  | __AV_alist_flags___AV_FLOAT_ARGS
  | __AV_alist_flags___AV_CLEANUP;

pub fn avcall_start_void(
  arg_list_ptr: *mut av_alist,
  fn_call: unsafe extern "C" fn() -> __avrword
) -> () {
  let mut arg_list: av_alist = unsafe { *arg_list_ptr };

  unsafe {
    avcall_start(
      arg_list_ptr,
      arg_list._av_alist_flexarray._av_m_args.as_mut_ptr(),
      arg_list._av_alist_flexarray._av_m_args.as_mut_ptr().offset(255),
      Option::Some(fn_call),
      null_mut(),
      __AVtype___AVvoid as std::os::raw::c_int,
      __AV_START_FLAGS as std::os::raw::c_int
    )
  }
}

pub fn avcall_start_int(
  arg_list_ptr: *mut av_alist,
  fn_call: unsafe extern "C" fn() -> __avrword,
  ret: *mut i32
) -> () {
  let mut arg_list: av_alist = unsafe { *arg_list_ptr };
  unsafe {
    avcall_start(
      arg_list_ptr,
      arg_list._av_alist_flexarray._av_m_args.as_mut_ptr(),
      arg_list._av_alist_flexarray._av_m_args.as_mut_ptr().offset(255),
      Option::Some(fn_call),
      ret as *mut c_void,
      __AVtype___AVint as std::os::raw::c_int,
      __AV_START_FLAGS as std::os::raw::c_int
    )
  }
  return;
}


pub fn avcall_extract_fn(
  fn_call: *const ()
) -> unsafe extern "C" fn() -> __avrword {
  let set_key: unsafe extern "C" fn() -> __avrword = unsafe { transmute(fn_call) };
  return set_key;
}


impl av_alist {
  pub fn new() -> Self {
    av_alist {
      _av_alist_flexarray: av_alist__bindgen_ty_2 {
        align4: 0
      },
      _av_alist_head: av_alist__bindgen_ty_1 {
        align4: 0
      }
    }
  }
}
