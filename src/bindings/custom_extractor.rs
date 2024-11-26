use std::ffi::CStr;

use napi::bindgen_prelude::This;
use napi::{noop_finalize, Either, Env, Error, JsFunction, JsObject, Ref};
use crate::external::wiredtiger::{WT_CONFIG_ITEM, WT_CURSOR, WT_EXTRACTOR, WT_ITEM, WT_SESSION};
use crate::glue::cursor::InternalCursor;
use crate::glue::custom_extractor::CustomExtractorTrait;
use crate::glue::error::{GlueError, GlueErrorCode};
use crate::glue::session::InternalSession;
use crate::glue::utils::char_ptr_of_length_to_string;

use super::result_cursor::ResultCursor;
use super::session::Session;
use super::utils::{unwrap_or_displayable_error, unwrap_or_error};


fn unwrap_or_either_a<T>(res: Result<T, Error>) -> Result<T, Either<Error, GlueError>> {
  match res {
    Ok(t) => Ok(t),
    Err(err) => Err(Either::A(err))
  }
}
fn unwrap_or_either_b<T>(res: Result<T, GlueError>) -> Result<T, Either<Error, GlueError>> {
  match res {
    Ok(t) => Ok(t),
    Err(err) => Err(Either::B(err))
  }
}

// #[napi]
// pub fn wrap_in_obj(env: Env, js_fn: JsFunction) -> Result<JsObject> {
//   let mut js_obj = env.create_object()?;
//   // create a reference for the javascript function
//   let js_fn_ref = env.create_reference(js_fn)?;
//   let ctx = CallbackContext {
//     callback: js_fn_ref,
//   };
//   // wrap it in an object
//   env.wrap(&mut js_obj, ctx)?;
//   Ok(js_obj)
// }

#[napi]
pub struct CustomExtractor {
  // if you change the order here - make sure the static members still work
  env: Env,
  extract_fn: Option<Ref<()>>,
  customize_fn: Option<Ref<()>>,
  terminate_fn: Option<Ref<()>>,
  extractor: WT_EXTRACTOR,
  this: Ref<()>
}


impl CustomExtractorTrait for CustomExtractor {
  fn get_extractor(&mut self) -> *mut WT_EXTRACTOR {
    return &raw mut self.extractor;
  }
}

impl Drop for CustomExtractor {
  fn drop(&mut self) {
    if self.extract_fn.is_some() {
      self.extract_fn.as_mut().unwrap().unref(self.env).unwrap();
    }
    if self.customize_fn.is_some() {
      self.customize_fn.as_mut().unwrap().unref(self.env).unwrap();
    }
    if self.terminate_fn.is_some() {
      self.terminate_fn.as_mut().unwrap().unref(self.env).unwrap();
    }
    self.this.unref(self.env).unwrap();
  }
}

#[napi]
impl CustomExtractor {
  #[napi(constructor)]
  pub fn new(
    env: Env,
    this: This<JsObject>,
    #[napi(ts_arg_type="(session: Session, key: Buffer, value: Buffer, resultCursor: ResultCursor) => number")]
    extract: Option<JsFunction>,
    #[napi(ts_arg_type="(session: Session, uri: string, config: string) => number")]
    customize: Option<JsFunction>,
    #[napi(ts_arg_type="(session: Session) => number")]
    terminate: Option<JsFunction>
  ) -> Result<Self, Error> {
    let ret = CustomExtractor {
      // if you change the order here - make sure the static members still work
      env,
      extractor: WT_EXTRACTOR {
        extract: if extract.is_some() { Some(CustomExtractor::static_extract) } else { None },
        customize: if customize.is_some() { Some(CustomExtractor::static_customize) } else { None },
        terminate: Some(CustomExtractor::static_terminate),
      },
      extract_fn: if extract.is_some() { Some(env.create_reference(extract.unwrap())?) } else { None },
      customize_fn: if customize.is_some() { Some(env.create_reference(customize.unwrap())?) } else { None },
      terminate_fn: if terminate.is_some() { Some(env.create_reference(terminate.unwrap())?) } else { None },
      this: env.create_reference(this)?
    };

    return Ok(ret)
  }

  fn extract(
    &self,
    session: *mut WT_SESSION,
    key: *const WT_ITEM,
    value: *const WT_ITEM,
    result_cursor_ptr: *mut WT_CURSOR
  ) -> Result<(), Either<Error, GlueError>> {
    if self.extract_fn.is_some() {
      let mut cursor_this: Option<JsObject> = unwrap_or_either_a(ResultCursor::get_this(self.env, result_cursor_ptr))?;
      if cursor_this.is_none() {
        cursor_this = Some(unwrap_or_either_a(ResultCursor::new_internal(self.env, unwrap_or_either_b(InternalCursor::new(result_cursor_ptr))?))?);
      }
      let extract_fn: JsFunction = unwrap_or_either_a(self.env.get_reference_value(&self.extract_fn.as_ref().unwrap()))?;
      let this: This<JsObject> = unwrap_or_either_a(self.env.get_reference_value(&self.this))?;
      let mut session_this = unwrap_or_either_a(Session::get_this(self.env, session))?;

      if session_this.is_none() {
        session_this = Some(unwrap_or_either_a(Session::new_internal(self.env, InternalSession::new(session)))?);
      }
      // let cursor_this = WiredTigerCursor::get_this(self.env, session);
      let key_buffer = unwrap_or_either_a(unsafe { self.env.create_buffer_with_borrowed_data((*key).data as *mut u8, (*key).size as usize, 0, noop_finalize)})?;
      let value_buffer = unwrap_or_either_a(unsafe { self.env.create_buffer_with_borrowed_data((*value).data as *mut u8, (*value).size as usize, 0, noop_finalize)})?;

      let result: Option<i32> = unwrap_or_either_a(extract_fn.apply4(
        this,
        session_this,
        key_buffer.into_raw(),
        value_buffer.into_raw(),
        cursor_this
      ))?;
      if result.is_some() {
        let err = result.unwrap();
        if err != 0 {
          return Err(Either::B(GlueError::for_wiredtiger(err)))
        }
      }
      return Ok(());
    }
    return Err(Either::B(GlueError::for_glue_with_extra(GlueErrorCode::MissingRequiredConfig, "Missing extract function".to_string())));
  }

