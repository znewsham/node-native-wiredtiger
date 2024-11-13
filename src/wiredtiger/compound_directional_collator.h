#include <wiredtiger.h>
#include "types.h"
#include "../helpers.h"
#include "helpers.h"
#include <string.h>
#include <map>
#include <memory>

using namespace std;

#ifndef WIREDTIGER_COMPOUND_DIRECTIONAL_COLLATOR_H
#define WIREDTIGER_COMPOUND_DIRECTIONAL_COLLATOR_H

namespace wiredtiger {

  enum class CollatorDirection {
    REVERSE = -1,
    NONE = 0,
    FORWARD = 1
  };

  struct CompoundDirectionalCollatorColumnConfig {
    Format format;
    CollatorDirection direction = CollatorDirection::FORWARD;
  };

  class CompoundDirectionalCollator {
    public:
      // critical that it's first
      WT_COLLATOR collator;
    private:
      CollatorDirection direction = CollatorDirection::FORWARD;
      int extractColumnsConfiguration(
        WT_SESSION *session,
        WT_CONFIG_PARSER* config_parser
      );
      int extractColumnConfiguration(
        WT_SESSION *session,
        WT_CONFIG_ITEM *columnCfg
      );
    public:
      CompoundDirectionalCollator(char* indexId): key(indexId) {
        columnConfigs = new std::vector<CompoundDirectionalCollatorColumnConfig>();
        extractFormat = new std::vector<Format>();
        collator.compare = CompoundDirectionalCollator::Compare;
        collator.terminate = CompoundDirectionalCollator::Terminate;
      }
      ~CompoundDirectionalCollator() {
        delete columnConfigs;
        delete extractFormat;
        free(key);
        free(keyFormat);
      }
      int references;
      char* key;
      char* keyFormat;

      std::vector<Format>* extractFormat;
      std::vector<CompoundDirectionalCollatorColumnConfig>* columnConfigs;

    int compare(
      WT_SESSION *session,
      const WT_ITEM *v1,
      const WT_ITEM *v2,
      int* compare
    );

    static int Compare(
      WT_COLLATOR *collator,
      WT_SESSION *session,
      const WT_ITEM *v1,
      const WT_ITEM *v2,
      int* compare
    );

    static int Customize(
      WT_COLLATOR *collator,
      WT_SESSION *session,
      const char *uri,
      WT_CONFIG_ITEM *appcfg,
      WT_COLLATOR **customp
    );

    static int Terminate(
      WT_COLLATOR* collator,
      WT_SESSION* session
    );
  };
}

#endif
