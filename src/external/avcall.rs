/* automatically generated by rust-bindgen 0.59.1 */
#![allow(dead_code)]
#![allow(non_snake_case)]
#![allow(non_camel_case_types)]
#![allow(unused_imports)]
#![allow(non_upper_case_globals)]
#![allow(warnings)]

pub const LIBFFCALL_VERSION: u32 = 517;
pub const __AV_ALIST_WORDS: u32 = 256;
pub const __AV_ALIST_SIZE_BOUND: u32 = 256;
pub type size_t = ::std::os::raw::c_ulong;
pub type wchar_t = ::std::os::raw::c_int;
#[repr(C)]
#[repr(align(16))]
#[derive(Debug, Copy, Clone)]
pub struct max_align_t {
  pub __clang_max_align_nonce1: ::std::os::raw::c_longlong,
  pub __bindgen_padding_0: u64,
  pub __clang_max_align_nonce2: u128,
}
#[test]
fn bindgen_test_layout_max_align_t() {
  assert_eq!(
    ::std::mem::size_of::<max_align_t>(),
    32usize,
    concat!("Size of: ", stringify!(max_align_t))
  );
  assert_eq!(
    ::std::mem::align_of::<max_align_t>(),
    16usize,
    concat!("Alignment of ", stringify!(max_align_t))
  );
  assert_eq!(
    unsafe {
      &(*(::std::ptr::null::<max_align_t>())).__clang_max_align_nonce1 as *const _ as usize
    },
    0usize,
    concat!(
      "Offset of field: ",
      stringify!(max_align_t),
      "::",
      stringify!(__clang_max_align_nonce1)
    )
  );
  assert_eq!(
    unsafe {
      &(*(::std::ptr::null::<max_align_t>())).__clang_max_align_nonce2 as *const _ as usize
    },
    16usize,
    concat!(
      "Offset of field: ",
      stringify!(max_align_t),
      "::",
      stringify!(__clang_max_align_nonce2)
    )
  );
}
extern "C" {
  pub fn ffcall_get_version() -> ::std::os::raw::c_int;
}
pub type __avword = ::std::os::raw::c_long;
pub type __avrword = ::std::os::raw::c_long;
pub const __AVtype___AVword: __AVtype = 0;
pub const __AVtype___AVvoid: __AVtype = 1;
pub const __AVtype___AVchar: __AVtype = 2;
pub const __AVtype___AVschar: __AVtype = 3;
pub const __AVtype___AVuchar: __AVtype = 4;
pub const __AVtype___AVshort: __AVtype = 5;
pub const __AVtype___AVushort: __AVtype = 6;
pub const __AVtype___AVint: __AVtype = 7;
pub const __AVtype___AVuint: __AVtype = 8;
pub const __AVtype___AVlong: __AVtype = 9;
pub const __AVtype___AVulong: __AVtype = 10;
pub const __AVtype___AVlonglong: __AVtype = 11;
pub const __AVtype___AVulonglong: __AVtype = 12;
pub const __AVtype___AVfloat: __AVtype = 13;
pub const __AVtype___AVdouble: __AVtype = 14;
pub const __AVtype___AVvoidp: __AVtype = 15;
pub const __AVtype___AVstruct: __AVtype = 16;
pub type __AVtype = ::std::os::raw::c_uint;
pub const __AV_alist_flags___AV_SMALL_STRUCT_RETURN: __AV_alist_flags = 2;
pub const __AV_alist_flags___AV_GCC_STRUCT_RETURN: __AV_alist_flags = 4;
pub const __AV_alist_flags___AV_STRUCT_RETURN: __AV_alist_flags = 6;
pub const __AV_alist_flags___AV_FLOAT_RETURN: __AV_alist_flags = 0;
pub const __AV_alist_flags___AV_STRUCT_ARGS: __AV_alist_flags = 0;
pub const __AV_alist_flags___AV_FLOAT_ARGS: __AV_alist_flags = 0;
pub const __AV_alist_flags___AV_ANSI_INTEGERS: __AV_alist_flags = 0;
pub const __AV_alist_flags___AV_TRADITIONAL_INTEGERS: __AV_alist_flags = 0;
pub const __AV_alist_flags___AV_CDECL_CLEANUP: __AV_alist_flags = 0;
pub const __AV_alist_flags___AV_STDCALL_CLEANUP: __AV_alist_flags = 0;
pub const __AV_alist_flags___AV_CLEANUP: __AV_alist_flags = 0;
pub const __AV_alist_flags___AV_REGISTER_STRUCT_RETURN: __AV_alist_flags = 512;
pub const __AV_alist_flags___AV_flag_for_broken_compilers_that_dont_like_trailing_commas:
  __AV_alist_flags = 513;
