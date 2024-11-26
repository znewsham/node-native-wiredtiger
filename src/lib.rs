mod bindings {
  pub mod connection;
  pub mod session;
  pub mod cursor;
  pub mod table;
  pub mod find_cursor;
  pub mod custom_collator;
  pub mod custom_extractor;
  pub mod result_cursor;
  pub mod wt_item;
  mod cursor_trait;
  pub mod types;
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
  pub mod custom_extractor;
  mod multi_key_extractor;
  pub mod cursor_trait;
  mod multi_cursor;
  mod compound_directional_collator;
  mod index_spec;
  pub mod wt_item;

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
