use crate::external::wiredtiger::WT_NOTFOUND;

use super::{cursor::InternalCursor, cursor_trait::InternalCursorTrait, error::{ErrorDomain, GlueError}, query_value::{InternalIndexSpec, Operation}, session::InternalSession};

fn cursor_for_index_spec(
  session: &InternalSession,
  table_cursor_uri: &str,
  join_cursor_uri: &str,
  index_spec: &InternalIndexSpec
) -> Result<InternalCursor, GlueError> {
  let index_name = index_spec.index_name.clone();
  if index_spec.operation == Operation::INDEX {
    let cursor = session.open_cursor(index_name.unwrap(), Some("".to_string()))?;
    cursor.next()?;
    return Ok(cursor);
  }
  if index_spec.operation == Operation::NE {
    let sub_conditions: Vec<InternalIndexSpec> = vec![
      InternalIndexSpec {
        index_name: index_name.clone(),
        operation: Operation::GT,
        sub_conditions: index_spec.sub_conditions.clone(),
        query_values: index_spec.query_values.clone()
      },
      InternalIndexSpec {
        index_name: index_name.clone(),
        operation: Operation::LT,
        sub_conditions: index_spec.sub_conditions.clone(),
        query_values: index_spec.query_values.clone()
      }
    ];
    return cursor_for_index_specs(
      session,
      table_cursor_uri,
      join_cursor_uri,
      &sub_conditions,
      true
    );
  }

  if index_spec.query_values.is_some() {
    let query_values = index_spec.query_values.as_ref().unwrap();
    if query_values.len() != 0 {
      let cursor = session.open_cursor(match index_spec.index_name.clone() { Some(index_name) => index_name, None => table_cursor_uri.to_string() }, Some("".to_string()))?;
      cursor.set_key(&query_values)?;

      // yuck - assignments of an if
      let exact: i32 = if index_spec.index_name.as_ref().is_some_and(|index_name| { index_name.starts_with("i")} && (index_spec.operation == Operation::EQ || index_spec.operation == Operation::INDEX)) {
        cursor.search()?;
        0
      }
      else if index_spec.operation != Operation::EQ {
        cursor.search_near()?
      }
      else { 0 };

      if ((index_spec.operation == Operation::LT) && exact == -1)
        || ((index_spec.operation == Operation::GT) && exact == 1)
      {
        // pretty sure this is wrong. I think we need a next+error check+prev (and the reverse) since the closest key might be before
        // not 100% sure. requires some thought.
        // [a, b) should match a -> a.+. a search_near of b could result in c (if it exists) with an exact of 1
        // but there could still be an a.+ - this would probably be the case if the above key was czzzzzz (or similar).
        // there is nothing "the other side of" this key, so we don't need to add this cursor
        // I can't make this case fail though - it seems that WT always picks "the one after"
        return Err(GlueError::for_wiredtiger(WT_NOTFOUND));
      }
      return Ok(cursor);
    }
  }

  if index_spec.sub_conditions.is_some() {
    let mut sub_conditions = index_spec.sub_conditions.clone().unwrap();
    if sub_conditions.len() != 0 {
      return cursor_for_index_specs(
        session,
        table_cursor_uri,
        join_cursor_uri,
        &mut sub_conditions,
        index_spec.operation == Operation::OR
      );
    }
  }

  Err(GlueError::for_glue(super::error::GlueErrorCode::InvalidIndexSpec))
}

pub fn cursor_for_index_specs(
  session: &InternalSession,
  table_cursor_uri: &str,
  join_cursor_uri: &str,
  index_specs: &Vec<InternalIndexSpec>,
  disjunction: bool
) -> Result<InternalCursor, GlueError> {
  if index_specs.len() == 0 {
    return session.open_cursor(table_cursor_uri.to_string(), None);
  }
  let mut join_cursor = session.open_cursor(join_cursor_uri.to_string(), None)?;
  let mut current_join_cursor = &mut join_cursor;
  let mut last_operation: Option<Operation> = None;
  let mut i = 0;
  while i < index_specs.len() {
    let index_spec = index_specs.get(i).unwrap();
    let cursor_or_error = cursor_for_index_spec(
      session,
      table_cursor_uri,
      join_cursor_uri,
      index_spec
    );
    if cursor_or_error.as_ref().is_err_and(|err| { err.error_domain == ErrorDomain::WIREDTIGER && err.wiredtiger_error == WT_NOTFOUND}) {
      if !disjunction {
        i += 1;
        continue;
      }
    }
    else if cursor_or_error.is_err() {
      return Err(cursor_or_error.err().unwrap());
    }
    let cursor = cursor_or_error?;
    // if (error == WT_NOTFOUND) {
    //   if (!disjunction) {
    //     // bit hacky - but we only return WT_NOTFOUND when the cursor wouldn't restrict things, e.g., LT <key where nothing GT that key exists>
    //     // in the case of AND - this cursor wouldn't reduce the scope further (we need to look at every key) and adding it will exclude the last valid item
    //     continue;
    //   }
    // }
    let mut config = match index_spec.operation {
      Operation::EQ
      | Operation::NE
      | Operation::AND
      | Operation::OR => "compare=eq",
      Operation::LE => "compare=le",
      Operation::LT => "compare=lt",
      Operation::GE | Operation::INDEX => "compare=ge",
      Operation::GT => "compare=gt"
    }.to_string();


    if disjunction && (last_operation.is_none() || last_operation.unwrap() != index_spec.operation) {
      // for the conditions "LT or GT" (e.g., NE) we need to push into a join eg "'join LT' or 'join GT'"
      // but for EQ we want to keep everything together. (nested joins leads to duplicates)
      // "LT and GT" is supported by WT

      last_operation = None;

      let new_join_cursor = session.open_cursor(join_cursor_uri.to_string(), Some("".to_string()))?;
      session.join(current_join_cursor, &new_join_cursor, if disjunction { Some("operation=or".to_string()) } else { None })?;
      current_join_cursor = current_join_cursor.own_joined_cursor(new_join_cursor);
      continue;
    }
    last_operation = Some(index_spec.operation);
    if disjunction {
      config = format!("{},operation=or", config);
    }
    session.join(&current_join_cursor, &cursor, Some(config))?;
    current_join_cursor.own_joined_cursor(cursor);
    i+=1;
  }

  Ok(join_cursor)
}
