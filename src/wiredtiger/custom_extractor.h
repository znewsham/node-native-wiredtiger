#include <wiredtiger.h>
#include "types.h"
#include "../helpers.h"
#include "helpers.h"
#include <string.h>
#include <map>

using namespace std;

#ifndef WIREDTIGER_CUSTOM_EXTRACTOR_H
#define WIREDTIGER_CUSTOM_EXTRACTOR_H

namespace wiredtiger {

  class CustomExtractor {
    public:
      WT_EXTRACTOR extractor;
      bool hasTerminate = false;
      bool hasCustomize = false;
      virtual int extract(
        WT_SESSION *session,
        const WT_ITEM *key,
        const WT_ITEM *value,
        WT_CURSOR *result_cursor
      ) = 0;

      virtual int customize(
        WT_SESSION *session,
        const char *uri,
        WT_CONFIG_ITEM *appcfg,
        WT_EXTRACTOR **customp
      ) = 0;

      virtual int terminate(
        WT_SESSION* session
      ) = 0;

      static int Terminate(
        WT_EXTRACTOR* extractor,
        WT_SESSION* session
      );

      static CustomExtractor* CustomExtractorForExtractor(WT_EXTRACTOR* extractor) {
        CustomExtractor *potential = (CustomExtractor*) extractor;
        // what a fooking mess - but at least if we move extractor somewhere else, the offset system should work.
        uint64_t offset = ((char*)&potential->extractor - (char*)potential);
        return (CustomExtractor*)((char*)extractor - offset);
      }

      static int Customize(
        WT_EXTRACTOR *extractor,
        WT_SESSION *session,
        const char *uri,
        WT_CONFIG_ITEM *appcfg,
        WT_EXTRACTOR **customp
      );

      static int Extract(
        WT_EXTRACTOR *extractor,
        WT_SESSION *session,
        const WT_ITEM *key,
        const WT_ITEM *value,
        WT_CURSOR *result_cursor
      );
  };

  int terminateNgrams(
    WT_EXTRACTOR* extractor,
    WT_SESSION* session
  );

  int customizeNgrams(
    WT_EXTRACTOR *extractor,
    WT_SESSION *session,
    const char *uri,
    WT_CONFIG_ITEM *appcfg,
    WT_EXTRACTOR **customp
  );

  int extractNgrams(
    WT_EXTRACTOR *extractor,
    WT_SESSION *session,
    const WT_ITEM *key,
    const WT_ITEM *value,
    WT_CURSOR *result_cursor
  );

  int extractWords(
    WT_EXTRACTOR *extractor,
    WT_SESSION *session,
    const WT_ITEM *key,
    const WT_ITEM *value,
    WT_CURSOR *result_cursor
  );

  union ExtractorConfig {
    // TODO: more than just ngram?
    int ngram;
  };

  typedef struct ExtractorReferences {
    WT_EXTRACTOR extractor;
    int references;
    char* key;
    ExtractorConfig config;
  } ExtractorReferences;
}

#endif
