use std::{alloc::{alloc, Layout}, borrow::Borrow, ffi::{CStr, CString}, hash::Hash, mem::ManuallyDrop, os::raw::c_void};

use libc::strlen;

use crate::external::wiredtiger::{size_t, WT_ITEM};

use super::{error::{GlueError, GlueErrorCode}, utils::{field_is_wt_item, unwrap_string_and_dealloc}};

#[non_exhaustive]
pub struct FieldFormat;

impl FieldFormat {
  pub const PADDING: char = 'x';
  pub const CHAR_ARRAY: char = 's';
  pub const STRING: char = 'S';
  pub const UBYTE: char = 'B';
  pub const BYTE: char = 'b';
  pub const UHALF: char = 'H';
  pub const HALF: char = 'h';
  pub const UINT: char = 'I';
  pub const INT: char = 'i';
  pub const UINT2: char = 'L';
  pub const INT2: char = 'l';
  pub const LONG: char = 'q';
  pub const RECID: char = 'r';
  pub const ULONG: char = 'Q';
  pub const BITFIELD: char = 't';
  pub const BOOLEAN: char = 'T';
  pub const WT_ITEM: char = 'u';
  pub const WT_UITEM: char = 'U';
  pub const WT_ITEM_DOUBLE: char = 'd'; // I don't know what this is, it sometimes gets returned.
  pub const WT_ITEM_BIGINT: char = 'z';
}

#[repr(C)]
#[derive(Clone, PartialEq, Eq)]
pub enum ExternalValue {
  StrBox(CString),
  ByteArrayBox(Vec<u8>),

  // in some cases (e.g., extractor) we don't need to copy the value out of internal (when reading the WT_ITEM)
  ReferenceInternal(),
  UInt(u64),
  None()
}

#[repr(C)]
#[derive(Clone)]
pub enum InternalValue {
  ByteArrayBox(ManuallyDrop<Vec<u8>>),
  UInt(u64),
  None()
}

impl ExternalValue {
  pub fn copy_byte_array(bytes: &[u8]) -> Self {
    let layout = Layout::from_size_align(bytes.len(), 1).unwrap();
    let value_ptr = unsafe { alloc(layout) };
    unsafe { std::ptr::copy(bytes.as_ptr(), value_ptr, bytes.len()) };
    // TODO: this feels wrong - I don't want a Vec, I want a [u8]
    let v = unsafe { Vec::from_raw_parts(value_ptr, bytes.len(), bytes.len()) };
    ExternalValue::ByteArrayBox(v)
  }
}

#[derive(Clone)]
pub struct QueryValue {
  read_ptr: u64,
  pub data_type: char,
  wt_item: WT_ITEM,
  external_value: ExternalValue,
  pub internal_value: InternalValue
}

impl PartialEq for QueryValue {
  fn eq(&self, other: &Self) -> bool {
    if self.data_type != other.data_type {
      return false;
    }
    match &self.external_value {
      ExternalValue::None() | ExternalValue::ReferenceInternal() => return false,
      ExternalValue::ByteArrayBox(sbab) => {
        match &other.external_value {
          ExternalValue::ByteArrayBox(obab) => {
            if obab.len() != sbab.len() {
              return false;
            }
            for i in 0..obab.len() {
              if obab.get(i).unwrap() != sbab.get(i).unwrap() {
                return false;
              }
            }
            return true;
          },
          _ => return false
        }
      },
      ExternalValue::StrBox(ssb) => {
        match &other.external_value {
          ExternalValue::StrBox(osb) => {
            let o_bytes = osb.as_bytes();
            let s_bytes = ssb.as_bytes();
            if o_bytes.len() != s_bytes.len() {
              return false;
            }
            for i in 0..o_bytes.len() {
              if o_bytes.get(i).unwrap() != s_bytes.get(i).unwrap() {
                return false;
              }
            }
            return true;
          },
          _ => return false
        }
      },
      ExternalValue::UInt(sui) => {
        match &other.external_value {
          ExternalValue::UInt(oui) => return *sui == *oui,
          _ => return false
        }
      }

    }
  }
}

impl Eq for WT_ITEM {

}

impl PartialEq for WT_ITEM {
  fn eq(&self, other: &Self) -> bool {
    if self.size != other.size {
      return false;
    }
    for i in 0..self.size {
      if unsafe { self.data.offset(i as isize) != other.data.offset(i as isize) } {
        return false;
      }
    }
    return true;
  }
}



