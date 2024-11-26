use std::{borrow::Borrow, rc::Rc};

use napi::{bindgen_prelude::{Array, Reference}, Env, Error};
use crate::glue::{connection::InternalConnection, query_value::InternalDocument, session::InternalSession, table::InternalTable};

use super::{connection::Connection, find_cursor::FindCursor, session::Session, types::Document, utils::{extract_values, parse_index_specs, unwrap_or_error, IndexSpec}};

#[napi(object)]
pub struct FindOptions {
  pub key_format: Option<String>,
  pub value_format: Option<String>,
  pub session: Option<Reference<Session>>,
  pub columns: Option<String>
}

impl FindOptions {
  pub fn default() -> Self {
    return FindOptions {
      key_format: None,
      value_format: None,
      session: None,
      columns: None
    };
  }
}


#[napi]
pub struct Table {
  table: InternalTable,
  connection: Rc<InternalConnection>
}

fn get_session<'l>(env: &'l Env, session: &'l Option<Reference<Session>>, connection: &InternalConnection) -> Result<&'l InternalSession, Error> {
  if session.is_some() {
    let unwrapped = session.as_ref().unwrap();
    return Ok(unwrapped.get_internal_session());
  }
  else {
    let session = unwrap_or_error(connection.open_session(None))?;
    let mut js_object = env.create_object()?;
    env.wrap(&mut js_object, session)?;
    return Ok(env.unwrap::<InternalSession>(&js_object)?);
  };
}

#[napi]
impl Table {
  #[napi(constructor)]
  pub fn new(connection: &Connection, table_name: String, config: String) -> Self {
    return Table { connection: connection.get_connection(), table: InternalTable::new(table_name, config) };
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
        key: extract_values(document.key, key_formats, false)?,
        value: extract_values(document.value, value_formats, false)?,
      });
    }
    let session = unwrap_or_error(self.connection.open_session(None))?;
    let ret = unwrap_or_error(self.table.insert_many(session.borrow(), &mut converted_documents));
    return ret;
  }

  #[napi]
  pub fn create_index(&mut self, index_name: String, config: String) -> Result<(), Error> {
    let session = unwrap_or_error(self.connection.open_session(None))?;
    return unwrap_or_error(self.table.create_index(session.borrow(), index_name, config));
  }

  #[napi]
  pub fn delete_many(&mut self, conditions: Option<Vec<IndexSpec>>) -> Result<i32, Error> {
    let session = unwrap_or_error(self.connection.open_session(None))?;
    let unwrapped = conditions.unwrap_or(Vec::new());
    let index_uris:Vec<String> = (&unwrapped).into_iter()
      .filter(|is| { match is.index_name { Some(_) => true, None => false } })
      .map(|is| { is.index_name.as_ref().unwrap().to_string() })
      .collect();

    unwrap_or_error(self.table.ensure_index_formats(&session, &index_uris))?;
    let (table_key_format, index_formats) = unwrap_or_error(self.table.get_index_and_key_formats())?;

    let index_specs = parse_index_specs(unwrapped, index_formats, table_key_format)?;

    return unwrap_or_error(self.table.delete_many(session, index_specs));
  }

  #[napi]
  pub fn update_many(&mut self, conditions: Option<Vec<IndexSpec>>, new_value: Array) -> Result<i32, Error> {
    let session = unwrap_or_error(self.connection.open_session(None))?;
    let unwrapped = conditions.unwrap_or(Vec::new());
    let index_uris:Vec<String> = (&unwrapped).into_iter()
      .filter(|is| { match is.index_name { Some(_) => true, None => false } })
      .map(|is| { is.index_name.as_ref().unwrap().to_string() })
      .collect();

    unwrap_or_error(self.table.ensure_index_formats(&session, &index_uris))?;
    let (table_key_format, index_formats) = unwrap_or_error(self.table.get_index_and_key_formats())?;

    let index_specs = parse_index_specs(unwrapped, index_formats, table_key_format)?;

    {
      unwrap_or_error(self.table.init_table_formats())?;
    }
    let value_formats = unwrap_or_error(self.table.get_value_formats())?;

    let mut values = extract_values(new_value, value_formats, true)?;
    return unwrap_or_error(self.table.update_many(session, index_specs, &mut values));
  }

  #[napi]
  pub fn find(&mut self, env: Env, conditions: Option<Vec<IndexSpec>>, options: Option<FindOptions>) -> Result<FindCursor, Error> {
    let unwrapped_options = options.unwrap_or(FindOptions::default());
    let unwrapped = conditions.unwrap_or(Vec::new());
    let session_ref = get_session(&env, &unwrapped_options.session, &self.connection)?;


    let index_uris:Vec<String> = (&unwrapped).into_iter()
      .filter(|is| { match is.index_name { Some(_) => true, None => false } })
      .map(|is| { is.index_name.as_ref().unwrap().to_string() })
      .collect();

    unwrap_or_error(self.table.ensure_index_formats(session_ref, &index_uris))?;
    let (table_key_format, index_formats) = unwrap_or_error(self.table.get_index_and_key_formats())?;

    let index_specs = parse_index_specs(unwrapped, index_formats, table_key_format)?;

    return Ok(FindCursor::new(unwrap_or_error(self.table.find(session_ref, index_specs, unwrapped_options.columns))?, unwrapped_options.key_format, unwrapped_options.value_format)?);
  }
}
