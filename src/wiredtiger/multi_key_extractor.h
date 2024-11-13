#include <wiredtiger.h>
#include "types.h"
#include "../helpers.h"
#include "helpers.h"
#include <string.h>
#include <memory>
#include <vector>
#include <set>

using namespace std;

#ifndef WIREDTIGER_MULTI_KEY_EXTRACTOR_H
#define WIREDTIGER_MULTI_KEY_EXTRACTOR_H

namespace wiredtiger {
  enum class ExtractorType {
    NONE,
    NGRAM,
    WORDS
  };

  struct MultiKeyExtractorColumnConfig {
    std::vector<int> columns;
    ExtractorType extractorType = ExtractorType::NONE;
    Format format;

    // TODO: general config
    size_t ngrams;
  };

  class MultiKeyExtractor {
    public:
      // critical that it's first
      WT_EXTRACTOR extractor;
    private:
      int recurseCustomFields(
        WT_CURSOR* result_cursor,
        std::vector<QueryValue>* valueValues,
        std::vector<WT_ITEM>* wtItems,
        std::vector<unique_ptr<std::vector<unique_ptr<char>>>>* multikeys,
        size_t writeIndex,
        size_t multikeyIndex,
        std::vector<QueryValue> tempValues // intentionally *NOT* a ptr or ref
      );
      int extractColumnConfiguration(
        WT_SESSION *session,
        WT_CONFIG_ITEM *columnCfg
      );
      int extractColumnsConfiguration(
        WT_SESSION *session,
        WT_CONFIG_PARSER* config_parser
      );
    public:
      MultiKeyExtractor(char* _tableFormatStr, char* indexId): key(indexId), tableFormatStr(_tableFormatStr) {
        columnConfigs = new std::vector<MultiKeyExtractorColumnConfig>();
        extractFormat = new std::vector<Format>();
        keyFormat = new std::vector<Format>();
        extractor.extract = MultiKeyExtractor::Extract;
        extractor.terminate = MultiKeyExtractor::Terminate;
      }
      ~MultiKeyExtractor() {
        delete extractFormat;
        delete keyFormat;
        delete columnConfigs;
        free(tableFormatStr);
        free(key);
      }
      int references;
      char* key;

      char* tableFormatStr;
      std::vector<Format>* extractFormat;
      std::vector<Format>* keyFormat;
      std::vector<MultiKeyExtractorColumnConfig>* columnConfigs;

    int extract(
      WT_SESSION *session,
      const WT_ITEM *key,
      const WT_ITEM *value,
      WT_CURSOR *result_cursor
    );

    static int Extract(
      WT_EXTRACTOR *extractor,
      WT_SESSION *session,
      const WT_ITEM *key,
      const WT_ITEM *value,
      WT_CURSOR *result_cursor
    );

    static int Customize(
      WT_EXTRACTOR *extractor,
      WT_SESSION *session,
      const char *uri,
      WT_CONFIG_ITEM *appcfg,
      WT_EXTRACTOR **customp
    );

    static int Terminate(
      WT_EXTRACTOR* extractor,
      WT_SESSION* session
    );
  };
}

#endif