pub type __AV_alist_flags = ::std::os::raw::c_uint;
#[repr(C)]
#[repr(align(16))]
#[derive(Copy, Clone)]
pub struct av_alist {
  pub _av_alist_head: av_alist__bindgen_ty_1,
  pub _av_alist_flexarray: av_alist__bindgen_ty_2,
}
#[repr(C)]
#[repr(align(16))]
#[derive(Copy, Clone)]
pub union av_alist__bindgen_ty_1 {
  pub _av_m_room: [::std::os::raw::c_char; 256usize],
  pub align1: ::std::os::raw::c_long,
  pub align2: f64,
  pub align3: ::std::os::raw::c_longlong,
  pub align4: u128,
}
#[test]
fn bindgen_test_layout_av_alist__bindgen_ty_1() {
  assert_eq!(
    ::std::mem::size_of::<av_alist__bindgen_ty_1>(),
    256usize,
    concat!("Size of: ", stringify!(av_alist__bindgen_ty_1))
  );
  assert_eq!(
    ::std::mem::align_of::<av_alist__bindgen_ty_1>(),
    16usize,
    concat!("Alignment of ", stringify!(av_alist__bindgen_ty_1))
  );
  assert_eq!(
    unsafe { &(*(::std::ptr::null::<av_alist__bindgen_ty_1>()))._av_m_room as *const _ as usize },
    0usize,
    concat!(
      "Offset of field: ",
      stringify!(av_alist__bindgen_ty_1),
      "::",
      stringify!(_av_m_room)
    )
  );
  assert_eq!(
    unsafe { &(*(::std::ptr::null::<av_alist__bindgen_ty_1>())).align1 as *const _ as usize },
    0usize,
    concat!(
      "Offset of field: ",
      stringify!(av_alist__bindgen_ty_1),
      "::",
      stringify!(align1)
    )
  );
  assert_eq!(
    unsafe { &(*(::std::ptr::null::<av_alist__bindgen_ty_1>())).align2 as *const _ as usize },
    0usize,
    concat!(
      "Offset of field: ",
      stringify!(av_alist__bindgen_ty_1),
      "::",
      stringify!(align2)
    )
  );
  assert_eq!(
    unsafe { &(*(::std::ptr::null::<av_alist__bindgen_ty_1>())).align3 as *const _ as usize },
    0usize,
    concat!(
      "Offset of field: ",
      stringify!(av_alist__bindgen_ty_1),
      "::",
      stringify!(align3)
    )
  );
  assert_eq!(
    unsafe { &(*(::std::ptr::null::<av_alist__bindgen_ty_1>())).align4 as *const _ as usize },
    0usize,
    concat!(
      "Offset of field: ",
      stringify!(av_alist__bindgen_ty_1),
      "::",
      stringify!(align4)
    )
  );
}
#[repr(C)]
#[repr(align(16))]
#[derive(Copy, Clone)]
pub union av_alist__bindgen_ty_2 {
  pub _av_m_args: [__avword; 256usize],
  pub align1: ::std::os::raw::c_long,
  pub align2: f64,
  pub align3: ::std::os::raw::c_longlong,
  pub align4: u128,
}
#[test]
fn bindgen_test_layout_av_alist__bindgen_ty_2() {
  assert_eq!(
    ::std::mem::size_of::<av_alist__bindgen_ty_2>(),
    2048usize,
    concat!("Size of: ", stringify!(av_alist__bindgen_ty_2))
  );
  assert_eq!(
    ::std::mem::align_of::<av_alist__bindgen_ty_2>(),
    16usize,
    concat!("Alignment of ", stringify!(av_alist__bindgen_ty_2))
  );
  assert_eq!(
    unsafe { &(*(::std::ptr::null::<av_alist__bindgen_ty_2>()))._av_m_args as *const _ as usize },
    0usize,
    concat!(
      "Offset of field: ",
      stringify!(av_alist__bindgen_ty_2),
      "::",
      stringify!(_av_m_args)
    )
  );
  assert_eq!(
    unsafe { &(*(::std::ptr::null::<av_alist__bindgen_ty_2>())).align1 as *const _ as usize },
    0usize,
    concat!(
      "Offset of field: ",
      stringify!(av_alist__bindgen_ty_2),
      "::",
      stringify!(align1)
    )
  );
  assert_eq!(
    unsafe { &(*(::std::ptr::null::<av_alist__bindgen_ty_2>())).align2 as *const _ as usize },
    0usize,
    concat!(
      "Offset of field: ",
      stringify!(av_alist__bindgen_ty_2),
      "::",
      stringify!(align2)
    )
  );
  assert_eq!(
    unsafe { &(*(::std::ptr::null::<av_alist__bindgen_ty_2>())).align3 as *const _ as usize },
    0usize,
    concat!(
      "Offset of field: ",
      stringify!(av_alist__bindgen_ty_2),
      "::",
      stringify!(align3)
    )
  );
  assert_eq!(
    unsafe { &(*(::std::ptr::null::<av_alist__bindgen_ty_2>())).align4 as *const _ as usize },
    0usize,
    concat!(
      "Offset of field: ",
      stringify!(av_alist__bindgen_ty_2),
      "::",
      stringify!(align4)
    )
  );
}
#[test]
fn bindgen_test_layout_av_alist() {
  assert_eq!(
    ::std::mem::size_of::<av_alist>(),
    2304usize,
    concat!("Size of: ", stringify!(av_alist))
  );
  assert_eq!(
    ::std::mem::align_of::<av_alist>(),
    16usize,
    concat!("Alignment of ", stringify!(av_alist))
  );
  assert_eq!(
    unsafe { &(*(::std::ptr::null::<av_alist>()))._av_alist_head as *const _ as usize },
    0usize,
    concat!(
      "Offset of field: ",
      stringify!(av_alist),
      "::",
      stringify!(_av_alist_head)
    )
  );
  assert_eq!(
    unsafe { &(*(::std::ptr::null::<av_alist>()))._av_alist_flexarray as *const _ as usize },
    256usize,
    concat!(
      "Offset of field: ",
      stringify!(av_alist),
      "::",
      stringify!(_av_alist_flexarray)
    )
  );
}
extern "C" {
  pub fn avcall_overflown(arg1: *mut av_alist) -> ::std::os::raw::c_int;
}
extern "C" {
  pub fn avcall_start(
    arg1: *mut av_alist,
    arg2: *mut __avword,
    arg3: *mut __avword,
    arg4: ::std::option::Option<unsafe extern "C" fn() -> __avrword>,
    arg5: *mut ::std::os::raw::c_void,
    arg6: ::std::os::raw::c_int,
    arg7: ::std::os::raw::c_int,
  );
}
extern "C" {
  pub fn avcall_start_struct(
    arg1: *mut av_alist,
    arg2: *mut __avword,
    arg3: *mut __avword,
    arg4: ::std::option::Option<unsafe extern "C" fn() -> __avrword>,
    arg5: size_t,
    arg6: ::std::os::raw::c_int,
    arg7: *mut ::std::os::raw::c_void,
    arg8: ::std::os::raw::c_int,
  );
}
extern "C" {
  pub fn avcall_arg_int(arg1: *mut av_alist, arg2: ::std::os::raw::c_int) -> ::std::os::raw::c_int;
}
extern "C" {
  pub fn avcall_arg_uint(
    arg1: *mut av_alist,
    arg2: ::std::os::raw::c_uint,
  ) -> ::std::os::raw::c_int;
}
extern "C" {
  pub fn avcall_arg_long(
    arg1: *mut av_alist,
    arg2: ::std::os::raw::c_long,
  ) -> ::std::os::raw::c_int;
}
extern "C" {
  pub fn avcall_arg_ulong(
    arg1: *mut av_alist,
    arg2: ::std::os::raw::c_ulong,
  ) -> ::std::os::raw::c_int;
}
extern "C" {
  pub fn avcall_arg_ptr(
    arg1: *mut av_alist,
    arg2: *mut ::std::os::raw::c_void,
  ) -> ::std::os::raw::c_int;
}
extern "C" {
  pub fn avcall_arg_longlong(
    arg1: *mut av_alist,
    arg2: ::std::os::raw::c_longlong,
  ) -> ::std::os::raw::c_int;
}
extern "C" {
  pub fn avcall_arg_ulonglong(
    arg1: *mut av_alist,
    arg2: ::std::os::raw::c_ulonglong,
  ) -> ::std::os::raw::c_int;
}
extern "C" {
  pub fn avcall_arg_float(arg1: *mut av_alist, arg2: f32) -> ::std::os::raw::c_int;
}
extern "C" {
  pub fn avcall_arg_double(arg1: *mut av_alist, arg2: f64) -> ::std::os::raw::c_int;
}
extern "C" {
  pub fn avcall_arg_struct(
    arg1: *mut av_alist,
    arg2: size_t,
    arg3: size_t,
    arg4: *const ::std::os::raw::c_void,
  ) -> ::std::os::raw::c_int;
}
extern "C" {
  pub fn avcall_call(arg1: *mut av_alist) -> ::std::os::raw::c_int;
}
