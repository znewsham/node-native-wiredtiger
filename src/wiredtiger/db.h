#include <wiredtiger.h>
#include "types.h"
#include "../helpers.h"
#include "helpers.h"
#include "custom_extractor.h"
#include "custom_collator.h"
#include <string.h>
#include <map>

using namespace std;

#ifndef WIREDTIGER_DB_H
#define WIREDTIGER_DB_H

namespace wiredtiger {

static WT_COLLATOR nocasecoll = { compareNoCase, NULL, NULL };
static WT_COLLATOR reversecoll = { compareReverse, NULL, NULL };
static WT_COLLATOR regularcoll = { compareRegular, NULL, NULL };

static WT_EXTRACTOR wordsIndex = { extractWords, NULL, NULL };
static WT_EXTRACTOR ngramsIndex = { extractNgrams, customizeNgrams, terminateNgrams };


class WiredTigerDB {
  private:
	public:
    WT_CONNECTION *conn;

    ~WiredTigerDB() {
      printf("Destroyed?\n");
    }
    WiredTigerDB(): conn(NULL)
    {
    }

    int open(char* directory, char* options) {
      int ret = wiredtiger_open(directory, NULL, options, &this->conn);
      RETURN_IF_ERROR(conn->add_collator(conn, "nocase", &nocasecoll, NULL));
      RETURN_IF_ERROR(conn->add_collator(conn, "reverse", &reversecoll, NULL));
      RETURN_IF_ERROR(conn->add_collator(conn, "regular", &regularcoll, NULL));
      RETURN_IF_ERROR(conn->add_extractor(conn, "words", &wordsIndex, NULL));
      RETURN_IF_ERROR(conn->add_extractor(conn, "ngrams", &ngramsIndex, NULL));
      return ret;
    }

    int addExtractor(char* name, CustomExtractor* extractor) {
      extractor->extractor.extract = CustomExtractor::Extract;
      if (extractor->hasCustomize) {
        extractor->extractor.customize = CustomExtractor::Customize;
      }
      if (extractor->hasTerminate) {
        extractor->extractor.terminate = CustomExtractor::Terminate;
      }
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
