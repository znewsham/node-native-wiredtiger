use std::{borrow::Borrow, rc::Rc};

use napi::{bindgen_prelude::Array, Error};
use crate::glue::{connection::InternalWiredTigerConnection, query_value::InternalDocument, table::InternalWiredTigerTable};

use super::{connection::WiredTigerConnection, find_cursor::WiredTigerFindCursor, utils::{extract_values, parse_index_specs, unwrap_or_error, IndexSpec}};


#[napi(object)]
pub struct Document {
  pub key: Array,
  pub value: Array
}


#[napi(js_name = "WiredTigerTable")]
pub struct WiredTigerTable {
  table: InternalWiredTigerTable,
  connection: Rc<InternalWiredTigerConnection>
}

#[napi]
impl WiredTigerTable {
  #[napi(constructor)]
  pub fn new(connection: &WiredTigerConnection, table_name: String, config: String) -> Self {
    return WiredTigerTable { connection: connection.get_connection(), table: InternalWiredTigerTable::new(table_name, config) };
  }

  #[napi]
  pub fn insert_many(&mut self, documents: Vec<Document>) -> Result<i32, Error> {
    let mut converted_documents: Vec<InternalDocument> = Vec::with_capacity(documents.len());
    {
      unwrap_or_error(self.table.init_table_formats())?;
    }
    let key_formats = unwrap_or_error(self.table.get_key_formats())?;
    let value_formats = unwrap_or_error(self.table.get_value_formats())?;
    for document in documents {
      converted_documents.push(InternalDocument {
        key: extract_values(document.key, key_formats)?,
        value: extract_values(document.value, value_formats)?,
      });
    }
    let session = unwrap_or_error(self.connection.open_session(None))?;
    unwrap_or_error(self.table.insert_many(session.borrow(), &mut converted_documents))
  }

  #[napi]
  pub fn create_index(&mut self, index_name: String, config: String) -> Result<(), Error> {
    let session = unwrap_or_error(self.connection.open_session(None))?;
    return unwrap_or_error(self.table.create_index(session.borrow(), index_name, config));
  }

  #[napi]
  pub fn find(&mut self, conditions: Vec<IndexSpec>) -> Result<WiredTigerFindCursor, Error> {
    let session = unwrap_or_error(self.connection.open_session(None))?;
    let index_uris:Vec<String> = (&conditions).into_iter()
      .filter(|is| { match is.index_name { Some(_) => true, None => false } })
      .map(|is| { is.index_name.as_ref().unwrap().to_string() })
      .collect();

    unwrap_or_error(self.table.ensure_index_formats(&session, &index_uris))?;
    let (table_key_format, index_formats) = unwrap_or_error(self.table.get_index_and_key_formats())?;

    let index_specs = parse_index_specs(conditions, index_formats, table_key_format)?;
    Ok(WiredTigerFindCursor::new(unwrap_or_error(self.table.find(session, index_specs))?))
  }
}
