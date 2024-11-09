#include <wiredtiger.h>
#include <vector>
#include <string>

using namespace std;

#ifndef WIREDTIGER_TYPES_H
#define WIREDTIGER_TYPES_H

#define FIELD_PADDING 'x'
#define FIELD_CHAR_ARRAY 's'
#define FIELD_STRING 'S'
#define FIELD_UBYTE 'B'
#define FIELD_BYTE 'b'
#define FIELD_UHALF 'H'
#define FIELD_HALF 'h'
#define FIELD_UINT 'I'
#define FIELD_INT 'i'
#define FIELD_UINT2 'L'
#define FIELD_INT2 'l'
#define FIELD_LONG 'q'
#define FIELD_RECID 'r'
#define FIELD_ULONG 'Q'
#define FIELD_BITFIELD 't'
#define FIELD_BOOLEAN 'T'
#define FIELD_WT_ITEM 'u'
#define FIELD_WT_UITEM 'U' // I don't know what this is, it sometimes gets returned.
#define FIELD_WT_ITEM_DOUBLE 'd'
#define FIELD_WT_ITEM_BIGINT 'z'

#define ERROR_BASE 0x800000
#define CONDITION_NOT_OBJECT (ERROR_BASE | 1)
#define INDEX_NOT_STRING (ERROR_BASE | 2)
#define OPERATION_NOT_INTEGER (ERROR_BASE | 3)
#define VALUES_NOT_ARRAY (ERROR_BASE | 4)
#define CONDITIONS_NOT_ARRAY (ERROR_BASE | 5)
#define INVALID_VALUE_TYPE (ERROR_BASE | 6)
#define VALUES_AND_CONDITIONS (ERROR_BASE | 7)
#define INVALID_OPERATOR (ERROR_BASE | 8)
#define NO_VALUES_OR_CONDITIONS (ERROR_BASE | 9)
#define EMPTY_VALUES (ERROR_BASE | 10)
#define EMPTY_CONDITIONS (ERROR_BASE | 11)

namespace wiredtiger {
  inline bool FieldIsPtr(char field) {
    return field == FIELD_WT_ITEM || field == FIELD_WT_UITEM || field == FIELD_WT_ITEM_DOUBLE || field == FIELD_WT_ITEM_BIGINT || field == FIELD_CHAR_ARRAY || field == FIELD_STRING;
  }
  template <class T> struct ErrorAndResult {
    int error;
    T value;
  };


  typedef union QueryValueValue {
    void* valuePtr/* = NULL*/;
    uint64_t valueUint;
  } QueryValueValue;
  typedef struct QueryValue {
    ~QueryValue() {
      if (FieldIsPtr(dataType)) {
        if (!noFree && value.valuePtr != NULL) {
          free(value.valuePtr);
        }
      }
    }
    // CRITICAL: The first two ites of QueryValue *MUST* match the shape and size of wtItem
    // if we're unable to guarantee this - we should use a struct not a union for QueryValueOrWT_ITEM
    QueryValueValue value;
    size_t size;
    // END CRITICAL
    char dataType;
    bool noFree/* = false*/;
  } QueryValue;

  typedef union QueryValueOrWT_ITEM {
    ~QueryValueOrWT_ITEM() {
      if (FieldIsPtr(queryValue.dataType)) {
        if (!queryValue.noFree && queryValue.value.valuePtr != NULL) {
          free(queryValue.value.valuePtr);
        }
      }
    }
    QueryValue queryValue;
  } QueryValueOrWT_ITEM;

  typedef struct QueryCondition {
    ~QueryCondition() {
      if (subConditions != NULL) {
        delete subConditions;
      }
      if (values != NULL) {
        delete values;
      }
      if (index != NULL) {
        free(index);
      }
    }
    char* index;
    uint8_t operation;
    std::vector<QueryCondition>* subConditions = NULL;
    std::vector<QueryValueOrWT_ITEM>* values = NULL;
  } QueryCondition;

  typedef struct EntryOfVectors {
    ~EntryOfVectors() {
      if (keyArray != NULL) {
        delete keyArray;
      }
      if (valueArray != NULL) {
        delete valueArray;
      }
    }
    std::vector<QueryValueOrWT_ITEM>* keyArray;
    std::vector<QueryValueOrWT_ITEM>* valueArray;
  } EntryOfVectors;
  typedef struct Format {
    char format;
    size_t size;
  } Format;
}
#endif
