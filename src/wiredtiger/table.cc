#include <wiredtiger.h>
#include "helpers.h"
#include "../helpers.h"
#include "db.h"
#include "table.h"
#include "session.h"

#include <vector>

using namespace std;

namespace wiredtiger {
  WiredTigerTable::WiredTigerTable(WiredTigerDB *_db, char* _tableName, char* _config):
    tableConfig(NULL),
    tableName(NULL),
    db(_db)
  {
    if (_tableName != NULL) {
      tableName = (char*)calloc(sizeof(char), strlen(_tableName) + 1);
      strcpy(tableName, _tableName);
      specName = (char*)calloc(sizeof(char), strlen(_tableName) + 1 + 6); // table:
      sprintf(specName, "table:%s", tableName);
    }
    if (_config != NULL) {
      tableConfig = (char*)calloc(sizeof(char), strlen(_config) + 1);
      strcpy(tableConfig, _config);
    }
  }

  WiredTigerDB* WiredTigerTable::getDb() {
    return db;
  }

  char* WiredTigerTable::getTableName() {
    return tableName;
  }



  int WiredTigerTable::initTable(WiredTigerSession* session) {
    if (isInitted) {
      return 0;
    }
    isInitted = true;
    WT_CONFIG_PARSER* parser;
    RETURN_IF_ERROR(wiredtiger_config_parser_open(NULL, tableConfig, strlen(tableConfig), &parser));
    WT_CONFIG_ITEM item;
    RETURN_IF_ERROR(parser->get(parser, "key_format", &item));
    RETURN_IF_ERROR(parseFormat(item.str, item.len, &keyFormats));
    // free(keyFormat);

    RETURN_IF_ERROR(parser->get(parser, "value_format", &item));
    RETURN_IF_ERROR(parseFormat(item.str, item.len, &valueFormats));

    WiredTigerSession* actualSession = session;
    if (session == NULL) {
      WT_SESSION* wtSession;
      RETURN_IF_ERROR(db->conn->open_session(db->conn, NULL, NULL, &wtSession));
      actualSession = new WiredTigerSession(wtSession);
    }
    int result = actualSession->create(
      specName,
      tableConfig
    );
    if (session == NULL) {
      actualSession->session->close(actualSession->session, NULL);
    }
    return result;
  }

  WiredTigerTable::~WiredTigerTable() {
    if (tableName != NULL) {
      free(tableName);
      free(specName);
    }
    if (tableConfig != NULL) {
      free(tableConfig);
    }
  }
  std::vector<Format>* WiredTigerTable::getKeyFormats() {
    this->initTable(NULL); // TODO pass a session?
    return &keyFormats;
  }
  std::vector<Format>* WiredTigerTable::getValueFormats() {
    this->initTable(NULL); // TODO pass a session?
    return &valueFormats;
  }

  int WiredTigerTable::createIndex(WiredTigerSession* session, char* indexName, char* config) {
    this->initTable(session);
    char* uri = (char*)calloc(sizeof(char), strlen(tableName) + strlen(indexName) + 7);
    sprintf(uri, "index:%s:%s", tableName, indexName);
    RETURN_AND_FREE_IF_ERROR(session->create(uri, config), uri);
    free(uri);
    return 0;
  }


  int WiredTigerTable::insertMany(WiredTigerSession* session, std::vector<KVPair> *documents) {
    this->initTable(session);
    WT_SESSION* wtSession = session->session;
    WT_CURSOR* wtCursor;
    WT_ITEM item;
    WT_ITEM key;
    size_t bufferSize = 512;
    void* buffer = malloc(bufferSize);
    byte keyBuffer[200];// TODO: variable key buffer size
    RETURN_IF_ERROR(wtSession->open_cursor(wtSession, specName, NULL, NULL, &wtCursor));
    Cursor* cursor = new Cursor(wtCursor);
    size_t keySize;
    size_t valueSize;
    QueryValueOrWT_ITEM* keys = cursor->initArray(false, &keySize);
    QueryValueOrWT_ITEM* values = cursor->initArray(true, &valueSize);
    // WT_CURSOR* cursor = wtCursor;
    for(size_t i = 0; i < documents->size(); i++) {
      KVPair document = documents->at(i);
      for (int i = 0; i < keySize; i++) {
        keys[i].queryValue = document.key;
      }
      for (int i = 0; i < valueSize; i++) {
        values[i].queryValue = document.values.at(i);
      }
      cursor->setKey(keys);
      cursor->setValue(values);
      RETURN_IF_ERROR(cursor->insert());
    }
    free(buffer);
    return 0;
  }
}
