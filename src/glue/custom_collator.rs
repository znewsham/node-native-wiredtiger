use crate::external::wiredtiger::*;


pub trait CustomCollatorTrait {
  fn get_collator(&mut self) -> *mut WT_COLLATOR;
}