impl Hash for QueryValue {
  fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
    match &self.external_value {
      ExternalValue::None()
      | ExternalValue::ReferenceInternal() => {
        match &self.internal_value {
          InternalValue::None() => {
            state.write_i8(0);
          },
          InternalValue::ByteArrayBox(bbox) => {
            for i in 0..bbox.len() {
              state.write_u8(*bbox.get(i).unwrap());
            }
          },
          InternalValue::UInt(uint) => {
            state.write_u64(*uint);
          }
        }
      },
      ExternalValue::ByteArrayBox(bbox) => {
        for i in 0..bbox.len() {
          state.write_u8(*bbox.get(i).unwrap());
        }
      },
      ExternalValue::StrBox(strbox) => {
        let bytes = strbox.as_bytes();
        for i in 0..bytes.len() {
          state.write_u8(bytes[i]);
        }
      },
      ExternalValue::UInt(uint) => {
        state.write_u64(*uint);
      }
    }
  }
}

impl QueryValue {
  pub fn empty() -> Self {
    QueryValue {
      read_ptr: 0,
      data_type: 'x',
      wt_item: WT_ITEM::empty(),
      external_value: ExternalValue::None(),
      internal_value: InternalValue::None()
    }
  }

  pub fn for_string(string: String) -> Self {
    QueryValue {
      read_ptr: 0,
      data_type: 'S',
      wt_item: WT_ITEM::empty(),
      external_value: ExternalValue::StrBox(unwrap_string_and_dealloc(string)),
      internal_value: InternalValue::None()
    }
  }
  pub fn empty_for_passthrough() -> Self {
    QueryValue {
      read_ptr: 0,
      data_type: 'x',
      wt_item: WT_ITEM::empty(),
      external_value: ExternalValue::ReferenceInternal(),
      internal_value: InternalValue::None()
    }
  }

  #[inline(always)]
  pub fn set_external_byte_array_box(&mut self, bab: Vec<u8>) -> Result<(), GlueError> {
    self.wt_item.size = bab.len() as u64;
    self.external_value = ExternalValue::ByteArrayBox(bab);
    self.wt_item.data = self.ptr_or_value()? as *mut c_void;
    Ok(())
  }

  pub fn get_external_value_ref(&self) -> &ExternalValue {
    return &self.external_value;
  }

  pub fn get_external_value_mut(&mut self) -> &mut ExternalValue {
    return &mut self.external_value;
  }

  pub fn set_external_str_box(&mut self, sb: CString) -> Result<(), GlueError> {
    self.external_value = ExternalValue::StrBox(sb);
    Ok(())
  }

  pub fn set_external_unit(&mut self, uint: u64) -> Result<(), GlueError> {
    self.external_value = ExternalValue::UInt(uint);
    Ok(())
  }

  pub fn set_external_reference(&mut self) -> Result<(), GlueError> {
    self.external_value = ExternalValue::ReferenceInternal();
    self.wt_item.size = self.get_size()? as u64;
    self.wt_item.data = self.ptr_or_value()? as *mut c_void;
    Ok(())
  }

  // this is exclusively used to get the addr of the read_ptr
  #[inline(always)]
  pub fn get_read_ptr(&mut self, format: &Format) -> *mut u8 {
    if field_is_wt_item(format.format) || format.format == FieldFormat::PADDING {
      return std::ptr::addr_of_mut!(self.wt_item) as *mut u8;
    }
    return std::ptr::addr_of_mut!(self.read_ptr) as *mut u8;
  }

  // this is exclusively used to get the addr of the read_ptr
  #[inline(always)]
  pub fn get_read_ptr_for_read(&self, format: &Format) -> *const u8 {
    if field_is_wt_item(format.format) || format.format == FieldFormat::PADDING {
      return std::ptr::addr_of!(self.wt_item) as *const u8;
    }
    return self.read_ptr as *const u8;
  }

  #[inline(always)]
  pub fn finalize_read(&mut self, format: &Format) -> Result<(), GlueError> {
    if field_is_wt_item(format.format) {
      // we need to re-interpret it as a WT_ITEM
      self.internal_value = InternalValue::ByteArrayBox(ManuallyDrop::new(unsafe { Vec::from_raw_parts(
        self.wt_item.data as *mut u8,
        self.wt_item.size as usize,
        self.wt_item.size as usize
      ) }));
    }
    else if format.format == FieldFormat::CHAR_ARRAY {
      self.internal_value = InternalValue::ByteArrayBox(ManuallyDrop::new(unsafe { Vec::from_raw_parts(
        self.read_ptr as *mut u8,
        format.size as usize,
        format.size as usize
      ) }));
    }
    else if format.format == FieldFormat::STRING {
      let len = unsafe { strlen(self.read_ptr as *const i8)};
      self.internal_value = InternalValue::ByteArrayBox(ManuallyDrop::new(unsafe { Vec::from_raw_parts(
        self.read_ptr as *mut u8,
        len,
        len
      ) }));
    }
    else { // all number types
      self.internal_value = InternalValue::UInt(self.read_ptr as u64);
    }
    Ok(())
  }

