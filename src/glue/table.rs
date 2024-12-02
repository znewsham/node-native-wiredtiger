

use std::collections::HashMap;
use std::ffi::CString;


use super::cursor_trait::InternalCursorTrait;
use super::find_cursor::InternalFindCursor;
use super::multi_cursor::MultiCursor;
use super::query_value::{Format, InternalDocument, InternalIndexSpec, QueryValue};
use super::session::InternalSession;
use super::utils::extract_formats_from_config;
use super::error::*;

pub struct InternalTable {
  table_name: String,
  config: String,
  spec_name: String,
  is_initted: bool,

  key_formats: Vec<Format>,
  value_formats: Vec<Format>,
  pub index_formats: HashMap<String, Vec<Format>>
}

impl InternalTable {
  pub fn new(table_name: String, config: String) -> Self {
    let spec_name = format!("table:{}", table_name);
    return InternalTable {
      table_name,
      config,
      spec_name,
      is_initted: false,
      key_formats: Vec::new(),
      value_formats: Vec::new(),
      index_formats: HashMap::new()
    };
  }

  pub fn init_table_formats(&mut self) -> Result<(), GlueError> {
    if self.key_formats.len() != 0 {
      return Ok(());
    }
    return extract_formats_from_config(self.config.to_string(), &mut self.key_formats, &mut self.value_formats);
  }

  fn init_table(&mut self, session: &InternalSession) -> Result<(), GlueError> {
    if self.is_initted {
      return Ok(());
    }

    self.init_table_formats()?;
    session.create(self.spec_name.to_string(), Some(self.config.to_string()))?;

    Ok(())
  }

  pub fn get_key_formats(&self) -> Result<&Vec<Format>, GlueError> {
    Ok(&self.key_formats)
  }

  pub fn get_index_formats(&self) -> Result<&HashMap<String, Vec<Format>>, GlueError> {
    Ok(&self.index_formats)
  }

  // fuck this method - it's needed because of the "can't borrow immutable and mutable"
  pub fn get_index_and_key_formats(&mut self) -> Result<(&Vec<Format>, &HashMap<String, Vec<Format>>), GlueError> {
    self.init_table_formats()?;
    Ok((&self.key_formats, self.get_index_formats()?))
  }

  pub fn get_value_formats(&self) -> Result<&Vec<Format>, GlueError> {
    Ok(&self.value_formats)
  }

  pub fn insert_many(&mut self, session: &InternalSession, documents: &mut Vec<InternalDocument>) -> Result<i32, GlueError> {
    self.init_table(session)?;
    let len = documents.len();
    let mut cursor = session.open_cursor(self.spec_name.to_string(), None)?;
    for document in documents {
      cursor.set_key(&mut document.key)?;
      cursor.set_value(&mut document.value)?;
      cursor.insert()?;
    }
    (&mut cursor).close()?;
    Ok(len as i32)
  }

  // this function ensures we've loaded all the indexes created outside of create_index (just in case)
  pub fn ensure_index_formats(&mut self, session: &InternalSession, index_uris: &Vec<String>) -> Result<(), GlueError> {
    let metadata_cursor = session.open_cursor("metadata:create".to_string(), None)?;
    let mut qvs:Vec<QueryValue> = Vec::with_capacity(1);
    qvs.push(QueryValue::empty());
    for index_uri in index_uris {
      if self.index_formats.contains_key(index_uri) {
        continue;
      }
      let qv = qvs.get_mut(0).unwrap();
      qv.data_type = 'S';
      qv.set_external_str_box(CString::new(index_uri.to_string()).unwrap())?;
      metadata_cursor.set_key(&mut qvs)?;
      metadata_cursor.search()?;
      let index_config = metadata_cursor.get_value()?.get(0).unwrap().get_str()?.to_str().unwrap().to_string();
      let mut key_formats:Vec<Format> = Vec::new();
      let mut value_formats:Vec<Format> = Vec::new();
      extract_formats_from_config(index_config, &mut key_formats, &mut value_formats)?;
      self.index_formats.insert(index_uri.to_string(), key_formats);
    }
    Ok(())
  }

  pub fn create_index(&mut self, session: &InternalSession, index_name: String, config: String) -> Result<(), GlueError> {
    self.init_table(&session)?;
    session.create(format!("index:{}:{}", self.table_name, index_name), Some(config.clone()))?;

    let mut key_formats:Vec<Format> = Vec::new();
    let mut value_formats:Vec<Format> = Vec::new();
    // TODO: This only works if you specify the key_format - otherwise we need a way to extract format from column names
    extract_formats_from_config(config, &mut key_formats, &mut value_formats)?;

    self.index_formats.insert(format!("index:{}:{}", self.table_name, index_name), key_formats);
    Ok(())
  }

  pub fn find(
    &self,
    session: &InternalSession,
    index_specs: Vec<InternalIndexSpec>,
    columns_joined: Option<String>
  ) -> Result<InternalFindCursor, GlueError> {
    let mut cursor = InternalFindCursor::new(self.table_name.clone(), index_specs, columns_joined)?;
    cursor.init(session)?;
    return Ok(cursor);
  }

  pub fn delete_many(&self, session: InternalSession, index_specs: Vec<InternalIndexSpec>) -> Result<i32, GlueError> {
    let mut cursor = MultiCursor::new(self.table_name.clone(), index_specs, Some("()".to_string()))?;
    cursor.init(&session)?;
    let delete_cursor = session.open_cursor(format!("table:{}", self.table_name), None)?;
    let mut count = 0;
    while cursor.next()? {
      let mut key = cursor.get_key()?;
      for k in key.iter_mut() {
        k.set_external_reference()?;
      }
      delete_cursor.set_key(&mut key)?;
      delete_cursor.remove()?;
      count += 1;
    }

    return Ok(count);
  }

  pub fn update_many(&mut self, session: InternalSession, index_specs: Vec<InternalIndexSpec>, value: &mut Vec<QueryValue>) -> Result<i32, GlueError> {
    self.init_table(&session)?;
    let mut cursor = MultiCursor::new(self.table_name.clone(), index_specs, None)?;
    cursor.init(&session)?;
    let mut update_cursor = session.open_cursor(format!("table:{}", self.table_name), None)?;
    let mut count = 0;
    while cursor.next()? {
      let mut key = cursor.get_key()?;
      let existing = cursor.get_value()?;
      for k in key.iter_mut() {
        k.set_external_reference()?;
      }
      update_cursor.set_key(&mut key)?;

      for i in  0..value.len() {
        let value_in = value.get_mut(i).unwrap();
        let value_existing = existing.get(i).unwrap();
        if value_in.data_type == 'x' {
          value_in.internal_value = value_existing.internal_value.clone();
          value_in.set_external_reference()?;
        }
      }
      update_cursor.set_value(value)?;
      update_cursor.update()?;
      count += 1;
    }
    update_cursor.close()?;
    cursor.close()?;
    Ok(count)
  }
}
