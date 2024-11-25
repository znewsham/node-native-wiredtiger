use super::cursor::InternalWiredTigerCursor;
use super::index_spec::cursor_for_index_specs;
use super::query_value::{Format, InternalIndexSpec, QueryValue};
use super::error::*;
use super::session::InternalWiredTigerSession;

pub struct InternalWiredTigerFindCursor {
  initted: bool,
  cursor: InternalWiredTigerCursor,
  index_specs: Vec<InternalIndexSpec>,
  session: InternalWiredTigerSession,
  table_name: String,
}

impl InternalWiredTigerFindCursor {
  pub fn new(session: InternalWiredTigerSession, table_name: String, index_specs: Vec<InternalIndexSpec>) -> Result<Self, GlueError> {
    let cursor = InternalWiredTigerCursor::empty();
    let internal_cursor = InternalWiredTigerFindCursor { initted: false, cursor, index_specs, session, table_name };
    Ok(internal_cursor)
  }

  pub fn get_key(&self) -> Result<Vec<QueryValue>, GlueError> {
    return self.cursor.get_key();
  }

  pub fn get_value(&self) -> Result<Vec<QueryValue>, GlueError> {
    return self.cursor.get_value();
  }

  pub fn init(&mut self) -> Result<(), GlueError> {
    if self.initted {
      return Ok(());
    }
    self.initted = true;
    let table_join_uri = format!("join:table:{}", self.table_name.to_string());
    let table_cursor_uri = format!("table:{}", self.table_name.to_string());
    self.cursor = cursor_for_index_specs(
      &self.session,
      table_cursor_uri.as_str(),
      table_join_uri.as_str(),
      &mut self.index_specs,
      false
    )?;

    Ok(())
  }

  pub fn next(&self) -> Result<bool, GlueError> {
    if !self.initted {
      return Err(GlueError::for_glue_with_extra(GlueErrorCode::NotInitialised, "next".to_string()))
    }
    return self.cursor.next();
  }

  pub fn prev(&self) -> Result<bool, GlueError> {
    if !self.initted {
      return Err(GlueError::for_glue_with_extra(GlueErrorCode::NotInitialised, "next".to_string()))
    }
    return self.cursor.prev();
  }

  pub fn reset(&self) -> Result<(), GlueError> {
    if !self.initted {
      return Err(GlueError::for_glue_with_extra(GlueErrorCode::NotInitialised, "next".to_string()))
    }
    return self.cursor.reset();
  }

  pub fn close(&self) -> Result<(), GlueError> {
    if !self.initted {
      return Err(GlueError::for_glue_with_extra(GlueErrorCode::NotInitialised, "next".to_string()))
    }
    return self.cursor.close();
  }

  pub fn compare(&self, cursor: &InternalWiredTigerFindCursor) -> Result<i32, GlueError> {
    if !self.initted {
      return Err(GlueError::for_glue_with_extra(GlueErrorCode::NotInitialised, "next".to_string()))
    }
    return self.cursor.compare(&cursor.cursor);
  }

  pub fn equals(&self, cursor: &InternalWiredTigerFindCursor) -> Result<bool, GlueError> {
    if !self.initted {
      return Err(GlueError::for_glue_with_extra(GlueErrorCode::NotInitialised, "next".to_string()))
    }
    return self.cursor.equals(&cursor.cursor);
  }

  pub fn get_key_format(&self) -> Result<String, GlueError> {
    if !self.initted {
      return Err(GlueError::for_glue_with_extra(GlueErrorCode::NotInitialised, "next".to_string()))
    }
    return self.cursor.get_key_format();
  }

  pub fn get_value_format(&self) -> Result<String, GlueError> {
    if !self.initted {
      return Err(GlueError::for_glue_with_extra(GlueErrorCode::NotInitialised, "next".to_string()))
    }
    return self.cursor.get_value_format();
  }

  pub fn get_key_formats(&self) -> &Vec<Format> {
    return self.cursor.get_key_formats();
  }

  pub fn get_value_formats(&self) -> &Vec<Format> {
    return self.cursor.get_value_formats();
  }
}
