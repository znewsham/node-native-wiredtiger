use std::ffi::CString;
use std::ptr::null;

use crate::glue::session::InternalSession;
use crate::external::wiredtiger::*;

use super::compound_directional_collator::InternalWiredTigerCompoundDirectionalCollator;
use super::custom_collator::CustomCollatorTrait;
use super::custom_extractor::CustomExtractorTrait;
use super::error::{GlueError, GlueErrorCode};
use super::multi_key_extractor::InternalWiredTigerMultiKeyExtractor;
use super::utils::{get_fn, unwrap_string_and_dealloc, unwrap_string_or_null};

pub struct InternalConnection {
  connection: *mut WT_CONNECTION
}
//
//expected fn pointer `unsafe extern "C" fn(*mut __wt_collator, *mut __wt_session, *const i8, *mut __wt_config_item, *mut *mut __wt_collator) -> _`
//found       fn item `unsafe extern "C" fn(*mut __wt_collator, *mut __wt_session, *mut __wt_config_item, *mut *mut __wt_collator)
static mut MULTIKEY_EXTRACTOR: WT_EXTRACTOR = WT_EXTRACTOR {
  customize: Some(InternalWiredTigerMultiKeyExtractor::static_customize),
  terminate: None,
  extract: None
};

static mut COMPOUND_DIRECTIONAL_COLLATOR: WT_COLLATOR = WT_COLLATOR {
  customize: Some(InternalWiredTigerCompoundDirectionalCollator::static_customize),
  terminate: None,
  compare: None
};

impl InternalConnection {
  fn get_connection(&self) -> Result<WT_CONNECTION, GlueError> {
    if self.connection == std::ptr::null_mut() {
      return Err(GlueError::for_glue(GlueErrorCode::UnlikelyNoPtr));
    }
    unsafe {
      Ok(*self.connection)
    }
  }

  pub fn add_collator(&self, name: String, collator: &mut dyn CustomCollatorTrait) -> Result<i32, GlueError> {
    let con = self.get_connection()?;

    let result = unsafe {
      get_fn(con.add_collator)?(
        self.connection,
        unwrap_string_and_dealloc(name).as_ptr(),
        collator.get_collator(),
        null()
      )
    };

    Ok(result)
  }

  pub fn add_extractor(&self, name: String, extractor: &mut dyn CustomExtractorTrait) -> Result<i32, GlueError> {
    let con = self.get_connection()?;

    let result = unsafe {
      get_fn(con.add_extractor)?(
        self.connection,
        unwrap_string_and_dealloc(name).as_ptr(),
        extractor.get_extractor(),
        null()
      )
    };

    Ok(result)
  }

  pub fn new(home: String, config: String) -> Result<Self, GlueError> {
    let mut conn: *mut WT_CONNECTION = std::ptr::null_mut();
    let home_c = CString::new(home).unwrap().into_raw();
    let config_c = CString::new(config).unwrap().into_raw();
    unsafe {
      let error = wiredtiger_open(
        home_c,
        std::ptr::null_mut(),
        config_c,
        &mut conn
      );
      drop(CString::from_raw(home_c));
      drop(CString::from_raw(config_c));
      if error != 0 {
        return Err(GlueError::for_wiredtiger(error))
      }
      let connection = InternalConnection { connection: conn };
      (*connection.connection).add_extractor.unwrap()(
        connection.connection,
        unwrap_string_and_dealloc("multikey".to_string()).as_ptr(),
        std::ptr::addr_of_mut!(MULTIKEY_EXTRACTOR),
        std::ptr::null()
      );
      (*connection.connection).add_collator.unwrap()(
        connection.connection,
        unwrap_string_and_dealloc("compound".to_string()).as_ptr(),
        std::ptr::addr_of_mut!(COMPOUND_DIRECTIONAL_COLLATOR),
        std::ptr::null()
      );
      return Ok(connection);
    }
  }

  pub fn close(&self, config: Option<String>) -> Result<(), GlueError> {
    let config_c = unwrap_string_or_null(config);
    let error = unsafe { get_fn(self.get_connection()?.close)?(self.connection, config_c.as_ptr()) };

    if error != 0 {
      return Err(GlueError::for_wiredtiger(error));
    }

    Ok(())
  }

  pub fn open_session(&self, config: Option<String>) -> Result<InternalSession, GlueError> {
    let mut session: *mut WT_SESSION = std::ptr::null_mut();
    let error: std::os::raw::c_int = unsafe { get_fn(self.get_connection()?.open_session)?(
      self.connection,
      std::ptr::null_mut(),
      unwrap_string_or_null(config).as_ptr(),
      &mut session
    ) };
    if error != 0 {
      return Err(GlueError::for_wiredtiger(error));
    }
    Ok(InternalSession::new(session))
  }
}
