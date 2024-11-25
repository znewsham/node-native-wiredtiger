use crate::external::wiredtiger::*;


pub trait CustomCollator {
  fn get_collator(&mut self) -> *mut WT_COLLATOR;
}
