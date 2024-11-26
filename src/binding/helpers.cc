#include <node.h>
#include <node_version.h>
#include <node_buffer.h>
#include <nan.h>
#include "helpers.h"
#include "../wiredtiger/find_cursor.h"
#include "../wiredtiger/helpers.h"
#include "../helpers.h"

#include <assert.h>

#include <vector>
#include <iostream>

using namespace std;
using namespace v8;
using namespace node;
using namespace wiredtiger;

namespace wiredtiger::binding {



  void ThrowExtractError(int result, const FunctionCallbackInfo<Value>& args) {
    if (result == CONDITION_NOT_OBJECT) {
      THROW(Exception::TypeError, "Invalid Condition (not an object)");
    }
    else if (result == INDEX_NOT_STRING) {
      THROW(Exception::TypeError, "Invalid Condition (index not a string)");
    }
    else if (result == OPERATION_NOT_INTEGER) {
      THROW(Exception::TypeError, "Invalid Condition (operation not an integer)");
    }
    else if (result == VALUES_NOT_ARRAY) {
      THROW(Exception::TypeError, "Invalid Condition (values not an array)");
    }
    else if (result == CONDITIONS_NOT_ARRAY) {
      THROW(Exception::TypeError, "Invalid Condition (conditions not an array)");
    }
    else if (result == INVALID_VALUE_TYPE) {
      THROW(Exception::TypeError, "Invalid Condition (invalid value type)");
    }
    else if (result == VALUES_AND_CONDITIONS) {
      THROW(Exception::TypeError, "Invalid Condition (can't specify sub conditions and values)");
    }
    else if (result == INVALID_OPERATOR) {
      THROW(Exception::TypeError, "Invalid Condition (AND/OR mismatch with values/conditions)");
    }
    else if (result == NO_VALUES_OR_CONDITIONS) {
      THROW(Exception::TypeError, "Invalid Condition (missing values or conditions)");
    }
    else if (result == EMPTY_CONDITIONS) {
      THROW(Exception::TypeError, "Invalid Condition (empty conditions)");
    }
    else if (result == EMPTY_VALUES) {
      THROW(Exception::TypeError, "Invalid Condition (empty values)");
    }
    else {
      THROW(Exception::TypeError, "Unknown error");
    }
  }

  Local<String> NewLatin1String(Isolate* isolate, const char* string, int length) {
    return String::NewFromOneByte(isolate, (const uint8_t*)string, NewStringType::kNormal, length).ToLocalChecked();
  }

  Local<String> NewLatin1String(Isolate* isolate, const char* string) {
    return String::NewFromOneByte(isolate, (const uint8_t*)string, NewStringType::kNormal).ToLocalChecked();
  }

