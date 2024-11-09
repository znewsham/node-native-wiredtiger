#include "../wiredtiger/types.h"
#include "../wiredtiger/db.h"
#include "../wiredtiger/session.h"
#include "../wiredtiger/custom_collator.h"
#include "../wiredtiger/table.h"

using namespace wiredtiger;

int runRegular() {
  WiredTigerDB* db = new WiredTigerDB();
  db->open("/regular", "in_memory");

  WT_SESSION* session;
  db->conn->open_session(db->conn, NULL, NULL, &session);
  session->create(session, "table:Hello", "key_format=S,value_format=Si,columns=(_id,indexed,int)");
  session->create(session, "index:Hello:indexed", "columns=(indexed),collator=regular");
  session->create(session, "index:Hello:int", "columns=(int)");
  WT_CURSOR* insertCursor;
  session->open_cursor(session, "table:Hello", NULL, NULL, &insertCursor);
  insertCursor->set_key(insertCursor, "AAA");
  insertCursor->set_value(insertCursor, "test1", 1);
  insertCursor->insert(insertCursor);
  insertCursor->set_key(insertCursor, "BBB");
  insertCursor->set_value(insertCursor, "test1", 3);
  insertCursor->insert(insertCursor);
  insertCursor->set_key(insertCursor, "CCC");
  insertCursor->set_value(insertCursor, "test1", 5);
  insertCursor->insert(insertCursor);
  insertCursor->set_key(insertCursor, "DDD");
  insertCursor->set_value(insertCursor, "test1", 6);
  insertCursor->insert(insertCursor);
  insertCursor->close(insertCursor);
  session->close(session, NULL);

  db->conn->open_session(db->conn, NULL, NULL, &session);
  WT_CURSOR* deleteCursor;
  session->open_cursor(session, "table:Hello", NULL, NULL, &deleteCursor);
  // indexCursor->set_key(indexCursor, 3);
  // indexCursor->search(indexCursor);
  int myError = 0;
  deleteCursor->set_key(deleteCursor, "AAA");
  printf("AAA: %d\n", deleteCursor->remove(deleteCursor));
  deleteCursor->set_key(deleteCursor, "BBB");
  printf("BBB: %d\n", deleteCursor->remove(deleteCursor));
  deleteCursor->set_key(deleteCursor, "CCC");
  printf("CCC: %d\n", deleteCursor->remove(deleteCursor));
  deleteCursor->set_key(deleteCursor, "DDD");
  printf("DDD: %d\n", deleteCursor->remove(deleteCursor));
  deleteCursor->close(deleteCursor);
  session->close(session, NULL);
  db->conn->close(db->conn, NULL);
}

int runWrapped() {
  WiredTigerDB* db = new WiredTigerDB();
  db->open("/wrapped", "in_memory");

  WT_SESSION* wtSession;
  db->conn->open_session(db->conn, NULL, NULL, &wtSession);
  WiredTigerSession* session = new WiredTigerSession(wtSession);
  WiredTigerTable* table = new WiredTigerTable(db, "Hello", "key_format=S,value_format=Si,columns=(_id,indexed,int)");
  std::vector<std::unique_ptr<EntryOfVectors>> documents;

  char* vals[4] = { "AAA", "BBB", "CCC", "DDD" };


  for (int i = 0; i < 4; i++) {
    std::vector<QueryValueOrWT_ITEM>* key = new std::vector<QueryValueOrWT_ITEM>(1);
    (*key)[0] = { 'S', vals[i], 0, true };
    std::vector<QueryValueOrWT_ITEM>* value = new std::vector<QueryValueOrWT_ITEM>(2);
    EntryOfVectors* ev = new EntryOfVectors();
    (*value)[0] = {
      'S',
      (char*)"test2",
      0,
      true
    };
    (*value)[1] = {
        'i',
        (void*)3,
        0,
        true
    };
    ev->keyArray = key;
    ev->valueArray = value;
    documents.push_back(std::unique_ptr<EntryOfVectors>(ev));
  }
  printf("Insert: %d\n", table->insertMany(session, &documents));
  session->session->close(session->session, NULL);

  db->conn->open_session(db->conn, NULL, NULL, &wtSession);
  WT_CURSOR* wtDeleteCursor;
  session->session->open_cursor(session->session, "table:Hello", NULL, NULL, &wtDeleteCursor);
  Cursor* deleteCursor = new Cursor(wtDeleteCursor);

  int error;
  while((error = wtDeleteCursor->next(wtDeleteCursor)) == 0) {
    char* str;
    wtDeleteCursor->get_key(wtDeleteCursor, &str);
    printf("Found:%s\n", str);
  }

  std::vector<QueryValueOrWT_ITEM> key(1);
  key[0] = { 'S', (char*)"AAA", 0, true };
  deleteCursor->setKey(&key);
  printf("AAA: %d\n", deleteCursor->remove());
  deleteCursor->setKey(&key);
  printf("AAA: %d\n", deleteCursor->remove());
  key[0] = { 'S', (char*)"BBB", 0, true };
  deleteCursor->setKey(&key);
  printf("BBB: %d\n", deleteCursor->remove());
  key[0] = { 'S', (char*)"CCC", 0, true };
  deleteCursor->setKey(&key);
  printf("CCC: %d\n", deleteCursor->remove());
  key[0] = { 'S', (char*)"DDD", 0, true };
  deleteCursor->setKey(&key);
  printf("DDD: %d\n", deleteCursor->remove());

  while((error = wtDeleteCursor->next(wtDeleteCursor)) == 0) {
    char* str;
    wtDeleteCursor->get_key(wtDeleteCursor, &str);
    printf("Found:%s\n", str);
  }
  wtDeleteCursor->close(wtDeleteCursor);
  session->session->close(session->session, NULL);
  db->conn->close(db->conn, NULL);
}

int main() {
  int x = runWrapped();
  return 0;
}
