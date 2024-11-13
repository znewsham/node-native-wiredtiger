#include <wiredtiger.h>
#include "helpers.h"
#include "../helpers.h"
#include "db.h"
#include "table.h"
#include "session.h"
#include "multi_cursor.h"

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

  int WiredTigerTable::initTableFormats() {
    if (keyFormats.size() != 0) {
      return 0;
    }
    WT_CONFIG_PARSER* parser;
    RETURN_IF_ERROR(wiredtiger_config_parser_open(NULL, tableConfig, strlen(tableConfig), &parser));
    WT_CONFIG_ITEM item;
    RETURN_IF_ERROR(parser->get(parser, "key_format", &item));
    RETURN_IF_ERROR(parseFormat(item.str, item.len, &keyFormats));

    RETURN_IF_ERROR(parser->get(parser, "value_format", &item));
    RETURN_IF_ERROR(parseFormat(item.str, item.len, &valueFormats));
    return parser->close(parser);
  }



  int WiredTigerTable::initTable(WiredTigerSession* session) {
    if (isInitted) {
      return 0;
    }
    isInitted = true;
    initTableFormats();
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
      actualSession->close(NULL);
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
    initTableFormats();
    return &keyFormats;
  }
  std::vector<Format>* WiredTigerTable::getValueFormats() {
    initTableFormats();
    return &valueFormats;
  }

  int WiredTigerTable::createIndex(WiredTigerSession* session, char* indexName, char* config) {
    this->initTable(session);
    char* uri = (char*)calloc(sizeof(char), strlen(tableName) + strlen(indexName) + 8);
    sprintf(uri, "index:%s:%s", tableName, indexName);
    RETURN_AND_FREE_IF_ERROR(session->create(uri, config), uri);
    free(uri);
    return 0;
  }

  int WiredTigerTable::deleteMany(
    WiredTigerSession* session,
    std::vector<QueryCondition>* conditions,
    int* deletedCount
  ) {
    this->initTable(session);
    this->ensureSpecName();
    Cursor* deleteCursor;
    session->openCursor(specName, &deleteCursor);
    MultiCursor findCursor(session, tableName, conditions);
    int error;
    std::vector<unique_ptr<std::vector<QueryValue>>> keys;
    std::vector<QueryValue>* keyArray = NULL;
    while ((error = findCursor.next(&keyArray)) == 0) {
      keys.push_back(unique_ptr<std::vector<QueryValue>>(keyArray));
      deleteCursor->setKey(keyArray);
      int error = deleteCursor->remove();
      if (error == 0) {
        *deletedCount+=1;
      }
      if (error != 0 && error != WT_NOTFOUND) {
        delete deleteCursor;
        return error;
      }
    }
    delete deleteCursor;
    return 0;
  }

  int WiredTigerTable::insertMany(WiredTigerSession* session, std::vector<std::unique_ptr<EntryOfVectors>> *documents) {
    this->initTable(session);
    this->ensureSpecName();
    Cursor* cursor;
    RETURN_IF_ERROR(session->openCursor(specName, (char*)"overwrite=false", &cursor));
    // WT_CURSOR* cursor = wtCursor;
    for(size_t i = 0; i < documents->size(); i++) {
      EntryOfVectors& document = *documents->at(i);
      cursor->setKey(document.keyArray);
      cursor->setValue(document.valueArray);
      RETURN_AND_DELETE_IF_ERROR(cursor->insert(), cursor);
    }
    delete cursor;
    return 0;
  }

  int WiredTigerTable::updateMany(
    WiredTigerSession* session,
    std::vector<QueryCondition>* conditions,
    std::vector<QueryValue>* newValues,
    int* updatedCount
  ) {
    this->initTable(session);
    this->ensureSpecName();
    Cursor* updateCursor;
    RETURN_IF_ERROR(session->openCursor(specName, (char*)"overwrite=false", &updateCursor));
    MultiCursor findCursor(session, tableName, conditions);
    if (findCursor.error != 0) {
      return findCursor.error;
    }
    int error;
    // we can't delete keys until we're done, because we use them to dedupe overlapping index bounds
    std::vector<unique_ptr<std::vector<QueryValue>>> keys;
    std::vector<QueryValue>* keyArray = NULL;
    std::vector<QueryValue>* valueArray = NULL;
    int i = 0;
    while ((error = findCursor.next(&keyArray, &valueArray)) == 0) {
      keys.push_back(unique_ptr<std::vector<QueryValue>>(keyArray));
      updateCursor->setKey(keyArray);
      for (size_t i = 0; i < valueArray->size(); i++) {
        if ((*newValues)[i].dataType != FIELD_PADDING) {
          (*valueArray)[i].value.valuePtr = (*newValues)[i].value.valuePtr;
          (*valueArray)[i].value.valueUint = (*newValues)[i].value.valueUint;
          (*valueArray)[i].size = (*newValues)[i].size;
        }
      }
      updateCursor->setValue(valueArray);
      int error = updateCursor->update();
      if (error == 0) {
        *updatedCount+=1;
      }
      delete valueArray;
      if (error != 0 && error != WT_NOTFOUND) {
        delete updateCursor;
        return error;
      }
      RETURN_AND_DELETE_IF_ERROR(updateCursor->reset(), updateCursor);
    }
    delete updateCursor;
    return 0;
  }
}