  Local<Value> ThrowException(Isolate* isolate, Local<Value> exception) {
    return isolate->ThrowException(exception);
  }
  Local<FunctionTemplate> NewFunctionTemplate(
    Isolate* isolate,
    FunctionCallback callback,
    Local<Value> data,
    Local<Signature> signature,
    int length
  ) {
    return FunctionTemplate::New(isolate, callback, data, signature, length);
  }


Local<Signature> NewSignature(
  Isolate* isolate,
  Local<FunctionTemplate> receiver
) {
  return Signature::New(isolate, receiver);
}



void StringToCharPointer(Isolate* isolate, Local<String> string, char* where) {
  string->WriteUtf8(isolate, where, string->Utf8Length(isolate));
}

char* ArgToCharPointer (Isolate* isolate, Local<Value> value) {
  Local<String> string = Local<String>::Cast(value);
  size_t length = string->Utf8Length(isolate) + 1;
  char* where = (char*)calloc(sizeof(char), length); // freed wherever this method is used
  StringToCharPointer(isolate, string, where);
  return where;
}

// TODO: this won't cover fixed length items, e.g., 's', 't' and 'u'
int populateArrayItem(
  Isolate* isolate,
  Local<Array> items,
  int v,
  QueryValueValue value,
  char format,
  size_t size
) {
  if (format == FIELD_PADDING) {
    items->Set(
      isolate->GetCurrentContext(),
      v,
      Null(isolate)
    ).Check();
  }
  else if (format == FIELD_STRING) {
    if (size != 0) {
      items->Set(
        isolate->GetCurrentContext(),
        v,
        NewLatin1String(isolate, (char*)value.valuePtr, min(strlen((char*)value.valuePtr), size))
      ).Check();
    }
    else {
      items->Set(
        isolate->GetCurrentContext(),
        v,
        NewLatin1String(isolate, (char*)value.valuePtr)
      ).Check();
    }
  }
  else if (format == FIELD_CHAR_ARRAY) {
    items->Set(
      isolate->GetCurrentContext(),
      v,
      NewLatin1String(isolate, (char*)value.valuePtr, min(strlen((char*)value.valuePtr), size))
    ).Check();
  }
  else if (format == FIELD_INT || format == FIELD_INT2) {
    items->Set(
      isolate->GetCurrentContext(),
      v,
      Int32::New(isolate, (int32_t)value.valueUint)
    ).Check();
  }
  else if (format == FIELD_UINT || format == FIELD_UINT2) {
    items->Set(
      isolate->GetCurrentContext(),
      v,
      Uint32::New(isolate, (uint32_t)value.valueUint)
    ).Check();
  }
  else if (format == FIELD_HALF) {
    items->Set(
      isolate->GetCurrentContext(),
      v,
      Uint32::New(isolate, (uint16_t)value.valueUint)
    ).Check();
  }
  else if (format == FIELD_UHALF) {
    items->Set(
      isolate->GetCurrentContext(),
      v,
      Uint32::New(isolate, (int16_t)value.valueUint)
    ).Check();
  }
  else if (format == FIELD_UBYTE) {
    items->Set(
      isolate->GetCurrentContext(),
      v,
      Uint32::New(isolate, (uint8_t)value.valueUint)
    ).Check();
  }
  else if (format == FIELD_BYTE) {
    items->Set(
      isolate->GetCurrentContext(),
      v,
      Uint32::New(isolate, (int8_t)value.valueUint)
    ).Check();
  }
  else if (format == FIELD_ULONG || format == FIELD_RECID) {
    items->Set(
      isolate->GetCurrentContext(),
      v,
      BigInt::New(isolate, value.valueUint)
    ).Check();
  }
  else if (format == FIELD_LONG) {
    items->Set(
      isolate->GetCurrentContext(),
      v,
      BigInt::New(isolate, (int64_t)value.valueUint)
    ).Check();
  }
  else if (format == FIELD_WT_ITEM || format == FIELD_WT_UITEM) {
    items->Set(
      isolate->GetCurrentContext(),
      v,
      Nan::CopyBuffer((const char*)value.valuePtr, size).ToLocalChecked()
    ).Check();
  }
  else if (format == FIELD_WT_ITEM_DOUBLE) {
    double doubleValue;
    uint8_t* writableBytes = (uint8_t*)calloc(sizeof(uint8_t), size);
    memcpy(writableBytes, value.valuePtr, size);
    wiredtiger::byteArrayToDouble(writableBytes, &doubleValue);
    items->Set(
      isolate->GetCurrentContext(),
      v,
      Number::New(isolate, doubleValue)
    ).Check();
    free(writableBytes);
  }
  else if (format == FIELD_WT_ITEM_BIGINT) {
    uint8_t* writableBytes = (uint8_t*)calloc(sizeof(uint8_t), size);
    memcpy(writableBytes, value.valuePtr, size);
    wiredtiger::unmakeBigIntByteArraySortable(size, writableBytes);
    items->Set(
      isolate->GetCurrentContext(),
      v,
      BigInt::NewFromWords(isolate->GetCurrentContext(), *writableBytes, (size - 1) / 8, (uint64_t*)(writableBytes + 1)).ToLocalChecked()
    ).Check();
    free(writableBytes);
  }
  else if (format == FIELD_BITFIELD) {
    items->Set(
      isolate->GetCurrentContext(),
      v,
      Uint32::New(isolate, ((uint64_t)value.valueUint))
    ).Check();
  }
  else if (format == FIELD_BOOLEAN) {
    items->Set(
      isolate->GetCurrentContext(),
      v,
      Boolean::New(isolate, ((uint64_t)value.valueUint) == 1)
    ).Check();
  }
  else {
    printf("Couldn't find %c\n", format);
    return INVALID_VALUE_TYPE;
  }
  return 0;
}

int populateArray(
  Isolate* isolate,
  Local<Array> items,
  QueryValue* values,
  size_t size
) {
  for (size_t v = 0; v < size; v++) {
    QueryValue qv = values[v];
    RETURN_IF_ERROR(populateArrayItem(isolate, items, v, qv.value, qv.dataType, qv.size));
  }
  return 0;
}

int populateArray(
  Isolate* isolate,
  Local<Array> items,
  std::vector<QueryValue>* values
) {
  for (size_t v = 0; v < values->size(); v++) {
    QueryValue& qv = (*values)[v];
    RETURN_IF_ERROR(populateArrayItem(isolate, items, v, qv.value, qv.dataType, qv.size));
  }
  return 0;
}

#define SET_VALUE_SIZE_FORMAT_AND_RETURN() {converted->value = value; converted->size = size; converted->dataType = format.format; return 0;}
#define SET_VALUE_PTR_SIZE_FORMAT_AND_RETURN() {converted->value = *valueOut; converted->size = size; converted->dataType = format.format; return 0;}


template <typename b, typename h, typename i, typename LocalInt>
int extractNumber(
  Local<Value> valueIn,
  Format format,
  QueryValue* converted
) {
  if (format.format == FIELD_INT || format.format == FIELD_INT2 || format.format == FIELD_UINT || format.format == FIELD_UINT2) {
    converted->size = sizeof(i);
    converted->value.valueUint = (uint64_t)(i)(Local<LocalInt>::Cast(valueIn))->Value();
    return 0;
  }
  else if (format.format == FIELD_HALF || format.format == FIELD_UHALF) {
    converted->size = sizeof(h);
    converted->value.valueUint = (uint64_t)(h)(Local<LocalInt>::Cast(valueIn))->Value();
    return 0;
  }
  else if (format.format == FIELD_BYTE || format.format == FIELD_UBYTE) {
    converted->size = sizeof(b);
    converted->value.valueUint = (uint64_t)(b)(Local<LocalInt>::Cast(valueIn))->Value();
    return 0;
  }
  return INVALID_VALUE_TYPE;
}
int extractValue(
  Local<Value> val,
  Isolate* isolate,
  QueryValue* converted,
  Format format
) {
  QueryValueValue value;
  value.valuePtr = NULL;
  converted->dataType = format.format;
  size_t size = -1;
  if (val->IsUndefined()) {
    // TODO: we probably want some way to indicate that we don't want a value, e.g., a default value (0, null ptr, whatever)
    // for now, we use this only in the update to indicate we're not changing this column
    converted->dataType = FIELD_PADDING;
    converted->value = value;
    return 0;
  }
  switch(format.format) {
    case FIELD_BITFIELD:
      if (val->IsInt32()) {
        size = sizeof(uint8_t);
        // value = malloc(size);// freed on cursor close/insert completion
        value.valueUint = (uint8_t)(Local<Int32>::Cast(val))->Value();
        SET_VALUE_SIZE_FORMAT_AND_RETURN();
      }
      if (val->IsUint32()) {
        size = sizeof(uint8_t);
        // value = malloc(size);// freed on cursor close/insert completion
        value.valueUint = (uint8_t)(Local<Uint32>::Cast(val))->Value();
        SET_VALUE_SIZE_FORMAT_AND_RETURN();
      }
      if (val->IsBoolean()) {
        size = sizeof(uint8_t);
        value.valueUint = (uint8_t)val->BooleanValue(isolate);
        SET_VALUE_SIZE_FORMAT_AND_RETURN();
      }
      return INVALID_VALUE_TYPE;

    case FIELD_RECID:
    case FIELD_ULONG:
      if (val->IsInt32()) {
        size = sizeof(uint64_t);
        value.valueUint = (uint64_t)(Local<Int32>::Cast(val))->Value();
        SET_VALUE_SIZE_FORMAT_AND_RETURN();
      }
      if (val->IsUint32()) {
        size = sizeof(uint64_t);
        value.valueUint = (uint64_t)(Local<Uint32>::Cast(val))->Value();
        SET_VALUE_SIZE_FORMAT_AND_RETURN();
      }
      if (val->IsBigInt()) {
        int sign_bit;
        int word_count = 1;
        uint64_t* bytes = (uint64_t*)calloc(sizeof(uint64_t), 1);
        (Local<BigInt>::Cast(val))->ToWordsArray(&sign_bit, &word_count, bytes);
        value.valueUint = *bytes;
        free(bytes);
        size = sizeof(uint64_t);
        SET_VALUE_SIZE_FORMAT_AND_RETURN();
      }
      return INVALID_VALUE_TYPE;
    case FIELD_LONG:
      if (val->IsInt32()) {
        size = sizeof(int64_t);
        value.valueUint = (int64_t)(Local<Int32>::Cast(val))->Value();

        SET_VALUE_SIZE_FORMAT_AND_RETURN();
      }
      if (val->IsUint32()) {
        size = sizeof(int64_t);
        value.valueUint = (int64_t)(Local<Uint32>::Cast(val))->Value();
        SET_VALUE_SIZE_FORMAT_AND_RETURN();
      }
      if (val->IsBigInt()) {
        int sign_bit;
        int word_count = 1;
        int64_t* longValue = (int64_t*)calloc(sizeof(int64_t), 1);
        (Local<BigInt>::Cast(val))->ToWordsArray(&sign_bit, &word_count, (uint64_t*)longValue);
        if (sign_bit == 1) {
          *longValue = *longValue | 0x8000000000000000;
        }
        else {
          *longValue = *longValue | 0x7fffffffffffffff;
        }
        value.valueUint = *longValue;
        free(longValue);
        size = sizeof(int64_t);
        SET_VALUE_SIZE_FORMAT_AND_RETURN();
      }
      return INVALID_VALUE_TYPE;
    case FIELD_WT_UITEM:
    case FIELD_WT_ITEM:
      if (val->IsBigInt()) {
        int sign_bit;
        Local<BigInt> BI = Local<BigInt>::Cast(val);
        int word_count = BI->WordCount();
        uint8_t* bytes;
        bytes = (uint8_t*)calloc(sizeof(uint64_t), word_count + 1);
        (Local<BigInt>::Cast(val))->ToWordsArray(&sign_bit, &word_count, (uint64_t*)(bytes + 1));
        *bytes = sign_bit;
        value.valuePtr = (uint8_t*)bytes;
        size = (word_count * 8) + 1;
        // doing the flip here is totally acceptable - since this is write, we own the bytes.
        wiredtiger::makeBigIntByteArraySortable(size, bytes);
        SET_VALUE_SIZE_FORMAT_AND_RETURN();
      }
      if (val->IsNumber()) {
        uint8_t* buffer = (uint8_t*)calloc(sizeof(uint8_t), 8);
        wiredtiger::doubleToByteArray(Local<Number>::Cast(val)->Value(), buffer);
        value.valuePtr = (void*)buffer;
        size = 8;
        SET_VALUE_SIZE_FORMAT_AND_RETURN();
      }
      if (node::Buffer::HasInstance(val)) {
        value.valuePtr = (void*)node::Buffer::Data(val);
        size = node::Buffer::Length(val);
        converted->noFree = true;
        SET_VALUE_SIZE_FORMAT_AND_RETURN();
      }
      return INVALID_VALUE_TYPE;

    case FIELD_STRING:
    case FIELD_CHAR_ARRAY:
      if (val->IsString()) {
        Local<String> string = Local<String>::Cast(val);
        int length = string->Length();
        value.valuePtr = calloc(sizeof(char), length + 1);// freed on cursor close/insert completion
        size = length;
        StringToCharPointer(isolate, string, (char*)value.valuePtr);
        SET_VALUE_SIZE_FORMAT_AND_RETURN();
      }
      return INVALID_VALUE_TYPE;

    case FIELD_BYTE:
    case FIELD_HALF:
    case FIELD_INT:
    case FIELD_INT2:
      if (!val->IsInt32() && !val->IsUint32()) {
        return INVALID_VALUE_TYPE;
      }
      if (val->IsInt32()) {
        return extractNumber<int8_t, int16_t, int32_t, Int32>(val, format, converted);
      }
      if (val->IsUint32()) {
        return extractNumber<int8_t, int16_t, int32_t, Uint32>(val, format, converted);
      }
      return INVALID_VALUE_TYPE;
    case FIELD_UBYTE:
    case FIELD_UHALF:
    case FIELD_UINT:
    case FIELD_UINT2:
      if (!val->IsInt32() && !val->IsUint32()) {
        return INVALID_VALUE_TYPE;
      }
      if (val->IsInt32()) {
        return extractNumber<uint8_t, uint16_t, uint32_t, Int32>(val, format, converted);
      }
      if (val->IsUint32()) {
        return extractNumber<uint8_t, uint16_t, uint32_t, Uint32>(val, format, converted);
      }
      return INVALID_VALUE_TYPE;
    default:
      return INVALID_VALUE_TYPE;
  }
  return INVALID_VALUE_TYPE;
}

int extractValues(
  Local<Array> values,
  Isolate* isolate,
  Local<Context> context,
  std::vector<QueryValue> *convertedValues,
  std::vector<Format>* formats
) {
  for (uint32_t a = 0; a < values->Length(); a++) {
    Local<Value> val = values->Get(context, a).ToLocalChecked();
    RETURN_IF_ERROR(extractValue(val, isolate, &(*convertedValues)[a], formats->at(a)));
  }
  return 0;
}

int parseConditions(
  Isolate* isolate,
  Local<Context> context,
  Local<Array> *conditionSpecsPointer,
  std::vector<QueryCondition> *conditions,
  std::vector<Format>* formats
) {
  Local<Array> conditionSpecs = *conditionSpecsPointer;
  for (uint32_t i = 0; i < conditionSpecs->Length(); i++) {
    Local<Value> object = conditionSpecs->Get(context, i).ToLocalChecked();
    Local<String> indexProp = NewLatin1String(isolate, "index");
    Local<String> valuesProp = NewLatin1String(isolate, "values");
    Local<String> operationProp = NewLatin1String(isolate, "operation");
    Local<String> conditionsProp = NewLatin1String(isolate, "conditions");

    if (!object->IsObject()) {
      return CONDITION_NOT_OBJECT;
    }
    Local<Object> conditionSpec = Local<Object>::Cast(object);
    char* index = NULL;
    if (conditionSpec->Has(context, indexProp).ToChecked()) {
      Local<Value> val = conditionSpec->Get(context, indexProp).ToLocalChecked();
      if (!val->IsString()) {
        return INDEX_NOT_STRING;
      }
      Local<String> indexString = Local<String>::Cast(val);
      int length = indexString->Utf8Length(isolate);
      // TODO free (also pass it into the FindCursor)
      index = (char*)malloc(length + 1);
      bzero(index, length + 1);
      StringToCharPointer(isolate, indexString, index);
    }
    u_char operation = OPERATION_EQ;
    if (conditionSpec->Has(context, operationProp).ToChecked()) {
      Local<Value> val = conditionSpec->Get(context, operationProp).ToLocalChecked();
      if (!val->IsNumber()) {
        return OPERATION_NOT_INTEGER;
      }
      Local<Integer> operationInteger = Local<Integer>::Cast(val);
      operation = operationInteger->Value();
    }

    std::vector<QueryValue>* conditionValues = NULL;
    std::vector<QueryCondition>* subConditions = NULL;
    bool hasConditions = false;
    bool hasValues = false;
    if (conditionSpec->Has(context, conditionsProp).ToChecked()) {
      if (operation != OPERATION_AND && operation != OPERATION_OR) {
        return INVALID_OPERATOR;
      }
      hasConditions = true;
      Local<Value> subConditionsVal = conditionSpec->Get(context, conditionsProp).ToLocalChecked();
      if (!subConditionsVal->IsArray()) {
        return CONDITIONS_NOT_ARRAY;
      }
      Local<Array> subConditionsArr = Local<Array>::Cast(subConditionsVal);
      if (subConditionsArr->Length() == 0) {
        return EMPTY_CONDITIONS;
      }
      subConditions = new std::vector<QueryCondition>(subConditionsArr->Length()); // deleted with the root conditions vector
      int ret = parseConditions(
        isolate,
        context,
        &subConditionsArr,
        subConditions,
        formats
      );
      if (ret != 0) {
        return ret;
      }
    }
    if (conditionSpec->Has(context, valuesProp).ToChecked()) {
      if (operation == OPERATION_AND || operation == OPERATION_OR) {
        return INVALID_OPERATOR;
      }
      if (hasConditions) {
        return VALUES_AND_CONDITIONS;
      }
      Local<Value> valuesVal = conditionSpec->Get(context, valuesProp).ToLocalChecked();
      if (!valuesVal->IsArray()) {
        return VALUES_NOT_ARRAY;
      }
      Local<Array> values = Local<Array>::Cast(valuesVal);
      conditionValues = new std::vector<QueryValue>(values->Length()); // deleted with the root conditions vector
      if (values->Length() == 0) {
        return EMPTY_VALUES;
      }
      hasValues = true;
      conditionValues->reserve(values->Length());
      RETURN_IF_ERROR(extractValues(
        values,
        isolate,
        context,
        conditionValues,
        formats
      ));
    }
    if (!hasValues && !hasConditions && operation != OPERATION_INDEX) {
      return NO_VALUES_OR_CONDITIONS;
    }
    (*conditions)[i].index = index;
    (*conditions)[i].operation = operation;
    (*conditions)[i].subConditions = subConditions;
    (*conditions)[i].values = conditionValues;
  }
  return 0;
}
}
