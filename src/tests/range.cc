#include "../wiredtiger/types.h"
#include "../wiredtiger/db.h"
#include "../wiredtiger/session.h"
#include "../wiredtiger/find_cursor.h"

using namespace wiredtiger;

int wrapped() {
  WiredTigerDB* db = new WiredTigerDB();
  db->open("/wrapped", "in_memory");
  WT_SESSION* insertWtSession;
  db->conn->open_session(db->conn, NULL, NULL, &insertWtSession);
  WiredTigerSession* insertSession = new WiredTigerSession(insertWtSession);

  insertSession->create("table:Hello", "key_format=17s,value_format=S,columns=(_id,indexed)");
  insertSession->create("index:Hello:indexed", "columns=(indexed)");
  WT_CURSOR* insertWtCursor;
  insertSession->session->open_cursor(insertSession->session, "table:Hello", NULL, NULL, &insertWtCursor);
  Cursor* insertCursor = new Cursor(insertWtCursor);
  QueryValueOrWT_ITEM key;
  QueryValueOrWT_ITEM value;
  key.queryValue.dataType = 's';
  value.queryValue.dataType = 'S';
  key.queryValue.value = (void*)"a";
  value.queryValue.value = (void*)"a";
  insertCursor->setKey(&key);
  insertCursor->setValue(&value);
  insertCursor->insert();
  key.queryValue.value = (void*)"c";
  value.queryValue.value = (void*)"c";
  insertCursor->setKey(&key);
  insertCursor->setValue(&value);
  insertCursor->insert();
  key.queryValue.value = (void*)"aaaa";
  value.queryValue.value = (void*)"aaaa";
  insertCursor->setKey(&key);
  insertCursor->setValue(&value);
  insertCursor->insert();
  insertCursor->close();

  WT_SESSION* readWtSession;
  db->conn->open_session(db->conn, NULL, NULL, &readWtSession);
  WiredTigerSession* readSession = new WiredTigerSession(readWtSession);
  WT_CURSOR* indexWtCursor;
  readSession->session->open_cursor(readSession->session, "index:Hello:indexed", NULL, NULL, &indexWtCursor);
  FindOptions options;
  std::vector<QueryCondition> conditions;

  conditions.push_back({
    "index:Hello:indexed",
    OPERATION_GE,
    std::vector<QueryCondition>{},
    std::vector<QueryValue>{QueryValue{'C', (void*)"a"}}
  });

  FindCursor* indexCursor = new FindCursor((char*)"Hello", &conditions, &options);
  for (int i = 0; i < 5; i++) {
    indexCursor->setKey(&index);
    int exact;
    printf("Search: %d\n", indexCursor->search());
    // printf("Exact?: %d\n", exact);
    indexCursor->getKey(&key);
    printf("GetKey: %s\n", (char*)key.queryValue.value);
    indexCursor->reset();
  }
  indexCursor->close();
  readSession->session->close(readSession->session, NULL);
  db->conn->close(db->conn,NULL);
  return 0;
}

int main() {
  return wrapped();
}
