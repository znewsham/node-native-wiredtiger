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
  WT_CURSOR* indexCursor;
  session->open_cursor(session, "index:Hello:int", NULL, NULL, &indexCursor);
  // indexCursor->set_key(indexCursor, 3);
  // indexCursor->search(indexCursor);
  int myError = 0;
  while ((myError = indexCursor->next(indexCursor)) == 0) {
    int32_t key;
    myError = indexCursor->get_key(indexCursor, &key);
    printf("Found: %d %d\n", key, myError);
  }
  indexCursor->close(indexCursor);
  session->close(session, NULL);
  db->conn->close(db->conn, NULL);
}

int runWrapped() {
  WiredTigerDB* db = new WiredTigerDB();
  db->open("/wrapped", "in_memory");

  WT_SESSION* wtSession;
  db->conn->open_session(db->conn, NULL, NULL, &wtSession);
  WiredTigerSession* session = new WiredTigerSession(wtSession);
  WT_CURSOR* wtInsertCursor;
  session->session->open_cursor(session->session, "table:Hello", NULL, NULL, &wtInsertCursor);
  Cursor* insertCursor = new Cursor(wtInsertCursor);
  QueryValueOrWT_ITEM* key = (QueryValueOrWT_ITEM*)calloc(sizeof(QueryValueOrWT_ITEM), 1);
  QueryValueOrWT_ITEM* value = (QueryValueOrWT_ITEM*)calloc(sizeof(QueryValueOrWT_ITEM), 2);
  WiredTigerTable* table = new WiredTigerTable(db, "Hello", "key_format=S,value_format=Si,columns=(_id,indexed,int)");
  table->createIndex(session, "indexed", "columns=(indexed),collator=regular");
  table->createIndex(session, "int", "columns=(int)");
  std::vector<KVPair> documents;
  key[0].queryValue.dataType = 'S';
  value[0].queryValue.dataType = 'S';
  value[1].queryValue.dataType = 'i';
  key[0].queryValue.noFree = true;
  value[0].queryValue.noFree = true;
  value[1].queryValue.noFree = true;

  int vals[4] = { 1, 3, 5, 6 };

  documents.push_back(KVPair {
    QueryValue {
      'S',
      (char*)"AAA",
      0,
      true
    },
    std::vector<QueryValue>{
      {
        'S',
        (char*)"test1",
        0,
        true
      },
      {
        'i',
        (void*)1,
        0,
        true
      }
    }
  });


  documents.push_back(KVPair {
    QueryValue {
      'S',
      (char*)"BBB",
      0,
      true
    },
    std::vector<QueryValue>{
      {
        'S',
        (char*)"test2",
        0,
        true
      },
      {
        'i',
        (void*)3,
        0,
        true
      }
    }
  });

  documents.push_back(KVPair {
    QueryValue {
      'S',
      (char*)"CCC",
      0,
      true
    },
    std::vector<QueryValue>{
      {
        'S',
        (char*)"test3",
        0,
        true
      },
      {
        'i',
        (void*)5,
        0,
        true
      }
    }
  });


  documents.push_back(KVPair {
    QueryValue {
      'S',
      (char*)"DDD",
      0,
      true
    },
    std::vector<QueryValue>{
      {
        'S',
        (char*)"test4",
        0,
        true
      },
      {
        'i',
        (void*)6,
        0,
        true
      }
    }
  });
  table->insertMany(session, &documents);
  session->session->close(session->session, NULL);

  db->conn->open_session(db->conn, NULL, NULL, &wtSession);
  WT_CURSOR* wtIndexCursor;
  session->session->open_cursor(session->session, "index:Hello:int", NULL, NULL, &wtIndexCursor);
  Cursor* indexCursor = new Cursor(wtIndexCursor);
  // indexCursor->set_key(indexCursor, 3);
  // indexCursor->search(indexCursor);
  int myError = 0;
  key[0].queryValue.noFree = true;
  while ((myError = indexCursor->next()) == 0) {
    myError = indexCursor->getKey(key);
    printf("Found: %d %d\n", key->queryValue.value, myError);
  }
  wtIndexCursor->close(wtIndexCursor);
  session->session->close(session->session, NULL);
  db->conn->close(db->conn, NULL);
}

int main() {
  // int x = runRegular();
  runWrapped();
  return 0;
}
