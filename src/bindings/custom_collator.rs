use std::ffi::CStr;

use napi::bindgen_prelude::{Array, This};
use napi::{noop_finalize, Either, Env, Error, JsFunction, JsObject, Ref};
use crate::external::wiredtiger::{wiredtiger_lex_compare, WT_COLLATOR, WT_CONFIG_ITEM, WT_ITEM, WT_SESSION};
use crate::glue::custom_collator::CustomCollatorTrait;
use crate::glue::error::GlueError;
use crate::glue::session::InternalSession;
use crate::glue::utils::char_ptr_of_length_to_string;

use super::session::Session;
use super::utils::{unwrap_or_displayable_error, unwrap_or_error};


fn unwrap_or_either_a<T>(res: Result<T, Error>) -> Result<T, Either<Error, GlueError>> {
  match res {
    Ok(t) => Ok(t),
    Err(err) => Err(Either::A(err))
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
pub enum Comparison {
  LessThan=-1,
  Equal=0,
  GreaterThan=1
}

#[napi]
pub struct CustomCollator {
  // if you change the order here - make sure the static members still work
  env: Env,
  compare_fn: Option<Ref<()>>,
  customize_fn: Option<Ref<()>>,
  terminate_fn: Option<Ref<()>>,
  collator: WT_COLLATOR,
  this: Ref<()>
}


impl CustomCollatorTrait for CustomCollator {
  fn get_collator(&mut self) -> *mut WT_COLLATOR {
    return &raw mut self.collator;
  }
}

impl Drop for CustomCollator {
  fn drop(&mut self) {
    if self.compare_fn.is_some() {
      self.compare_fn.as_mut().unwrap().unref(self.env).unwrap();
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
impl CustomCollator {
  #[napi(constructor)]
  pub fn new(
    env: Env,
    this: This<JsObject>,
    #[napi(ts_arg_type="(session: Session, key1: Buffer, key2: Buffer) => number")]
    compare: Option<JsFunction>,
    #[napi(ts_arg_type="(session: Session, uri: string, config: string) => number")]
    customize: Option<JsFunction>,
    #[napi(ts_arg_type="(session: Session) => number")]
    terminate: Option<JsFunction>
  ) -> Result<Self, Error> {
    let ret = CustomCollator {
      // if you change the order here - make sure the static members still work
      env,
      collator: WT_COLLATOR {
        compare: if compare.is_some() { Some(CustomCollator::static_compare) } else { None },
        customize: if customize.is_some() { Some(CustomCollator::static_customize) } else { None },
        terminate: if terminate.is_some() { Some(CustomCollator::static_terminate) } else { None },
      },
      compare_fn: if compare.is_some() { Some(env.create_reference(compare.unwrap())?) } else { None },
      customize_fn: if customize.is_some() { Some(env.create_reference(customize.unwrap())?) } else { None },
      terminate_fn: if terminate.is_some() { Some(env.create_reference(terminate.unwrap())?) } else { None },
      this: env.create_reference(this)?
    };

    return Ok(ret)
  }

  fn compare(
    &self,
    session: *mut WT_SESSION,
    v1: *const WT_ITEM,
    v2: *const WT_ITEM,
  ) -> Result<i32, Either<Error, GlueError>> {
    if self.compare_fn.is_some() {
      let compare_fn: JsFunction = unwrap_or_either_a(self.env.get_reference_value(&self.compare_fn.as_ref().unwrap()))?;
      let this: This<JsObject> = unwrap_or_either_a(self.env.get_reference_value(&self.this))?;
      let mut session_this = unwrap_or_either_a(Session::get_this(self.env, session))?;
      if session_this.is_none() {
        session_this = Some(unwrap_or_either_a(Session::new_internal(self.env, InternalSession::new(session)))?);
      }
      let b1_buffer = unwrap_or_either_a(unsafe { self.env.create_buffer_with_borrowed_data((*v1).data as *mut u8, (*v1).size as usize, 0, noop_finalize)})?;
      let b2_buffer = unwrap_or_either_a(unsafe { self.env.create_buffer_with_borrowed_data((*v2).data as *mut u8, (*v2).size as usize, 0, noop_finalize)})?;

      let result: Array = unwrap_or_either_a(compare_fn.apply3(
        this,
        session_this,
        b1_buffer.into_raw(),
        b2_buffer.into_raw()
      ))?;
      if result.len() != 2 {
        return Err(Either::A(Error::from_reason("Must return a tuple of [err code|0, Comparison]")));
      }

      let err: i32 = unwrap_or_either_a(result.get(0))?.unwrap();
      if err != 0 {
        return Err(Either::B(GlueError::for_wiredtiger(err)))
      }
      return Ok(unwrap_or_either_a(result.get(1))?.unwrap());
    }
    return Ok(unsafe { wiredtiger_lex_compare(v1, v2) });
  }

  unsafe extern "C" fn raw_compare(
    &self,
    session: *mut WT_SESSION,
    v1: *const WT_ITEM,
    v2: *const WT_ITEM,
    compare: *mut i32
  ) -> i32 {
    let res = self.compare(session, v1, v2);
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
      Ok(comparison) => {
        *compare = comparison;
        0
      }
    }
  }

  fn customize(
    &self,
    session: *mut WT_SESSION,
    uri: *const ::std::os::raw::c_char,
    appcfg: *mut WT_CONFIG_ITEM,
    customp: *mut *mut WT_COLLATOR
  ) -> Result<i32, Error> {
    if self.customize_fn.is_some() {
      let customize_fn: JsFunction = self.env.get_reference_value(&self.customize_fn.as_ref().unwrap())?;
      let this: This<JsObject> = self.env.get_reference_value(&self.this)?;
      let mut session_this = Session::get_this(self.env, session)?;

      if session_this.is_none() {
        session_this = Some(Session::new_internal(self.env, InternalSession::new(session))?);
      }
      let either: Either<i32, &mut CustomCollator> = customize_fn.apply3(
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
          unsafe { *customp = &raw mut customized.collator };
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
    customp: *mut *mut WT_COLLATOR
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
          session_this = Some(Session::new_internal(self.env, InternalSession::new(session))?);
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


  unsafe extern "C" fn static_compare(
    collator: *mut WT_COLLATOR,
    session: *mut WT_SESSION,
    v1: *const WT_ITEM,
    v2: *const WT_ITEM,
    compare: *mut i32
  ) -> i32 {
    let diff = (&raw const (*(collator as *mut CustomCollator)).collator as usize - collator as usize) as isize;
    let real_collator: *mut CustomCollator = unsafe { (collator as *mut i8).offset(-diff) } as  *mut CustomCollator;

    return (*real_collator).raw_compare(session, v1, v2, compare);
  }


  pub unsafe extern "C" fn static_customize(
    collator: *mut WT_COLLATOR,
    session: *mut WT_SESSION,
    uri: *const ::std::os::raw::c_char,
    appcfg: *mut WT_CONFIG_ITEM,
    customp: *mut *mut WT_COLLATOR
  ) -> i32 {
    let diff = (&raw const (*(collator as *mut CustomCollator)).collator as usize - collator as usize) as isize;
    let real_collator: *mut CustomCollator = unsafe { (collator as *mut i8).offset(-diff) } as  *mut CustomCollator;

    return (*real_collator).raw_customize(session, uri, appcfg, customp);
  }



  unsafe extern "C" fn static_terminate(
    collator: *mut WT_COLLATOR,
    session: *mut WT_SESSION
  ) -> i32 {
    let diff = (&raw const (*(collator as *mut CustomCollator)).collator as usize - collator as usize) as isize;
    let real_collator: *mut CustomCollator = unsafe { (collator as *mut i8).offset(-diff) } as  *mut CustomCollator;

    return (*real_collator).raw_terminate(session);
  }
}

