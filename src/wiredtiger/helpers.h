#include <wiredtiger.h>
#include <avcall.h>
#include "./types.h"

#include <assert.h>

#include <vector>
#include <iostream>

using namespace std;

#ifndef WIREDTIGER_HELPERS_H
#define WIREDTIGER_HELPERS_H
namespace wiredtiger {
void* growBuffer(void* existing, size_t existingLength, size_t additionalLength);
int parseFormat(const char* format, int formatLength, std::vector<Format> *formats);
int parseFormat(const char* format, std::vector<Format> *formats);
int formatsToString(std::vector<Format> *formats, char** format);
void unpackItemForFormat(
  WT_PACK_STREAM *stream,
  std::vector<char> *formatsPtr,
  std::vector<QueryValue> *values
);

void populateAvListForPackingOrUnPacking(
  std::vector<QueryValue>* valueArray,
  std::vector<WT_ITEM>* wtItems,
  av_alist* argList,
  size_t i,
  Format format,
  bool forWrite
);

int makeDoubleArraySortable(uint8_t* buffer);

int makeBigIntByteArraySortable(
  size_t wordCount,
  uint8_t* buffer
);

int unmakeBigIntByteArraySortable(
  size_t wordCount,
  uint8_t* buffer
);


int doubleToByteArray(
  double value,
  uint8_t* buffer
);

int byteArrayToDouble(
  uint8_t* valueBuffer,
  double* value
);

int packItem(
  WT_PACK_STREAM *stream,
  QueryValue item
);
int packItems(
  WT_SESSION* session,
  const char* format,
  std::vector<QueryValue>* items,
  void** buffer,
  size_t* bufferSize,
  size_t* usedSize
);
char* copyCfgValue(WT_CONFIG_ITEM *cfg);
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
);
}
#endif
