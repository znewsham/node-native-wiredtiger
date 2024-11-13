#include <wiredtiger.h>
#include "types.h"
#include "../helpers.h"
#include "helpers.h"

using namespace std;

#ifndef WIREDTIGER_CUSTOM_EXTRACTOR_H
#define WIREDTIGER_CUSTOM_EXTRACTOR_H

#define EXTRACTOR_TYPE_NGRAM 1
#define EXTRACTOR_TYPE_CUSTOM 2

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
}

#endif