  unsafe extern "C" fn raw_extract(
    &self,
    session: *mut WT_SESSION,
    key: *const WT_ITEM,
    value: *const WT_ITEM,
    result_cursor: *mut WT_CURSOR
  ) -> i32 {
    let res = self.extract(session, key, value, result_cursor);
    match res {
      Err(err) => {
        match err {
          Either::A(err) => {
            println!("Error: {}", err);
            -1
          },
          Either::B(err) => err.wiredtiger_error
        }
      },
      Ok(_comparison) => {
        0
      }
    }
  }

  fn customize(
    &self,
    session: *mut WT_SESSION,
    uri: *const ::std::os::raw::c_char,
    appcfg: *mut WT_CONFIG_ITEM,
    customp: *mut *mut WT_EXTRACTOR
  ) -> Result<i32, Error> {
    if self.customize_fn.is_some() {
      let customize_fn: JsFunction = self.env.get_reference_value(&self.customize_fn.as_ref().unwrap())?;
      let this: This<JsObject> = self.env.get_reference_value(&self.this)?;
      let mut session_this = Session::get_this(self.env, session)?;

      if session_this.is_none() {
        let temp_this = Session::new_internal(self.env, InternalSession::new(session))?;
        session_this = Some(temp_this);
      }
      let either: Either<i32, &mut CustomExtractor> = customize_fn.apply3(
        this,
        session_this,
        unwrap_or_displayable_error(unsafe {CStr::from_ptr(uri) }.to_str())?.to_string(),
        unwrap_or_error(char_ptr_of_length_to_string(unsafe {(*appcfg).str_}, unsafe {(*appcfg).len}))?
      )?;
      match either {
        Either::A(err) => {
          return Ok(err);
        },
        Either::B(customized) => {
          unsafe { *customp = &raw mut customized.extractor };
          return Ok(0);
        }
      }
      // let ret: i32 = customize_fn.apply0(*self).unwrap();
    }
    return Ok(0);
  }


  unsafe extern "C" fn raw_customize(
    &self,
    session: *mut WT_SESSION,
    uri: *const ::std::os::raw::c_char,
    appcfg: *mut WT_CONFIG_ITEM,
    customp: *mut *mut WT_EXTRACTOR
  ) -> i32 {
    let res = self.customize(session, uri, appcfg, customp);
    match res {
      Err(err) => {
        println!("Error: {}", err);
        -1
      },
      Ok(err) => {
        err
      }
    }
  }

  fn terminate(&self, session: *mut WT_SESSION) -> Result<i32, Error> {
    match &self.terminate_fn {
      Some(terminate_fn_ref) => {
        let terminate_fn: JsFunction = self.env.get_reference_value(terminate_fn_ref)?;
        let this: This<JsObject> = self.env.get_reference_value(&self.this)?;
        let mut session_this = Session::get_this(self.env, session)?;
        if session_this.is_none() {
          let temp_this = Session::new_internal(self.env, InternalSession::new(session))?;
          session_this = Some(temp_this);
        }
        let res: Result<i32, Error> = terminate_fn.apply1(
          this,
          session_this
        );
        return res;
      },
      None => Ok(0)
    }
  }

  unsafe extern "C" fn raw_terminate(
    &self,
    session: *mut WT_SESSION
  ) -> i32 {
    let res = self.terminate(session);
    match res {
      Err(err) => {
        println!("Error: {}", err);
        -1
      },
      Ok(err) => {
        err
      }
    }
  }


  unsafe extern "C" fn static_extract(
    extractor: *mut WT_EXTRACTOR,
    session: *mut WT_SESSION,
    key: *const WT_ITEM,
    value: *const WT_ITEM,
    result_cursor: *mut WT_CURSOR
  ) -> i32 {
    let diff = (&raw const (*(extractor as *mut CustomExtractor)).extractor as usize - extractor as usize) as isize;
    let real_extractor: *mut CustomExtractor = unsafe { (extractor as *mut i8).offset(-diff) } as  *mut CustomExtractor;

    return (*real_extractor).raw_extract(session, key, value, result_cursor);
  }


  pub unsafe extern "C" fn static_customize(
    extractor: *mut WT_EXTRACTOR,
    session: *mut WT_SESSION,
    uri: *const ::std::os::raw::c_char,
    appcfg: *mut WT_CONFIG_ITEM,
    customp: *mut *mut WT_EXTRACTOR
  ) -> i32 {
    let diff = (&raw const (*(extractor as *mut CustomExtractor)).extractor as usize - extractor as usize) as isize;
    let real_extractor: *mut CustomExtractor = unsafe { (extractor as *mut i8).offset(-diff) } as  *mut CustomExtractor;

    return (*real_extractor).raw_customize(session, uri, appcfg, customp);
  }



  unsafe extern "C" fn static_terminate(
    extractor: *mut WT_EXTRACTOR,
    session: *mut WT_SESSION
  ) -> i32 {
    let diff = (&raw const (*(extractor as *mut CustomExtractor)).extractor as usize - extractor as usize) as isize;
    let real_extractor: *mut CustomExtractor = unsafe { (extractor as *mut i8).offset(-diff) } as  *mut CustomExtractor;

    return (*real_extractor).raw_terminate(session);
  }
}

