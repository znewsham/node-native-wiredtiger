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
  template <class T> struct ErrorAndResult {
    int error;
    T value;
  };

  typedef struct DataPointersAndSize {
    size_t size;
    std::vector<std::string> keys;
  } DataPointersAndSize;


  typedef struct QueryValue {
    char dataType;
    void* value/* = NULL*/;
    size_t size;
    bool noFree/* = false*/;
  } QueryValue;

  typedef struct KVPair {
    QueryValue key;
    std::vector<QueryValue> values;
  } KVPair;

  typedef struct QueryCondition {
    char* index;
    uint8_t operation;
    std::vector<QueryCondition> subConditions;
    std::vector<QueryValue> values;
  } QueryCondition;

  typedef struct Entry {
    std::vector<QueryValue>keys;
    std::vector<QueryValue>values;
    void* keyBuffer;
    void* valueBuffer;
  } Entry;

  typedef union QueryValueOrWT_ITEM {
    QueryValue queryValue;
    WT_ITEM wtItem;
  } QueryValueOrWT_ITEM;

  typedef struct EntryOfPointers {
    QueryValueOrWT_ITEM* keyArray;
    size_t keySize;
    QueryValueOrWT_ITEM* valueArray;
    size_t valueSize;
  } EntryOfPointers;
  typedef struct Format {
    char format;
    size_t size;
  } Format;
}
#endif
