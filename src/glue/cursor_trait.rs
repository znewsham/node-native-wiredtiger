use std::ptr::null;

use crate::external::wiredtiger::WT_CURSOR;

use super::{error::GlueError, query_value::{Format, QueryValue}};

// A cursor can be broken down into a few parts
// 1. Accessible - can you read the static properties on it (e.g., get_key_format)
// 2. Readable - can you read a value from the cursor  (e.g., get_value)
// 3. Iterable - can you call next, prev
// 4. Searchable - can you call search, search_near
// 5. Comparable - can you call compare, equalis
// 6. Writable - can you call set_key, set_value, insert, update, remove

pub trait InternalCursorTrait {
  fn get_cursor_impl(&self) -> Result<&impl InternalCursorTrait, GlueError> {
    if (&raw const self) != null() {
      panic!("Something went wrong - a ReadCursor must either implement get_cursor_impl or all other functions");
    }
    return Err::<&FakeCursor, GlueError>(GlueError::for_glue(crate::glue::error::GlueErrorCode::ImpossibleNoFn));
  }
  fn get_mut_cursor_impl(&mut self) -> Result<&mut impl InternalCursorTrait, GlueError> {
    if (&raw const self) != null() {
      panic!("Something went wrong - a ReadCursor must either implement get_cursor_impl or all other functions");
    }
    return Err::<&mut FakeCursor, GlueError>(GlueError::for_glue(crate::glue::error::GlueErrorCode::ImpossibleNoFn));
  }

  fn get_key_formats(&self) -> &Vec<Format> {
    return self.get_cursor_impl().unwrap().get_key_formats();
  }
  fn get_value_formats(&self) -> &Vec<Format> {
    return self.get_cursor_impl().unwrap().get_value_formats();
  }

  fn next(&self) -> Result<bool, GlueError> {
    return self.get_cursor_impl()?.next();
  }
  fn prev(&self) -> Result<bool, GlueError> {
    return self.get_cursor_impl()?.prev();
  }
  fn reset(&mut self) -> Result<(), GlueError> {
    return self.get_mut_cursor_impl()?.reset();
  }
  fn close(&mut self) -> Result<(), GlueError> {
    return self.get_mut_cursor_impl()?.close();
  }
  fn get_key_format(&self) -> Result<String, GlueError> {
    return self.get_cursor_impl()?.get_key_format();
  }
  fn get_value_format(&self) -> Result<String, GlueError> {
    return self.get_cursor_impl()?.get_value_format();
  }
  fn get_key(&self) -> Result<Vec<QueryValue>, GlueError> {
    return self.get_cursor_impl()?.get_key();
  }
  fn get_value(&self) -> Result<Vec<QueryValue>, GlueError> {
    return self.get_cursor_impl()?.get_value();
  }

  fn get_raw_cursor_ptr(&self) -> *mut WT_CURSOR {
    return self.get_cursor_impl().unwrap().get_raw_cursor_ptr();
  }
  fn compare(&self, cursor: &impl InternalCursorTrait) -> Result<i32, GlueError> {
    return self.get_cursor_impl()?.compare(cursor);
  }
  fn equals(&self, cursor: &impl InternalCursorTrait) -> Result<bool, GlueError> {
    return self.get_cursor_impl()?.equals(cursor);
  }
}


struct FakeCursor {}

impl InternalCursorTrait for FakeCursor {

}
