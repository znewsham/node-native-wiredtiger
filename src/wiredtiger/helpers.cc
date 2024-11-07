#include <wiredtiger.h>
#include "find_cursor.h"
#include "helpers.h"
#include "../helpers.h"


#include <cstring>
#include <vector>

using namespace std;

double _x = -1;
char* _y = (char*)&_x;
bool isBigEndian = (uint8_t)_y[7] == 0;

namespace wiredtiger {
void* growBuffer(void* existing, size_t existingLength, size_t additionalLength) {
  void* buffer = malloc(existingLength + additionalLength);
  if (existing != NULL) {
    memcpy(buffer, existing, existingLength);
    free(existing);
  }
  bzero((char*)buffer + existingLength, additionalLength);
  return buffer;
}

int parseFormat(const char* format, int formatLength, std::vector<Format> *formats) {
  size_t size = 0;
  for (int i = 0; i < formatLength; i++) {
    char c = format[i];
    if (c >= 48 && c <= 57) {
      size *= 10;
      // we don't care about lengths (for now)
      size += (c - 48);

      continue;
    }
    switch (c) {
      case FIELD_CHAR_ARRAY:
        formats->push_back({ c, (size_t)max((int)size, 1) }); // a char array has a default size of 1 - if nothing is specified
        size = 0;
        break;

      case FIELD_STRING:
        formats->push_back({ c, size }); // if a null terminated string has a length, it may store a real char instead of the nul terminator
        size = 0;
        break;

      case FIELD_BYTE:
      case FIELD_HALF:
      case FIELD_INT:
      case FIELD_INT2:
      case FIELD_UBYTE:
      case FIELD_UHALF:
      case FIELD_UINT:
      case FIELD_UINT2:
      case FIELD_LONG:
      case FIELD_ULONG:
      case FIELD_RECID:
        formats->push_back({ c, 0 });
        size = 0;
        break;

      case FIELD_BITFIELD:
        formats->push_back({ c, size });
        size = 0;
        break;


      case FIELD_WT_ITEM:
      case FIELD_WT_UITEM:
        formats->push_back({ c, size });
        size = 0;
        break;

      // mapped types of u - technically their sizees are different (bits for BOOLEAN and bytes for the others)
      case FIELD_WT_ITEM_BIGINT:
      case FIELD_WT_ITEM_DOUBLE:
      case FIELD_BOOLEAN:
        formats->push_back({ c, size });
        size = 0;
        break;
      case FIELD_PADDING:
        // this should only happen with indexes, as far as I know you don't need to do anything
        break;
      default:
        printf("TODO: unhandled format: %c\n", c);
        return INVALID_OPERATOR; // TODO real error code
    }
  }
  return 0;
}


void flipSign(
  uint8_t* valueBuffer,
  bool positive // the positive condition changes depending on whether we're going out or in. Going out the MSB will be 1
) {
  if (positive) {
    valueBuffer[0] = valueBuffer[0] ^ 0x80;
    valueBuffer[1] = valueBuffer[1] ^ 0x00;
    valueBuffer[2] = valueBuffer[2] ^ 0x00;
    valueBuffer[3] = valueBuffer[3] ^ 0x00;
    valueBuffer[4] = valueBuffer[4] ^ 0x00;
    valueBuffer[5] = valueBuffer[5] ^ 0x00;
    valueBuffer[6] = valueBuffer[6] ^ 0x00;
    valueBuffer[7] = valueBuffer[7] ^ 0x00;
  }
  else {
    valueBuffer[0] = valueBuffer[0] ^ 0xff;
    valueBuffer[1] = valueBuffer[1] ^ 0xff;
    valueBuffer[2] = valueBuffer[2] ^ 0xff;
    valueBuffer[3] = valueBuffer[3] ^ 0xff;
    valueBuffer[4] = valueBuffer[4] ^ 0xff;
    valueBuffer[5] = valueBuffer[5] ^ 0xff;
    valueBuffer[6] = valueBuffer[6] ^ 0xff;
    valueBuffer[7] = valueBuffer[7] ^ 0xff;
  }
}

// USE EXCESSIVE CARE WITH THESE TWO - mongo (seems?) converts to LE, but we use BE because https://stackoverflow.com/questions/43299299/sorting-floating-point-values-using-their-byte-representation
int doubleToByteArray(
  double value,
  uint8_t* buffer
) {
  double innerValue = value;
  uint8_t* valueBuffer = (uint8_t*)&innerValue;
  if (isBigEndian) {
    buffer[0] = valueBuffer[0];
    buffer[1] = valueBuffer[1];
    buffer[2] = valueBuffer[2];
    buffer[3] = valueBuffer[3];
    buffer[4] = valueBuffer[4];
    buffer[5] = valueBuffer[5];
    buffer[6] = valueBuffer[6];
    buffer[7] = valueBuffer[7];
  }
  else {
    buffer[0] = valueBuffer[7];
    buffer[1] = valueBuffer[6];
    buffer[2] = valueBuffer[5];
    buffer[3] = valueBuffer[4];
    buffer[4] = valueBuffer[3];
    buffer[5] = valueBuffer[2];
    buffer[6] = valueBuffer[1];
    buffer[7] = valueBuffer[0];
  }
  flipSign(buffer, (buffer[0] & 0x80) == 0x00);
  return 0;
}

void forceBE(uint8_t* valueBuffer) {
  uint8_t swap = valueBuffer[0];
  valueBuffer[0] = valueBuffer[7];
  valueBuffer[7] = swap;

  swap = valueBuffer[1];
  valueBuffer[1] = valueBuffer[6];
  valueBuffer[6] = swap;

  swap = valueBuffer[2];
  valueBuffer[2] = valueBuffer[5];
  valueBuffer[5] = swap;

  swap = valueBuffer[3];
  valueBuffer[3] = valueBuffer[4];
  valueBuffer[4] = swap;
}

// USE EXCESSIVE CARE WITH THESE TWO - mongo (seems?) converts to LE, but we use BE because https://stackoverflow.com/questions/43299299/sorting-floating-point-values-using-their-byte-representation
int byteArrayToDouble(
  uint8_t* valueBuffer,
  double* value
) {
  uint8_t* buffer = (uint8_t*)value;
  flipSign(valueBuffer, (valueBuffer[0] & 0x80) == 0x80);

  if (isBigEndian) {
    buffer[0] = valueBuffer[0];
    buffer[1] = valueBuffer[1];
    buffer[2] = valueBuffer[2];
    buffer[3] = valueBuffer[3];
    buffer[4] = valueBuffer[4];
    buffer[5] = valueBuffer[5];
    buffer[6] = valueBuffer[6];
    buffer[7] = valueBuffer[7];
  }
  else {
    buffer[0] = valueBuffer[7];
    buffer[1] = valueBuffer[6];
    buffer[2] = valueBuffer[5];
    buffer[3] = valueBuffer[4];
    buffer[4] = valueBuffer[3];
    buffer[5] = valueBuffer[2];
    buffer[6] = valueBuffer[1];
    buffer[7] = valueBuffer[0];
  }

  return 0;
}

int makeBigIntByteArraySortable(size_t wordCount, uint8_t* buffer) {
  if (!isBigEndian) {
    for (size_t i = 1; i <= wordCount / 2; i++) {
      uint8_t swap = buffer[i];
      buffer[i] = buffer[wordCount - i];
      buffer[wordCount - i] = swap;
    }
  }
  if ((buffer[0] & 0x01) == 0x00) {
    // we're positive - we're gonna flip the sign to 1 and do nothing else
    buffer[0] = 1;
    return 0;
  }
  // we must be negative - flip every bit
  buffer[0] = 0;
  for (size_t i = 1; i < wordCount; i+= 8) {
    flipSign(&buffer[i], false);
  }
  return 0;
}

int unmakeBigIntByteArraySortable(size_t wordCount, uint8_t* buffer) {
  if (!isBigEndian) {
    for (size_t i = 1; i <= wordCount / 2; i++) {
      uint8_t swap = buffer[i];
      buffer[i] = buffer[wordCount - i];
      buffer[wordCount - i] = swap;
    }
  }
  if ((buffer[0] & 0x01) == 0x00) {
    // we must be negative - flip every bit
    buffer[0] = 1;
    for (size_t i = 1; i < wordCount; i+= 8) {
      flipSign(&buffer[i], false);
    }
    return 0;
  }
  // we're positive - we're gonna flip the sign to 0 and do nothing else
  buffer[0] = 0;
  return 0;
}

int parseFormat(const char* format, std::vector<Format> *formats) {
  int formatLength = strlen(format);
  return parseFormat(format, formatLength, formats);
}

void unpackItemForFormat(
  WT_PACK_STREAM *stream,
  std::vector<char> *formatsPtr,
  std::vector<QueryValue> *values
) {
  for (size_t i = 0; i < formatsPtr->size(); i++) {
    void* valueItem;
    size_t size = -1;
    char format = formatsPtr->at(i);
    if (format == FIELD_STRING) {
      wiredtiger_unpack_str(stream, (const char**)&valueItem);
      size = strlen((char*)valueItem);
    }
    else if (format == FIELD_INT || format == FIELD_LONG) {// TODO all the other formats
      wiredtiger_unpack_int(stream, (int64_t*)&valueItem);
      size = 4;
    }
    else if (format == FIELD_UINT || format == FIELD_ULONG) {
      wiredtiger_unpack_uint(stream, (uint64_t*)&valueItem);
      size = 4;
    }
    else if (format == FIELD_WT_ITEM) {
      WT_ITEM item;
      wiredtiger_unpack_item(stream, &item);
      size = item.size;
      valueItem = (void*)item.data;
    }
    values->push_back(QueryValue {
      format,
      valueItem,
      size
    });
  }
}

int packItem(
  WT_PACK_STREAM *stream,
  QueryValue item
) {
  printf("Packing: %c\n", item.dataType);
  if (item.dataType == FIELD_STRING) {
    printf("Packing: %s\n", (char*)item.value);
    return wiredtiger_pack_str(stream, (char*)item.value);
  }
  else if (item.dataType == FIELD_BYTE || item.dataType == FIELD_HALF || item.dataType == FIELD_INT || item.dataType == FIELD_INT2 || item.dataType == FIELD_LONG) {
    int64_t intVal = *(int64_t*)item.value;
    printf("Packing: %d\n", intVal);
    // HUGE TODO: this code is unusable without casting to the right int size
    return wiredtiger_pack_int(stream, intVal);
  }
  else if (item.dataType == FIELD_BITFIELD || item.dataType == FIELD_UBYTE || item.dataType == FIELD_UHALF || item.dataType == FIELD_UINT || item.dataType == FIELD_UINT2 || item.dataType == FIELD_ULONG) {
    uint64_t intVal = *(uint64_t*)item.value;
    // HUGE TODO: this code is unusable without casting to the right int size
    return wiredtiger_pack_uint(stream, intVal);
  }
  else if (item.dataType == FIELD_WT_ITEM) {
    WT_ITEM wt;
    wt.data = item.value;
    wt.size = item.size;
    return wiredtiger_pack_item(stream, &wt);
  }
  printf("Invalid type\n");
  return INVALID_VALUE_TYPE;
}


int packItems(
  WT_SESSION* session,
  const char* format,
  std::vector<QueryValue>* items,
  void** buffer,
  size_t* bufferSize,
  size_t* usedSize
) {
  WT_PACK_STREAM *stream;
  RETURN_IF_ERROR(wiredtiger_pack_start(session, format, *buffer, *bufferSize, &stream));
  for (size_t i = 0; i < items->size(); i++) {
    int error = packItem(stream, items->at(i));
    if (error == 12) {// insufficient space in buffer
      // unfortunately it does no good to pre-empt this - the bufferSize is fixed at start, we just have to start over.
      // worst case scenario would be ever increasing doc sizes during packing - so we kept having to increase the buffer
      *buffer = growBuffer(*buffer, *bufferSize, *bufferSize);
      *bufferSize *= 2;
      // close the current one.
      RETURN_IF_ERROR(wiredtiger_pack_close(stream, usedSize));
      return packItems(session, format, items, buffer, bufferSize, usedSize);
    }
    RETURN_IF_ERROR(error);
  }
  RETURN_IF_ERROR(wiredtiger_pack_close(stream, usedSize));
  return 0;
}

char* copyCfgValue(WT_CONFIG_ITEM *cfg) {
  char* value = (char*)malloc(cfg->len + 1);
  memcpy(value, cfg->str, cfg->len);
  bzero(value + cfg->len, 1);
  return value;
}

// I'm a bit worried that sometimes a value.value may be an actual pointer and sometimes it may be a value pretending to be a pointer
// on extracting values from node (e.g., table::insertMany) they're always pointers.
void freeExtracted(QueryValue value) {
  if (!value.noFree && value.value != NULL) {
    free(value.value);
  }
}

void freeQueryValues(std::vector<QueryValue>* values) {
  for (size_t a = 0; a < values->size(); a++) {
    freeExtracted(values->at(a));
  }
}

void freeQueryValues(QueryValueOrWT_ITEM* values, int length) {
  for (int i = 0; i < length; i++) {
    freeExtracted(values[i].queryValue);
  }

  free(values);
}

void freeConvertedKVPairs(std::vector<KVPair>* converted) {
  for (size_t i = 0; i < converted->size(); i++) {
    KVPair kv = converted->at(i);
    freeExtracted(kv.key);
    for (size_t a = 0; a < kv.values.size(); a++) {
      freeExtracted(kv.values.at(a));
    }
  }
}

void freeResults(std::vector<EntryOfPointers>* results) {
  for (size_t i = 0; i < results->size(); i++) {
    free(results->at(i).keyArray);
    free(results->at(i).valueArray);
  }
}

void freeConditions(std::vector<QueryCondition>* conditions) {
  for (size_t i = 0; i < conditions->size(); i++) {
    QueryCondition condition = conditions->at(i);
    freeQueryValues(&condition.values);
    freeConditions(&condition.subConditions);
  }
}

int cursorForCondition(
  WT_SESSION* session,
  char* tableCursorURI,
  char* joinCursorURI,
  QueryCondition condition,
  WT_CURSOR** conditionCursorPointer,
  bool inSubJoin,
  std::vector<WT_CURSOR*>* cursors,
  std::vector<void*>* buffers
) {
  if (condition.operation == OPERATION_INDEX) {
    RETURN_IF_ERROR(session->open_cursor(session, condition.index, NULL, "raw", conditionCursorPointer));
    RETURN_IF_ERROR((*conditionCursorPointer)->next(*conditionCursorPointer));
    return 0;
  }
  if (condition.operation == OPERATION_NE) {
    std::vector<QueryCondition> subConditions;
    subConditions.push_back({
      condition.index,
      operation: OPERATION_GT,
      condition.subConditions,
      condition.values
    });
    subConditions.push_back({
      condition.index,
      operation: OPERATION_LT,
      condition.subConditions,
      condition.values
    });
    return cursorForConditions(
      session,
      tableCursorURI,
      joinCursorURI,
      &subConditions,
      conditionCursorPointer,
      true,
      true,
      cursors,
      buffers
    );
  }
  if (condition.values.size()) {
    // TODO: table name
    RETURN_IF_ERROR(session->open_cursor(session, condition.index == NULL ? tableCursorURI : condition.index, NULL, "raw", conditionCursorPointer));
    WT_CURSOR* conditionCursor = *conditionCursorPointer;
    cursors->push_back(conditionCursor);
    size_t bufferSize = 512;
    char* buffer = (char*)malloc(bufferSize); // we can't free the buffer here because it will still be in use. Instead, we'll free it when we release the cursor by looking at the key
    size_t valueSize;
    WT_ITEM item;
    RETURN_IF_ERROR(packItems(
      session,
      conditionCursor->value_format,
      &condition.values,
      (void**)&buffer,
      &bufferSize,
      &valueSize
    ));
    buffers->push_back(buffer);
    // does NOT work - the goal of this was to support total index matching with a partial key,
    // e.g., if you have an index with format SS as ["string1", "string2"] - a search for just  ["string1"] should return no results
    // it doesn't seem possible to do this currently in general - in the case that columns are binary types we can add placeholder [0x0] buffers
    // but this will be handled on the JS side.
    // if (condition.operation == OPERATION_EQ) {
    //   if (valueSize >= bufferSize - 1) {
    //     buffer = (char*)growBuffer(buffer, bufferSize, 1);
    //   }
    //   buffer[valueSize + 1] = '\0';
    //   // buffer[valueSize + 2] = '\0';
    //   valueSize += 1;
    // }
    item.size = valueSize;
    item.data = buffer;
    conditionCursor->set_key(conditionCursor, &item);

    int exact = 0;


    // TODO: toggling this on breaks exact key search, because it'll find "the wrong" one.
    // toggling it off breaks any search where a non exact match is used.
    // Maybe need to look at the operation here and just turn on when not EQ
    if (condition.index != NULL && condition.index[0] == 'i' && (condition.operation == OPERATION_EQ || condition.operation == OPERATION_INDEX)) {
      RETURN_IF_ERROR(conditionCursor->search(conditionCursor));
    }
    else if (condition.operation != OPERATION_EQ) {
      RETURN_IF_ERROR(conditionCursor->search_near(conditionCursor, &exact));
    }
    if (
      // what about GE/LE? Seems these should be the same or similar at least? But adding GE makes text startsWith not work
      ((condition.operation == OPERATION_LT) && exact == -1)
      || ((condition.operation == OPERATION_GT) && exact == 1)
    ) {
      // pretty sure this is wrong. I think we need a next+error check+prev (and the reverse) since the closest key might be before
      // not 100% sure. requires some thought.
      // [a, b) should match a -> a.+. a search_near of b could result in c (if it exists) with an exact of 1
      // but there could still be an a.+ - this would probably be the case if the above key was czzzzzz (or similar).
      // there is nothing "the other side of" this key, so we don't need to add this cursor
      // I can't make this case fail though - it seems that WT always picks "the one after"
      return WT_NOTFOUND;
    }
    return 0;
  }
  if (condition.subConditions.size()) {
    return cursorForConditions(
      session,
      tableCursorURI,
      joinCursorURI,
      &condition.subConditions,
      conditionCursorPointer,
      condition.operation == OPERATION_OR,
      true,
      cursors,
      buffers
    );
  }
  return 0;
}

int cursorForConditions(
  WT_SESSION* session,
  char* tableCursorURI,
  char* joinCursorURI,
  std::vector<QueryCondition> *conditions,
  WT_CURSOR** joinCursorPointer,
  bool disjunction,
  bool inSubJoinAlready,
  std::vector<WT_CURSOR*>* cursors,
  std::vector<void*>* buffers
) {
  if (conditions->size() == 0) {
    RETURN_IF_ERROR(session->open_cursor(session, tableCursorURI, NULL, NULL, joinCursorPointer));
    return 0;
  }
  // else if (conditions->size() == 1 && conditions->at(0).operation == OPERATION_INDEX) {
  // // need to be careful here - you can query an index solo, but by default the id of the table is missing (although the rest of the fields are tehre)
  // // getting the key + all fields from the table would be a chore (manually specifying each column)
  //   return cursorForCondition(session, tableCursorURI, joinCursorURI, conditions->at(0), joinCursorPointer, false, cursors);
  // }
  RETURN_IF_ERROR(session->open_cursor(session, joinCursorURI, NULL, NULL, joinCursorPointer));
  cursors->push_back(*joinCursorPointer);
  WT_CURSOR* currentJoinCursorPointer = *joinCursorPointer;
  int lastOperation = -1;
  bool inSubJoin = inSubJoinAlready;
  for (size_t i = 0; i < conditions->size(); i++) {
    WT_CURSOR* conditionCursor;
    QueryCondition condition = conditions->at(i);

    int error = cursorForCondition(session, tableCursorURI, joinCursorURI, condition, &conditionCursor, inSubJoin, cursors, buffers);
    if (error == WT_NOTFOUND) {
      if (!disjunction) {
        printf("Not joining %s %d\n", conditionCursor->key.data, condition.operation);
        // bit hacky - but we only return WT_NOTFOUND when the cursor wouldn't restrict things, e.g., LT <key where nothing GT that key exists>
        // in the case of AND - this cursor wouldn't reduce the scope further (we need to look at every key) and adding it will exclude the last valid item
        continue;
      }
    }
    else if (error != 0) {
      return error;
    }
    char config[100];
    bzero(config, 100);
    if (
      condition.operation == OPERATION_EQ
      || condition.operation == OPERATION_NE
      || condition.operation == OPERATION_AND
      || condition.operation == OPERATION_OR
    ) {
      // while this looks wrong, it's correct - if the operation is NE,
      // cursorForCondition would have returned a join cursor on the exclusion (or: gt, lt)
      strcpy(config, "compare=eq");
    }
    else if (condition.operation == OPERATION_LE) {
      strcpy(config, "compare=le");
    }
    else if (condition.operation == OPERATION_LT) {
      strcpy(config, "compare=lt");
    }
    else if (condition.operation == OPERATION_GE || condition.operation == OPERATION_INDEX) {
      // TODO: LT+reverse?
      // bit weird, but an index operation is always a GE
      strcpy(config, "compare=ge");
    }
    else if (condition.operation == OPERATION_GT) {
      strcpy(config, "compare=gt");
    }
    if (disjunction && lastOperation != -1 && lastOperation != condition.operation) {
      // for the conditions "LT or GT" (e.g., NE) we need to push into a join eg "'join LT' or 'join GT'"
      // but for EQ we want to keep everything together. (nested joins leads to duplicates)
      // "LT and GT" is supported by WT

      lastOperation = -1;
      i--; // rerun the last operation
      WT_CURSOR* newJoinCursor;
      RETURN_IF_ERROR(session->open_cursor(session, joinCursorURI, NULL, "raw", &newJoinCursor)); // TODO: table name
      newJoinCursor->key.data = NULL;
      RETURN_IF_ERROR(session->join(session, *joinCursorPointer, newJoinCursor, disjunction ? "operation=or" : NULL));
      currentJoinCursorPointer = newJoinCursor;
      cursors->push_back(newJoinCursor);
      inSubJoin = true;
      continue;
    }
    lastOperation = condition.operation;
    if (disjunction) {
      strcat(config, ",");
      strcat(config, "operation=or");
    }
    RETURN_IF_ERROR(session->join(session, currentJoinCursorPointer, conditionCursor, config));
  }

  return 0;
}
}