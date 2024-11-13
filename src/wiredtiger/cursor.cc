#include <wiredtiger.h>
#include "types.h"
#include "helpers.h"
#include "../helpers.h"
#include "cursor.h"

#include <vector>
#include <cstring>
#include <avcall.h>
using namespace std;

namespace wiredtiger {
  size_t Cursor::columnCount(bool forValues) {
    if (isRaw) {
      return 1;
    }
    init();
    std::vector<Format> *formats = forValues ? &valueValueFormats : &keyValueFormats;
    return formats->size();
  }
  Format Cursor::formatAt(bool forValues, int i) {
    init();
    if (isRaw) {
      return { 'u', 0 };
    }
    std::vector<Format> *formats = forValues ? &valueValueFormats : &keyValueFormats;

    return formats->at(i);
  }

  void Cursor::populateInner(
    std::vector<QueryValue>* valueArray,
    std::vector<WT_ITEM>* wtItems,
    av_alist* argList,
    bool forValues,
    bool forWrite
  ) {
    for (size_t i = 0; i < columnCount(forValues); i++) {
      populateAvListForPackingOrUnPacking(
        valueArray,
        wtItems,
        argList,
        i,
        formatAt(forValues, i),
        forWrite
      );
    }
  }

  void Cursor::populate(
    void (*funcptr)(WT_CURSOR* cursor, ...),
    std::vector<QueryValue>* valueArray,
    bool forValues
  ) {
    std::vector<WT_ITEM> wtItems(valueArray->size());
    init();
    av_alist argList;
    av_start_void(argList, funcptr);
    av_ptr(argList, WT_CURSOR*, cursor);
    populateInner(valueArray, &wtItems, &argList, forValues, true);
    av_call(argList);
  }

  int Cursor::populate(
    int (*funcptr)(WT_CURSOR* cursor, ...),
    std::vector<QueryValue>* valueArray,
    bool forValues
  ) {
    init();
    av_alist argList;
    int result;
    av_start_int(argList, funcptr, &result);
    av_ptr(argList, WT_CURSOR*, cursor);
    populateInner(valueArray, NULL, &argList, forValues, false);
    av_call(argList);
    for (size_t i = 0; i < columnCount(forValues); i++) {
      Format format = formatAt(forValues, i);
      if (FieldIsWTItem(format.format)) {
        // implementation must match populateInner's binary read.
        WT_ITEM* wtItem = (WT_ITEM*)&(*valueArray)[i];
        void* value = (void*)wtItem->data;
        size_t size = wtItem->size;

        (*valueArray)[i].size = size;
        (*valueArray)[i].value.valuePtr = (void*)value;

      }
      if (format.size && !(*valueArray)[i].size) {
        (*valueArray)[i].size = format.size;
      }
      // for better or worse, we store/access values in the same place "void* value" - but sometimes its just a real value (e.g., any time other than the above)
      // for worse, on set/insert or whatever, we store an actual pointer in these fields (e.g., int*).
      // more confusing is that despite both these facts, we never want to free these items - since the memory is always owned/managed by WT.
      // setting noFree allows us to reuse helpers around queryValue and free the whole thing in a consistent way
      (*valueArray)[i].noFree = true;
      (*valueArray)[i].dataType = format.format;
    }

    // it is vital that at this point, variable (non null terminated) items have a size associated
    // critically U, S (when the format has a size) and s must have a size associated.
    // this is primarily handled by passing any size assigned to the format onto the queryValue in the case that a size is missing
    return result;
  }

  void Cursor::populateRaw(
    void (*funcptr)(WT_CURSOR* cursor, ...),
    std::vector<QueryValue>* valueArray
  ) {
    WT_ITEM item;
    item.size = (*valueArray)[0].size;
    item.data = (*valueArray)[0].value.valuePtr;
    funcptr(cursor, &item);
  }

  int Cursor::populateRaw(
    int (*funcptr)(WT_CURSOR* cursor, ...),
    std::vector<QueryValue>* valueArray
  ) {
    WT_ITEM item;
    int result = funcptr(cursor, &item);
    RETURN_IF_ERROR(result);
    (*valueArray)[0].dataType = FIELD_WT_ITEM;
    (*valueArray)[0].value.valuePtr = (void*)item.data;
    (*valueArray)[0].size = item.size;
    (*valueArray)[0].noFree = true;
    return result;
  }

  int Cursor::getValue(std::vector<QueryValue>* valueArray) {
    init();
    if (isRaw) {
      return populateRaw(cursor->get_value, valueArray);
    }
    else {
      return populate(cursor->get_value, valueArray, true);
    }
  }

  void Cursor::setValue(std::vector<QueryValue>* valueArray) {
    init();
    if (isRaw) {
      populateRaw(cursor->set_value, valueArray);
    }
    else {
      populate(cursor->set_value, valueArray, true);
    }
  }

  int Cursor::getKey(std::vector<QueryValue>* valueArray) {
    init();
    if (isRaw) {
      return populateRaw(cursor->get_key, valueArray);
    }
    else {
      return populate(cursor->get_key, valueArray, false);
    }
  }

  void Cursor::setKey(std::vector<QueryValue>* valueArray) {
    init();
    if (isRaw) {
      printf("Raw mode\n");
      populateRaw(cursor->set_key, valueArray);
    }
    else {
      populate(cursor->set_key, valueArray, false);
    }
  }

  int Cursor::search() {
    return cursor->search(cursor);
  }

  int Cursor::searchNear(int* exact) {
    int res = cursor->search_near(cursor, exact);
    return res;
  }

  int Cursor::update() {
    return cursor->update(cursor);
  }

  int Cursor::insert() {
    return cursor->insert(cursor);
  }


  int Cursor::remove() {
    return cursor->remove(cursor);
  }

  int Cursor::next() {
    return cursor->next(cursor);
  }

  int Cursor::prev() {
    return cursor->prev(cursor);
  }

  int Cursor::close() {
    isClosed = true;
    return cursor->close(cursor);
  }

  int Cursor::reset() {
    return cursor->reset(cursor);
  }

  int Cursor::bound(char* config) {
    return cursor->bound(cursor, config);
  }
  int Cursor::compare(Cursor* other, int* comp) {
    return cursor->compare(cursor, other->cursor, comp);
  }
  int Cursor::equals(Cursor* other, int* equals) {
    return cursor->equals(cursor, other->cursor, equals);
  }

  int Cursor::init() {
    if (isInitted) {
      return 0;
    }
    isRaw = (cursor->flags & WT_CURSTD_RAW) == WT_CURSTD_RAW;
    isInitted = true;
    if (strcmp(cursor->key_format, "S") != 0) {
      RETURN_IF_ERROR(parseFormat(cursor->key_format, &this->keyValueFormats));
    }
    else {
      keyValueFormats.push_back({ 'S', 0 });
    }
    if (cursor->value_format != NULL && strcmp(cursor->value_format, "S") != 0 /*&& strcmp(cursor->value_format, "") != 0 removed because it makes it hard to just pull the ID*/) {
      RETURN_IF_ERROR(parseFormat(cursor->value_format, &this->valueValueFormats));
    }
    else {
      valueValueFormats.push_back({ 'S', 0 });
    }
    return 0;
  }
}
