
use crate::external::wiredtiger::*;

use super::cursor::InternalCursor;
use super::utils::{get_fn, unwrap_string_and_dealloc, unwrap_string_or_null};
use super::error::*;

pub struct InternalSession {
  session: *mut WT_SESSION
}

impl InternalSession {
  pub fn new(session: *mut WT_SESSION) -> Self {
    InternalSession { session }
  }

  pub fn get_session_ptr(&self) -> *mut WT_SESSION {
    return self.session;
  }

  fn get_session(&self) -> Result<WT_SESSION, GlueError> {
    if self.session == std::ptr::null_mut() {
      return Err(GlueError::for_glue(GlueErrorCode::UnlikelyNoPtr));
    }
    unsafe {
      Ok(*self.session)
    }
  }

  pub fn create(&self, uri: String, config: Option<String>) -> Result<(), GlueError> {
    let result = unsafe {
      get_fn(self.get_session()?.create)?(
        self.session,
        unwrap_string_and_dealloc(uri).as_ptr(),
        unwrap_string_or_null(config).as_ptr()
      )
    };

    if result == 0 {
      return Ok(());
    }
    Err(GlueError::for_wiredtiger( result))
  }

  pub fn close(&self, config: Option<String>) -> Result<(), GlueError> {
    let result = unsafe {
      get_fn(self.get_session()?.close)?(
        self.session,
        unwrap_string_or_null(config).as_ptr()
      )
    };

    if result == 0 {
      return Ok(());
    }
    Err(GlueError::for_wiredtiger( result))
  }

  pub fn open_cursor(&self, cursor_spec: String, config: Option<String>) -> Result<InternalCursor, GlueError> {
    let mut cursor: *mut WT_CURSOR = std::ptr::null_mut();
    let error: std::os::raw::c_int = unsafe { get_fn(self.get_session()?.open_cursor)?(
      self.session,
      unwrap_string_and_dealloc(cursor_spec).as_ptr(),
      std::ptr::null_mut(),
      unwrap_string_or_null(config).as_ptr(),
      &mut cursor
    ) };
    if error != 0 {
      return Err(GlueError::for_wiredtiger(error));
    }
    InternalCursor::new(cursor)
  }

  pub fn begin_transaction(&self, config: Option<String>) -> Result<(), GlueError> {
    let result = unsafe {
      get_fn(self.get_session()?.begin_transaction)?(
        self.session,
        unwrap_string_or_null(config).as_ptr()
      )
    };

    if result == 0 {
      return Ok(());
    }
    Err(GlueError::for_wiredtiger( result))
  }

  pub fn commit_transaction(&self, config: Option<String>) -> Result<(), GlueError> {
    let result = unsafe {
      get_fn(self.get_session()?.commit_transaction)?(
        self.session,
        unwrap_string_or_null(config).as_ptr()
      )
    };

    if result == 0 {
      return Ok(());
    }
    Err(GlueError::for_wiredtiger( result))
  }

  pub fn rollback_transaction(&self, config: Option<String>) -> Result<(), GlueError> {
    let result = unsafe {
      get_fn(self.get_session()?.rollback_transaction)?(
        self.session,
        unwrap_string_or_null(config).as_ptr()
      )
    };

    if result == 0 {
      return Ok(());
    }
    Err(GlueError::for_wiredtiger( result))
  }

  pub fn prepare_transaction(&self, config: Option<String>) -> Result<(), GlueError> {
    let result = unsafe {
      get_fn(self.get_session()?.prepare_transaction)?(
        self.session,
        unwrap_string_or_null(config).as_ptr()
      )
    };

    if result == 0 {
      return Ok(());
    }
    Err(GlueError::for_wiredtiger( result))
  }

  pub fn join(&self, join_cursor: &InternalCursor, ref_cursor: &InternalCursor, config: Option<String>) -> Result<(), GlueError> {
    let result = unsafe {
      get_fn(self.get_session()?.join)?(
        self.session,
        join_cursor.cursor,
        ref_cursor.cursor,
        unwrap_string_or_null(config).as_ptr()
      )
    };

    if result == 0 {
      return Ok(());
    }
    Err(GlueError::for_wiredtiger( result))
  }
}
