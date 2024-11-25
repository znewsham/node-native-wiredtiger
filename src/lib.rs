mod bindings {
  pub mod connection;
  pub mod session;
  pub mod cursor;
  pub mod table;
  pub mod find_cursor;
  pub mod custom_collator;
  mod utils;
}

mod glue {
  pub mod connection;
  pub mod session;
  pub mod error;
  pub mod cursor;
  pub mod query_value;
  pub mod find_cursor;
  pub mod table;
  pub mod custom_collator;
  mod multi_key_extractor;
  mod compound_directional_collator;
  mod index_spec;
  mod wt_item;

  // shared with bindings
  pub mod utils;
  mod avcall;
}

mod external {
  mod lint;
  pub mod wiredtiger;
  pub mod avcall;
  pub mod avcall_helpers;
}

#[macro_use]
extern crate napi_derive;
