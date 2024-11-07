#include <wiredtiger.h>
#include "types.h"
#include "../helpers.h"
#include "helpers.h"
#include <string.h>
#include <map>

using namespace std;

#ifndef WIREDTIGER_CUSTOM_COLLATOR_H
#define WIREDTIGER_CUSTOM_COLLATOR_H

namespace wiredtiger {

  class CustomCollator {
    public:
      WT_COLLATOR collator;
      bool hasTerminate = false;
      bool hasCustomize = false;

      virtual int compare(
        WT_SESSION* session,
        const WT_ITEM* v1,
        const WT_ITEM* v2,
        int* cmp
      ) = 0;

      virtual int customize(
        WT_SESSION* session,
        const char* uri,
        WT_CONFIG_ITEM* appcfg,
        WT_COLLATOR** customp
      ) = 0;

      virtual int terminate(
        WT_SESSION* session
      ) = 0;

      static CustomCollator* CustomCollatorForCollator(WT_COLLATOR* collator) {
        CustomCollator *potential = (CustomCollator*) collator;
        // what a fooking mess - but at least if we move extractor somewhere else, the offset system should work.
        uint64_t offset = ((char*)&potential->collator - (char*)potential);
        return (CustomCollator*)((char*)collator - offset);
      }

      static int Terminate(
        WT_COLLATOR* collator,
        WT_SESSION* session
      );

      static int Customize(
        WT_COLLATOR* collator,
        WT_SESSION* session,
        const char* uri,
        WT_CONFIG_ITEM* appcfg,
        WT_COLLATOR** customp
      );

      static int Compare(
        WT_COLLATOR* collator,
        WT_SESSION* session,
        const WT_ITEM* v1,
        const WT_ITEM* v2,
        int* result_cursor
      );
  };

  int compareNoCase(
    WT_COLLATOR *collator,
    WT_SESSION *session,
    const WT_ITEM *v1,
    const WT_ITEM *v2,
    int *cmp
  );

  int compareReverse(
    WT_COLLATOR *collator,
    WT_SESSION *session,
    const WT_ITEM *v1,
    const WT_ITEM *v2,
    int *cmp
  );

  int compareRegular(
    WT_COLLATOR *collator,
    WT_SESSION *session,
    const WT_ITEM *v1,
    const WT_ITEM *v2,
    int *cmp
  );

}

#endif
