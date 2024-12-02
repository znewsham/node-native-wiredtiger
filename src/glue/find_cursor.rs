use super::cursor_trait::InternalCursorTrait;
use super::multi_cursor::MultiCursor;
use super::query_value::{InternalDocument, InternalIndexSpec};
use super::error::*;
use super::session::InternalSession;

pub struct InternalFindCursor {
  initted: bool,
  cursor: MultiCursor
}

impl InternalCursorTrait for InternalFindCursor {
  fn get_cursor_impl(&self) -> Result<&impl InternalCursorTrait, GlueError> {
    return Ok(&self.cursor);
  }
  fn get_mut_cursor_impl(&mut self) -> Result<&mut impl InternalCursorTrait, GlueError> {
    return Ok(&mut self.cursor);
  }
}

impl<'l> InternalFindCursor {
  pub fn new(table_name: String, index_specs: Vec<InternalIndexSpec>, columns_joined: Option<String>) -> Result<Self, GlueError> {
    let cursor = MultiCursor::new(table_name, index_specs, columns_joined)?;
    let internal_cursor = InternalFindCursor { initted: false, cursor };
    Ok(internal_cursor)
  }

  pub fn init(&mut self, session: &InternalSession) -> Result<(), GlueError> {
    if self.initted {
      return Ok(());
    }
    self.initted = true;
    return self.cursor.init(session);
  }

  pub fn next_batch(&mut self, batch_size_opt: Option<i32>) -> Result<Vec<InternalDocument>, GlueError> {
    let mut docs: Vec<InternalDocument> = Vec::new();
    let mut count = 0;
    let batch_size = match batch_size_opt { Some(bs) => bs, None => 100 };
    loop {
      let next = self.cursor.next_with_multikey_check();
      if next.is_err() {
        return Err(next.unwrap_err());
      }
      if next.unwrap() == false {
        break;
      }
      // TODO: I think this is wrong? Seems we'd need to finalise the data for this to work in all situations? E.g., in case it's paged to disk
      docs.push(InternalDocument {
        key: self.get_key()?,
        value: self.get_value()?
      });
      count += 1;
      if count == batch_size {
        break;
      }
    }

    Ok(docs)
  }
}
