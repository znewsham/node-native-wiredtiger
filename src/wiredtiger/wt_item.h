#include "helpers.h"
#include <vector>
#ifndef WIREDTIGER_WT_ITEM_H
#define WIREDTIGER_WT_ITEM_H
namespace wiredtiger {
  int unpackWtItem(
    WT_SESSION* session,
    const WT_ITEM* item,
    char* cleanedFormat,
    std::vector<Format>* formats,
    std::vector<QueryValue> *queryValues
  );
}
#endif
