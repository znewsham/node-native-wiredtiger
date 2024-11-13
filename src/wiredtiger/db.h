#include <wiredtiger.h>
#include "types.h"
#include "../helpers.h"
#include "helpers.h"
#include "custom_extractor.h"
#include "multi_key_extractor.h"
#include "custom_collator.h"
#include "compound_directional_collator.h"
#include <string.h>
#include <map>

using namespace std;

#ifndef WIREDTIGER_DB_H
#define WIREDTIGER_DB_H

namespace wiredtiger {

static WT_COLLATOR compoundDirectionalCollator = { NULL, CompoundDirectionalCollator::Customize, NULL };

static WT_EXTRACTOR multiKeyIndex = { NULL, MultiKeyExtractor::Customize, NULL };


class WiredTigerDB {
  private:
    bool _isOpen = false;
	public:
    WT_CONNECTION *conn;

    ~WiredTigerDB() {
      // free the collator/extractor?
    }
    WiredTigerDB(): conn(NULL)
    {
    }

    int open(char* directory, char* options) {
      if (_isOpen) {
        return WT_NOTFOUND;// TODO: real error
      }
      RETURN_IF_ERROR(wiredtiger_open(directory, NULL, options, &this->conn));
      RETURN_IF_ERROR(conn->add_collator(conn, "compound", &compoundDirectionalCollator, NULL));
      RETURN_IF_ERROR(conn->add_extractor(conn, "multiKey", &multiKeyIndex, NULL));
      _isOpen = true;
      return 0;
    }

    int close(char* config) {
      if (!_isOpen) {
        return WT_NOTFOUND;// TODO: real error
      }
      _isOpen = false;
      RETURN_IF_ERROR(conn->close(conn, config));
      conn = NULL;
      return 0;
    }

    bool isOpen() {
      return _isOpen;
    }

    int addExtractor(char* name, CustomExtractor* extractor) {
      extractor->extractor.extract = CustomExtractor::Extract;
      // if (extractor->hasCustomize) {
      //   extractor->extractor.customize = CustomExtractor::Customize;
      // }
      // if (extractor->hasTerminate) {
      //   extractor->extractor.terminate = CustomExtractor::Terminate;
      // }
      this->conn->add_extractor(this->conn, name, &extractor->extractor, NULL);
      return 0;
    }

    int addCollator(char* name, CustomCollator* collator) {
      collator->collator.compare = CustomCollator::Compare;
      if (collator->hasCustomize) {
        collator->collator.customize = CustomCollator::Customize;
      }
      if (collator->hasTerminate) {
        collator->collator.terminate = CustomCollator::Terminate;
      }
      this->conn->add_collator(this->conn, name, &collator->collator, NULL);
      return 0;
    }
  };
}

#endif
