use std::collections::HashSet;
use std::hash::Hash;

use super::cursor::InternalCursor;
use super::cursor_trait::InternalCursorTrait;
use super::index_spec::cursor_for_index_specs;
use super::query_value::{InternalIndexSpec, QueryValue};
use super::error::*;
use super::session::InternalSession;

struct Key {
  key: Vec<QueryValue>
}

impl Eq for Key {}
impl PartialEq for Key {
  fn eq(&self, other: &Self) -> bool {
    if self.key.len() != other.key.len() {
      return false;
    }
    for i in 0..self.key.len() {
      if self.key.get(i).unwrap() != other.key.get(i).unwrap() {
        return false;
      }
    }
    return true;
  }
}

impl Hash for Key {
  fn hash<H: std::hash::Hasher>(&self, state: &mut H) {
    for qv in self.key.iter() {
      qv.hash(state);
    }
  }
}

pub struct MultiCursor {
  initted: bool,
  multikey: bool,
  // temporary - so I can hack around with find_cursor - this shouldn't be pub
  pub cursor: InternalCursor,
  index_specs: Vec<InternalIndexSpec>,
  table_name: String,
  seen_keys: HashSet<Key>,
  columns_joined: Option<String>
}


impl InternalCursorTrait for MultiCursor {
  fn get_cursor_impl(&self) -> Result<&impl InternalCursorTrait, GlueError> {
    if !self.initted {
      return Err(GlueError::for_glue_with_extra(GlueErrorCode::NotInitialised, "unknown".to_string()))
    }
    return Ok(&self.cursor);
  }
  fn get_mut_cursor_impl(&mut self) -> Result<&mut impl InternalCursorTrait, GlueError> {
    if !self.initted {
      return Err(GlueError::for_glue_with_extra(GlueErrorCode::NotInitialised, "unknown".to_string()))
    }
    return Ok(&mut self.cursor);
  }

  fn close(&mut self) -> Result<(), GlueError> {
    self.seen_keys.clear();
    return self.get_mut_cursor_impl()?.close();
  }

  fn reset(&mut self) -> Result<(), GlueError> {
    self.seen_keys.clear();
    return self.get_mut_cursor_impl()?.reset();
  }
}

impl MultiCursor {
  pub fn new(table_name: String, index_specs: Vec<InternalIndexSpec>, columns_joined: Option<String>) -> Result<Self, GlueError> {
    let cursor = InternalCursor::empty();
    let internal_cursor = MultiCursor {
      initted: false,
      cursor,
      index_specs,
      table_name,
      columns_joined,
      seen_keys: HashSet::new(),

      // TODO: detect whether we're using an index that's multikey and a range that could match multiple
      multikey: true
    };
    Ok(internal_cursor)
  }

  pub fn init(&mut self, session: &InternalSession) -> Result<(), GlueError> {
    if self.initted {
      return Ok(());
    }
    self.initted = true;
    let table_join_uri = format!("join:table:{}{}", self.table_name.to_string(), self.columns_joined.as_ref().unwrap_or(&"".to_string()));
    let table_cursor_uri = format!("table:{}{}", self.table_name.to_string(), self.columns_joined.as_ref().unwrap_or(&"".to_string()));
    self.cursor = cursor_for_index_specs(
      session,
      table_cursor_uri.as_str(),
      table_join_uri.as_str(),
      &mut self.index_specs,
      false
    )?;

    Ok(())
  }

  pub fn next_with_multikey_check(&mut self) -> Result<bool, GlueError> {
    loop {
      let next = self.cursor.next()?;
      if !next {
        return Ok(false);
      }
      if !self.multikey {
        return Ok(true);
      }

      let mut key = Key {
        key: self.get_key()?
      };
      for k in key.key.iter_mut() {
        k.copy_to_external()?;
      }

      if self.seen_keys.contains(&key) {
        continue;
      }

      self.seen_keys.insert(key);
      return Ok(true);
    }
}
}
