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
  QueryValueOrWT_ITEM* Cursor::initArray(bool forValues, size_t* size) {
    init();
    *size = isRaw ? 1 : (forValues ? valueValueFormats : keyValueFormats).size();
    QueryValueOrWT_ITEM* pointerArray = (QueryValueOrWT_ITEM*)calloc(sizeof(QueryValueOrWT_ITEM), *size);
    return pointerArray;
  }

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
    std::vector<QueryValueOrWT_ITEM>* valueArray,
    av_alist* argList,
    bool forValues,
    bool forWrite
  ) {
    std::vector<WT_ITEM>* wtItems;
    if (forWrite) {
      wtItems = new std::vector<WT_ITEM>(valueArray->size());
    }
    for (size_t i = 0; i < columnCount(forValues); i++) {
      Format format = formatAt(forValues, i);
      if (
        format.format == FIELD_WT_ITEM
        || format.format == FIELD_WT_UITEM
        || format.format == FIELD_WT_ITEM_BIGINT
        || format.format == FIELD_WT_ITEM_DOUBLE
      ) {
        // when writing, valueArray should be considered const and not modified
        // setting valueArray.wtItem.* relies on the shape of wtItem and queryValue being identical. Gross.
        // for reading - there is no such requirement and we dont want the cost of allocating a new WT_ITEM vector
        if (forWrite) {
          (*wtItems)[i].data = (*valueArray)[i].queryValue.value.valuePtr;
          (*wtItems)[i].size = (*valueArray)[i].queryValue.size;
          av_ptr(*argList, WT_ITEM*, &(*wtItems)[i]);
        }
        else {
          // we fake that a QueryValue is a WT_ITEM (they're at least the same size)
          // in populate we'll pull the data out and rebuild this item correctly
          av_ptr(*argList, WT_ITEM*, &(*valueArray)[i]);
        }
      }
      else {
        if (forWrite) {
          if (FieldIsPtr(format.format)) {
            av_ptr(*argList, void*, (*valueArray)[i].queryValue.value.valuePtr);
          }
          else {
            av_uint(*argList, (*valueArray)[i].queryValue.value.valueUint);
          }
        }
        else {
          if (FieldIsPtr(format.format)) {
            av_ptr(*argList, void*, &(*valueArray)[i].queryValue.value.valuePtr);
          }
          else {
            av_ptr(*argList, void*, &(*valueArray)[i].queryValue.value.valueUint);
          }
        }
      }
    }
    if (forWrite) {
      delete wtItems;
    }
  }

  void Cursor::populate(
    void (*funcptr)(WT_CURSOR* cursor, ...),
    std::vector<QueryValueOrWT_ITEM>* valueArray,
    bool forValues
  ) {
    init();
    av_alist argList;
    av_start_void(argList, funcptr);
    av_ptr(argList, WT_CURSOR*, cursor);
    populateInner(valueArray, &argList, forValues, true);
    av_call(argList);
  }

  int Cursor::populate(
    int (*funcptr)(WT_CURSOR* cursor, ...),
    std::vector<QueryValueOrWT_ITEM>* valueArray,
    bool forValues
  ) {
    init();
    av_alist argList;
    int result;
    av_start_int(argList, funcptr, &result);
    av_ptr(argList, WT_CURSOR*, cursor);
    populateInner(valueArray, &argList, forValues, false);
    av_call(argList);
    for (size_t i = 0; i < columnCount(forValues); i++) {
      Format format = formatAt(forValues, i);
      // TODO: I don't love that the cursor needs to know about this - I think this should use the raw types
      if (
        format.format == FIELD_WT_ITEM
        || format.format == FIELD_WT_UITEM
        || format.format == FIELD_WT_ITEM_BIGINT
        || format.format == FIELD_WT_ITEM_DOUBLE
      ) {
        // implementation must match populateInner's binary read.
        WT_ITEM* wtItem = (WT_ITEM*)&(*valueArray)[i];
        void* value = (void*)wtItem->data;
        size_t size = wtItem->size;

        (*valueArray)[i].queryValue.size = size;
        (*valueArray)[i].queryValue.value.valuePtr = (void*)value;

      }
      if (format.size && !(*valueArray)[i].queryValue.size) {
        (*valueArray)[i].queryValue.size = format.size;
      }
      // for better or worse, we store/access values in the same place "void* value" - but sometimes its just a real value (e.g., any time other than the above)
      // for worse, on set/insert or whatever, we store an actual pointer in these fields (e.g., int*).
      // more confusing is that despite both these facts, we never want to free these items - since the memory is always owned/managed by WT.
      // setting noFree allows us to reuse helpers around queryValue and free the whole thing in a consistent way
      (*valueArray)[i].queryValue.noFree = true;
      (*valueArray)[i].queryValue.dataType = format.format;
    }

    // it is vital that at this point, variable (non null terminated) items have a size associated
    // critically U, S (when the format has a size) and s must have a size associated.
    // this is primarily handled by passing any size assigned to the format onto the queryValue in the case that a size is missing
    return result;
  }

  void Cursor::populateRaw(
    void (*funcptr)(WT_CURSOR* cursor, ...),
    std::vector<QueryValueOrWT_ITEM>* valueArray
  ) {
    WT_ITEM item;
    item.size = (*valueArray)[0].queryValue.size;
    item.data = (*valueArray)[0].queryValue.value.valuePtr;
    funcptr(cursor, &item);
  }

  int Cursor::populateRaw(
    int (*funcptr)(WT_CURSOR* cursor, ...),
    std::vector<QueryValueOrWT_ITEM>* valueArray
  ) {
    WT_ITEM item;
    int result = funcptr(cursor, &item);
    RETURN_IF_ERROR(result);
    (*valueArray)[0].queryValue.dataType = FIELD_WT_ITEM;
    (*valueArray)[0].queryValue.value.valuePtr = (void*)item.data;
    (*valueArray)[0].queryValue.size = item.size;
    (*valueArray)[0].queryValue.noFree = true;
    return result;
  }

  int Cursor::getValue(std::vector<QueryValueOrWT_ITEM>* valueArray) {
    init();
    if (isRaw) {
      return populateRaw(cursor->get_value, valueArray);
    }
    else {
      return populate(cursor->get_value, valueArray, true);
    }
  }

  void Cursor::setValue(std::vector<QueryValueOrWT_ITEM>* valueArray) {
    init();
    if (isRaw) {
      populateRaw(cursor->set_value, valueArray);
    }
    else {
      populate(cursor->set_value, valueArray, true);
    }
  }

  int Cursor::getKey(std::vector<QueryValueOrWT_ITEM>* valueArray) {
    init();
    if (isRaw) {
      return populateRaw(cursor->get_key, valueArray);
    }
    else {
      return populate(cursor->get_key, valueArray, false);
    }
  }

  void Cursor::setKey(std::vector<QueryValueOrWT_ITEM>* valueArray) {
    init();
    if (isRaw) {
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
