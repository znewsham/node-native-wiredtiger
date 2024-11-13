#include <wiredtiger.h>
#include "find_cursor.h"
#include "helpers.h"
#include "../helpers.h"


#include <cstring>
#include <vector>
#include <bitset>

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
        // we use this in two places
        // 1. for passing values into an update (e.g., to say "don't update this")
        // 2. for specifying which values need handling with the custom extractor
        // for the first, we don't care about adding a value to the formats list, but for the second we do
        // because it allows us to not parse the table format to get the column count.

        formats->push_back({ c, size });
        size = 0;
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
  size_t bytes,
  bool positive // the positive condition changes depending on whether we're going out or in. Going out the MSB will be 1
) {
  if (positive) {
    valueBuffer[0] = valueBuffer[0] ^ 0x80;
  }
  else {
    for (size_t i = 0; i < bytes; i++) {
      valueBuffer[i] = valueBuffer[i] ^ 0xff;
    }
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
  bool positive = (buffer[0] & 0x80) == 0x00;
  flipSign(buffer, 8, positive);

  return 0;
}


// USE EXCESSIVE CARE WITH THESE TWO - mongo (seems?) converts to LE, but we use BE because https://stackoverflow.com/questions/43299299/sorting-floating-point-values-using-their-byte-representation
int byteArrayToDouble(
  uint8_t* valueBuffer,
  double* value
) {
  uint8_t* buffer = (uint8_t*)value;
  bool positive = (valueBuffer[0] & 0x80) == 0x80;
  flipSign(valueBuffer, 8, positive);

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

// 8 bytes
int makeDoubleArraySortable(uint8_t* buffer) {
  if (!isBigEndian) {
    for (size_t i = 0; i <= 4; i++) {
      uint8_t swap = buffer[i];
      buffer[i] = buffer[4 - i];
      buffer[4 - i] = swap;
    }
  }

  if ((buffer[0] & 0x80) == 0x00) {
    // we're positive - we're gonna flip the sign to 1 and do nothing else
    buffer[0] = buffer[0] | 0x80;
    return 0;
  }
  // we must be negative - flip every bit
  flipSign(buffer, 8, false);
}

int makeBigIntByteArraySortable(size_t byteCount, uint8_t* buffer) {
  if (!isBigEndian) {
    for (size_t i = 1; i <= byteCount / 2; i++) {
      uint8_t swap = buffer[i];
      buffer[i] = buffer[byteCount - i];
      buffer[byteCount - i] = swap;
    }
  }
  if ((buffer[0] & 0x01) == 0x00) {
    // we're positive - we're gonna flip the sign to 1 and do nothing else
    buffer[0] = 1;
    return 0;
  }
  // we must be negative - flip every bit
  flipSign(buffer, byteCount, false);
  // flipSign assumes 0x80 = negative and so flips too much in byte 1
  buffer[0] = 0;
  return 0;
}

int unmakeBigIntByteArraySortable(size_t bytes, uint8_t* buffer) {
  if (!isBigEndian) {
    for (size_t i = 1; i <= bytes / 2; i++) {
      uint8_t swap = buffer[i];
      buffer[i] = buffer[bytes - i];
      buffer[bytes - i] = swap;
    }
  }
  if ((buffer[0] & 0x01) == 0x00) {
    // we must be negative - flip every bit
    flipSign(buffer, bytes, false);
    buffer[0] = 1;
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
int formatsToString(std::vector<Format> *formats, char** format) {
  // TODO: dynamic format;
  int size = 1;
  for(size_t i = 0; i < formats->size(); i++) {
    int formatSize = formats->at(0).size;
    size += 1;
    if (formatSize >= 10000) {
      size += 10;// fuck it - if we've got > 10 digits of size info we're in trouble
    }
    else if (formatSize >= 1000) {
      size += 4;
    }
    else if (formatSize >= 100) {
      size += 3;
    }
    else if (formatSize >= 10) {
      size += 2;
    }
    else if (formatSize >= 1) {
      size += 1;
    }

  }
  *format = (char*)calloc(sizeof(char), size);
  char* formatPtr = *format;
  for(size_t i = 0; i < formats->size(); i++) {
    Format format = formats->at(i);
    char formatChar = format.format;
    if (FieldIsWTItem(formatChar)) {
      formatChar = 'u';
    }
    if (format.size) {
      formatPtr += sprintf(formatPtr, "%d%c", format.size, formatChar);
    }
    else {
      formatPtr += sprintf(formatPtr, "%c", formatChar);
    }
  }
  return 0;
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
      valueItem,
      size,
      format,
      false
    });
  }
}

/**
 * @deprecated see comments regarding int sizees
 */
int packItem(
  WT_PACK_STREAM *stream,
  QueryValue* item
) {
  if (item->dataType == FIELD_STRING) {
    return wiredtiger_pack_str(stream, (char*)item->value.valuePtr);
  }
  else if (item->dataType == FIELD_BYTE || item->dataType == FIELD_HALF || item->dataType == FIELD_INT || item->dataType == FIELD_INT2 || item->dataType == FIELD_LONG) {
    int64_t intVal = (int64_t)item->value.valueUint;
    // HUGE TODO: this code is unusable without casting to the right int size
    return wiredtiger_pack_int(stream, intVal);
  }
  else if (item->dataType == FIELD_BITFIELD || item->dataType == FIELD_UBYTE || item->dataType == FIELD_UHALF || item->dataType == FIELD_UINT || item->dataType == FIELD_UINT2 || item->dataType == FIELD_ULONG) {
    uint64_t intVal = item->value.valueUint;
    // HUGE TODO: this code is unusable without casting to the right int size
    return wiredtiger_pack_uint(stream, intVal);
  }
  else if (item->dataType == FIELD_WT_ITEM) {
    WT_ITEM wt;
    wt.data = item->value.valuePtr;
    wt.size = item->size;
    return wiredtiger_pack_item(stream, &wt);
  }
  printf("Invalid type\n");
  return INVALID_VALUE_TYPE;
}


/**
 * @deprecated see comments regarding int sizees
 */
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
    int error = packItem(stream, &(*items)[i]);
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
  if (cfg->type < 0 || cfg->type > WT_CONFIG_ITEM_TYPE::WT_CONFIG_ITEM_STRUCT || cfg->len == 0 || cfg->str == NULL) {
    return NULL;
  }
  char* value = (char*)malloc(cfg->len + 1);
  memcpy(value, cfg->str, cfg->len);
  bzero(value + cfg->len, 1);
  return value;
}