  #[inline(always)]
  pub fn get_size(&self) -> Result<u64, GlueError> {
    match self.external_value.borrow() {
      ExternalValue::StrBox(_b) => Ok(0),
      ExternalValue::ByteArrayBox(b) => Ok(b.len() as u64),
      ExternalValue::UInt(_b) => Ok(0),
      ExternalValue::ReferenceInternal() => {
        match self.internal_value.borrow() {
          InternalValue::ByteArrayBox(b) => Ok(b.len() as u64),
          _ => Ok(0)
        }
      },
      ExternalValue::None() => Err(GlueError::for_glue_with_extra(GlueErrorCode::AccessEmptyBox, " getting size".to_string()))
    }
  }

  #[inline(always)]
  pub fn ptr_or_value_single(&self, format: &Format) -> Result<*const i8, GlueError> {
    if field_is_wt_item(format.format) {
      return Ok(&raw const self.wt_item as *const i8);
    }
    return self.ptr_or_value();
  }

  #[inline(always)]
  pub fn ptr_or_value(&self) -> Result<*const i8, GlueError> {
    match self.external_value.borrow() {
      // the use of *mut is "wrong" here
      ExternalValue::StrBox(b) => Ok(b.as_ptr()),
      ExternalValue::ByteArrayBox(b) => Ok(b.as_ptr() as *const i8),
      ExternalValue::UInt(b) => Ok(*b as *mut i8),
      ExternalValue::ReferenceInternal() => {
        match self.internal_value.borrow() {
          InternalValue::ByteArrayBox(b) => Ok(b.as_ptr() as *const i8),
          InternalValue::UInt(b) => Ok(*b as *mut i8),
          InternalValue::None() => Err(GlueError::for_glue_with_extra(GlueErrorCode::AccessEmptyBox, " getting ptr or value from internal".to_string()))
        }
      },
      ExternalValue::None() => Err(GlueError::for_glue_with_extra(GlueErrorCode::AccessEmptyBox, " getting ptr or value".to_string()))
    }
  }

  #[inline(always)]
  pub fn get_str(&self) -> Result<&CStr, GlueError> {
    match self.internal_value.borrow() {
      InternalValue::ByteArrayBox(b) => Ok(unsafe { CStr::from_ptr(b.as_ptr() as *const i8) }),
      _ => Err(GlueError::for_glue_with_extra(GlueErrorCode::AccessEmptyBox, " getting str".to_string()))
    }
  }

  #[inline(always)]
  pub fn get_byte_array(&self) -> Result<&Vec<u8>, GlueError> {
    match self.internal_value.borrow() {
      InternalValue::ByteArrayBox(b) => Ok(b),
      _ => Err(GlueError::for_glue_with_extra(GlueErrorCode::AccessEmptyBox, " getting byte array".to_string()))
    }
  }

  #[inline(always)]
  pub fn get_uint(&self) -> Result<&u64, GlueError> {
    match self.internal_value.borrow() {
      InternalValue::UInt(b) => Ok(b),
      _ => Err(GlueError::for_glue_with_extra(GlueErrorCode::AccessEmptyBox, " getting uint".to_string()))
    }
  }

  // this function concretises the values - the `InternalValue` values are only valid while the cursor is positioned on that entry
  // right now we need these concrete values for the seen_keys in multicursor
  // TODO: a decent amount of work in binding/utils can possibly be replaced with this
  pub fn copy_to_external(&mut self) -> Result<(), GlueError> {
    match &self.internal_value {
      InternalValue::None() => Err(GlueError::for_glue_with_extra(GlueErrorCode::AccessEmptyBox, " while copying".to_string())),
      InternalValue::ByteArrayBox(bab) => {
        self.external_value = ExternalValue::ByteArrayBox(bab.clone().to_vec());
        Ok(())
      },
      InternalValue::UInt(uint) => {
        self.external_value = ExternalValue::UInt(*uint);
        Ok(())
      }
    }
  }
}

pub struct Format {
  pub format: char,
  pub size: size_t
}

pub struct InternalDocument {
  pub key: Vec<QueryValue>,
  pub value: Vec<QueryValue>
}

// I hate this - but I really can't be bothered to make a perfectly clean extraction :shrug:
// without this, it's somewhat easy (implement FromNapiValue, ToNapiValue) to get it to work - but I can't figure out the typedef
#[napi]
#[derive(PartialEq)]
pub enum Operation {
  AND,
  OR,
  LT,
  GT,
  LE,
  GE,
  EQ,
  NE,
  INDEX,
}

#[derive(Clone)]
pub struct InternalIndexSpec {
  pub index_name: Option<String>,
  pub operation: Operation,
  pub sub_conditions: Option<Vec<InternalIndexSpec>>,
  pub query_values: Option<Vec<QueryValue>>
}
