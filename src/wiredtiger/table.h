#include <wiredtiger.h>
#include "helpers.h"
#include "../helpers.h"
#include "db.h"
#include "session.h"

#include <cstring>
#include <vector>
#include <memory>

using namespace std;

#ifndef WIREDTIGER_TABLE_H
#define WIREDTIGER_TABLE_H
namespace wiredtiger {
class WiredTigerTable {
  private:
    char* tableConfig;
    char* tableName;
    bool isInitted = false;
    std::vector<Format> keyFormats;
    std::vector<Format> valueFormats;

  protected:
    char* specName;
    WiredTigerDB *db;

  public:

    WiredTigerTable(WiredTigerDB *d, char* _tableName, char* _config);

    ~WiredTigerTable();
    int initTable(WiredTigerSession* session);
    int insertMany(WiredTigerSession* session, std::vector<std::unique_ptr<EntryOfVectors>> *documents);
    int createIndex(WiredTigerSession* session, char* indexName, char* config);
    std::vector<Format>* getKeyFormats();
    std::vector<Format>* getValueFormats();

    // TODO: these shouldn't be needed. Something is wrong with the relationship between table, session, cursor, etc.
    char* getTableName();
    WiredTigerDB* getDb();
};
}
#endif
