use napi::bindgen_prelude::Array;

#[napi(object)]
pub struct Document {
  pub key: Array,
  pub value: Array
}
