use std::{borrow::Borrow, ffi::CStr, fmt::Display};

use crate::external::wiredtiger::wiredtiger_strerror;

#[derive(PartialEq, Debug)]
pub enum ErrorDomain {
  WIREDTIGER,
  GLUE
}
#[derive(Debug)]
pub struct GlueError {
  pub error_domain: ErrorDomain,
  pub wiredtiger_error: i32,
  pub glue_error: GlueErrorCode,
  extra: String
}


impl Display for GlueError {
  fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
    f.write_str("Glue Error (")?;
    match self.error_domain {
      ErrorDomain::GLUE => {
        f.write_str("Domain=Glue,")?;
        f.write_fmt(format_args!("Error={}", glue_error_strerror(&self.glue_error, &self.extra)))?;
      },
      ErrorDomain::WIREDTIGER => {
        f.write_str("Domain=WIREDTIGER,")?;
        f.write_fmt(format_args!("Error={}",unsafe { CStr::from_ptr(wiredtiger_strerror(self.wiredtiger_error)).to_str().unwrap() }))?;
      }
    }
    f.write_str(")")?;
    Ok(())
  }
}

impl GlueError {
  pub fn for_wiredtiger(error: i32) -> Self {
    return GlueError { error_domain: ErrorDomain::WIREDTIGER, wiredtiger_error: error, glue_error: GlueErrorCode::NoError, extra: "".to_string() };
  }
  pub fn for_glue(error: GlueErrorCode) -> Self {
    return GlueError { error_domain: ErrorDomain::GLUE, wiredtiger_error: 0, glue_error: error, extra: "".to_string() }
  }
  pub fn for_glue_with_extra(error: GlueErrorCode, extra: String) -> Self {
    return GlueError { error_domain: ErrorDomain::GLUE, wiredtiger_error: 0, glue_error: error, extra };
  }
}


fn glue_error_strerror(error: &GlueErrorCode, extra: &String) -> String {
  return match error {
    GlueErrorCode::NoError => "Should not have got here - no error".to_string(),
    GlueErrorCode::ImpossibleNoFn => "Impossible - there's no function available".to_string(),
    GlueErrorCode::UnlikelyNoPtr => "Unlikely - but there's no pointer".to_string(),
    GlueErrorCode::UnlikelyNoValue => "Unlikely - but there's no value where we tried to extract something".to_string(),
    GlueErrorCode::UnlikelyNoRun => "Unlikely - but the function never populated the return value ptr".to_string(),
    GlueErrorCode::NoString => "Failed to convert a string".to_string(),
    GlueErrorCode::InvalidIndexSpec => "Invalid index spec (no values or conditions)".to_string(),
    GlueErrorCode::InvalidFormat => "Unrecognised format: ".to_string() +extra.borrow(),
    GlueErrorCode::InvalidDataForFormat => "Unrecognised data for format: ".to_string() + extra.borrow(),
    GlueErrorCode::InvalidLength => "Invalid length: ".to_string() + extra.borrow(),
    GlueErrorCode::AccessEmptyBox => "Tried to access an empty box: ".to_string() + extra.borrow(),
    GlueErrorCode::NotInitialised => "Tried to access without initializing in: ".to_string() + extra.borrow(),
    GlueErrorCode::MissingRequiredConfig => "Missing required config: ".to_string() + extra.borrow()
  };
}

pub fn glue_strerror(error: GlueError) -> String {
  return glue_error_strerror(&error.glue_error, &error.extra);

  // return "Unknown Glue Error".to_string();
}
#[derive(Debug)]
pub enum GlueErrorCode {
  NoError = 0,
  UnlikelyNoPtr = 1,
  UnlikelyNoValue,
  ImpossibleNoFn,
  InvalidFormat,
  InvalidDataForFormat,
  InvalidLength,
  NoString,
  AccessEmptyBox,
  InvalidIndexSpec,
  NotInitialised,
  MissingRequiredConfig,


  UnlikelyNoRun = 1000
}

// pub const ERROR_UNLIKELY_NO_PTR: i32 = 1;
// pub const ERROR_IMPOSSIBLE_NO_FN: i32 = 2;
// pub const ERROR_INVALID_FORMAT: i32 = 3;
// pub const ERROR_INVALID_DATA_FOR_FORMAT: i32 = 4;
// pub const ERROR_UNLIKELY_NO_VALUE: i32 = 5;
// pub const ERROR_UNLIKELY_NO_RUN: i32 = 6;
// pub const ERROR_NO_STRING: i32 = 7;
// pub const ERROR_LENGTH_MISMATCH: i32 = 8;