void populateAvListForPackingOrUnPacking(
  std::vector<QueryValue>* valueArray,
  std::vector<WT_ITEM>* wtItems,
  av_alist* argList,
  size_t i,
  Format format,
  bool forWrite
) {
  if (forWrite && format.format == FIELD_PADDING) {
    // generally we shouldn't call it with a padding byte
    // this might bite us in the ass when implementing unique indexes and we need to pass in a null char to this
    return;
  }
  if (FieldIsWTItem(format.format) || format.format == FIELD_PADDING) {
    // when writing, valueArray should be considered const and not modified
    // setting valueArray.wtItem.* relies on the shape of wtItem and queryValue being identical. Gross.
    // for reading - there is no such requirement and we dont want the cost of allocating a new WT_ITEM vector
    if (forWrite) {
      (*wtItems)[i].data = (*valueArray)[i].value.valuePtr;
      (*wtItems)[i].size = (*valueArray)[i].size;
      av_ptr(*argList, WT_ITEM*, &(*wtItems)[i]);
    }
    else {
      // we fake that a QueryValue is a WT_ITEM (they're at least the same size)
      // in populate we'll pull the data out and rebuild this item correctly
      av_ptr(*argList, WT_ITEM*, &(*valueArray)[i]);
      (*valueArray)[i].dataType = format.format;
    }
  }
  else {
    if (forWrite) {
      if (FieldIsPtr(format.format)) {
        av_ptr(*argList, void*, (*valueArray)[i].value.valuePtr);
      }
      else {
        av_uint(*argList, (*valueArray)[i].value.valueUint);
      }
    }
    else {
      if (FieldIsPtr(format.format)) {
        av_ptr(*argList, void*, &(*valueArray)[i].value.valuePtr);
      }
      else {
        av_ptr(*argList, void*, &(*valueArray)[i].value.valueUint);
      }
      (*valueArray)[i].dataType = format.format;
    }
  }
}


int cursorForCondition(
  WT_SESSION* session,
  char* tableCursorURI,
  char* joinCursorURI,
  QueryCondition& condition,
  WT_CURSOR** conditionCursorPointer,
  bool inSubJoin,
  std::vector<WT_CURSOR*>* cursors,
  std::vector<void*>* buffers
) {
  if (condition.operation == OPERATION_INDEX) {
    RETURN_IF_ERROR(session->open_cursor(session, condition.index, NULL, "raw", conditionCursorPointer));
    // TODO: Why are we calling next here?
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
  if (condition.values != NULL && condition.values->size()) {
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
      condition.values,
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
  if (condition.subConditions != NULL && condition.subConditions->size()) {
    return cursorForConditions(
      session,
      tableCursorURI,
      joinCursorURI,
      condition.subConditions,
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
    QueryCondition& condition = conditions->at(i);

    int error = cursorForCondition(
      session,
      tableCursorURI,
      joinCursorURI,
      condition,
      &conditionCursor,
      inSubJoin,
      cursors,
      buffers
    );
    if (error == WT_NOTFOUND) {
      if (!disjunction) {
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
