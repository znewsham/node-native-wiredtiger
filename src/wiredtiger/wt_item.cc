#include "wt_item.h"
#include <avcall.h>

namespace wiredtiger {
  int unpackWtItem(
    WT_SESSION* session,
    const WT_ITEM* item,
    char* cleanedFormat,
    std::vector<Format>* formats,
    std::vector<QueryValue> *queryValues
  ) {
    av_alist argList;
    int error;
    av_start_int(argList, wiredtiger_struct_unpack, &error);
    av_ptr(argList, WT_SESSION*, session);
    av_ptr(argList, void*, item->data);
    av_uint(argList, item->size);
    av_ptr(argList, char*, cleanedFormat);
    for (size_t i = 0; i < formats->size(); i++) {
      populateAvListForPackingOrUnPacking(
        queryValues,
        NULL,
        &argList,
        i,
        formats->at(i),
        false
      );
      queryValues->at(i).noFree = true;
    }

    av_call(argList);
    return error;
  }
}
