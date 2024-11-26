use crate::external::wiredtiger::*;


pub trait CustomExtractorTrait {
  fn get_extractor(&mut self) -> *mut WT_EXTRACTOR;
}
