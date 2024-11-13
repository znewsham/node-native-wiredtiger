#include "multi_key_extractor.h"
#include "helpers.h"
#include "wt_item.h"
#include <avcall.h>
#include <memory>
#include <map>

namespace wiredtiger {
  static map<string, MultiKeyExtractor*> multiKeyExtractors;

  int MultiKeyExtractor::extractColumnConfiguration(
    WT_SESSION *session,
    WT_CONFIG_ITEM *columnCfg
  ) {
    char* cfg = (char*)calloc(columnCfg->len - 1, 1);
    memcpy(cfg, columnCfg->str + 1, columnCfg->len - 2);
    WT_CONFIG_PARSER* config_parser;
    WT_CONFIG_ITEM columnsCfg;
    WT_CONFIG_ITEM extractorCfg;
    WT_CONFIG_ITEM ngramsCfg;

    RETURN_AND_FREE_IF_ERROR(wiredtiger_config_parser_open(
      NULL,
      cfg,
      columnCfg->len - 2,
      &config_parser
    ), cfg);


    config_parser->get(config_parser, "columns", &columnsCfg);
    char* columnArrayStr = copyCfgValue(&columnsCfg);
    config_parser->get(config_parser, "extractor", &extractorCfg);
    char* extractorStr = copyCfgValue(&extractorCfg);
    config_parser->get(config_parser, "ngrams", &ngramsCfg);
    char* ngramStr = copyCfgValue(&ngramsCfg);
    free(cfg);

    MultiKeyExtractorColumnConfig columnConfig;
    if (ngramStr != NULL) {
      columnConfig.ngrams = strtol(ngramStr, NULL, 10);
    }
    else {
      columnConfig.ngrams = 0;
    }
    if (extractorStr != NULL && strcmp(extractorStr, "ngrams") == 0) {
      columnConfig.format = {'S', 0};
      columnConfig.extractorType = ExtractorType::NGRAM;
    }
    else if (extractorStr != NULL && strcmp(extractorStr, "words") == 0) {
      columnConfig.format = {'S', 0};
      columnConfig.extractorType = ExtractorType::WORDS;
    }
    char* columnArrayStrStart = columnArrayStr+1;
    columnArrayStr[columnsCfg.len - 1] = 0;

    char* token;

    while ((token = strtok_r(columnArrayStrStart, ",", &columnArrayStrStart)) != NULL) {
      columnConfig.columns.push_back((size_t)strtol(token, NULL, 10) - 1);
    }

    columnConfigs->push_back(columnConfig);

    free(columnArrayStr);
    free(extractorStr);
    if (ngramStr != NULL) {
      free(ngramStr);
    }
    return 0;
  }


  int MultiKeyExtractor::extractColumnsConfiguration(
    WT_SESSION *session,
    WT_CONFIG_PARSER* config_parser
  ) {
    WT_CONFIG_ITEM columnCountCfg;

    config_parser->get(config_parser, "columns", &columnCountCfg);
    char* columnCountStr = copyCfgValue(&columnCountCfg);
    if (columnCountStr == NULL) {
      return 0;
    }
    size_t columnCount = strtol(columnCountStr, NULL, 10);
    for (size_t i = 0; i < columnCount; i++) {
      WT_CONFIG_ITEM columnCfg;
      char columnName[11];
      sprintf(columnName, "column%zx", i);
      RETURN_IF_ERROR(config_parser->get(config_parser, columnName, &columnCfg));
      RETURN_IF_ERROR(extractColumnConfiguration(session, &columnCfg));
    }
    return 0;
  }

  /**
   * This allows custom per-field extractors
   * it takes a list of the form [{columns=[1],extractor?=string,params?={}}]
   * As such, it's possible to take multiple columns of input into a single column of output. This only (really) makes sense with a multi-key extractor like ngrams/words
   * The regular form (which probably shouldn't even use a custom extractor) would be [{columns=[1]},{columns=[2]}, ...{columns=[n]}]
   */
  int MultiKeyExtractor::Customize(
    WT_EXTRACTOR *extractor,
    WT_SESSION *session,
    const char *uri,
    WT_CONFIG_ITEM *appcfg,
    WT_EXTRACTOR **customp
  ) {

    char* cfg = (char*)calloc(appcfg->len - 1, 1);
    memcpy(cfg, appcfg->str + 1, appcfg->len - 2);
    WT_CONFIG_PARSER* config_parser;
    WT_CONFIG_ITEM keyFormatCfg;
    WT_CONFIG_ITEM extractKeyFormatCfg;
    WT_CONFIG_ITEM indexIdCfg;
    WT_CONFIG_ITEM tableFormatCfg;


    RETURN_AND_FREE_IF_ERROR(wiredtiger_config_parser_open(
      NULL,
      cfg,
      appcfg->len - 2,
      &config_parser
    ), cfg);

    config_parser->get(config_parser, "index_id", &indexIdCfg);
    char* indexId = copyCfgValue(&indexIdCfg);
    config_parser->get(config_parser, "key_extract_format", &extractKeyFormatCfg);
    char* extractKeyFormat = copyCfgValue(&extractKeyFormatCfg);
    config_parser->get(config_parser, "key_format", &keyFormatCfg);
    char* keyFormat = copyCfgValue(&keyFormatCfg);
    config_parser->get(config_parser, "table_value_format", &tableFormatCfg);
    char* tableFormatStr = copyCfgValue(&tableFormatCfg);

    MultiKeyExtractor* exRef;
    // TODO: this needs to include the key_write_indexes
    if (multiKeyExtractors.find(indexId) != multiKeyExtractors.end()) {
      exRef = multiKeyExtractors.at(indexId);
      exRef->references++;
      *customp=&exRef->extractor;
      free(indexId);
      free(extractKeyFormat);
      free(keyFormat);
      free(tableFormatStr);
      return 0;
    }

    exRef = new MultiKeyExtractor(tableFormatStr, indexId);
    RETURN_IF_ERROR(exRef->extractColumnsConfiguration(session, config_parser));

    exRef->references = 1;

    RETURN_IF_ERROR(parseFormat(extractKeyFormat, exRef->extractFormat));
    RETURN_IF_ERROR(parseFormat(keyFormat, exRef->keyFormat));
    multiKeyExtractors.insert_or_assign(indexId, exRef);
    *customp=&exRef->extractor;
    free(extractKeyFormat);
    free(keyFormat);
    return 0;
  }

  int extractWordsForString(
    char* str,
    std::vector<unique_ptr<char>>* wordStore
  ) {
    size_t totalLen = strlen(str);
    // because multiple columns can refer to the same
    char* copy = (char*)calloc(sizeof(char), totalLen + 1);
    char* copyOrig = copy;
    memcpy(copy, str, totalLen);
    char* token;
    while ((token = strtok_r(copy, " ", &copy))) {
      int strLen = strlen(token);
      if (strLen == 0) {
        continue;
      }
      char* word = (char*) calloc(sizeof(char), strLen + 1);
      memcpy(word, token, strLen);
      wordStore->push_back(unique_ptr<char>(word));
    }
    free(copyOrig);

    return 0;
  }

  int extractNgramsForString(
    char* str,
    size_t ngramLength,
    std::vector<unique_ptr<char>>* ngramStore
  ) {
    size_t totalLen = strlen(str);
    // because multiple columns can refer to the same
    char* copy = (char*)calloc(sizeof(char), totalLen + 1);
    char* copyOrig = copy;
    memcpy(copy, str, totalLen);
    char* token;
    while ((token = strtok_r(copy, " ", &copy))) {
      size_t strLen = strlen(token);
      if (strLen == 0) {
        continue;
      }
      if (strLen <= ngramLength) {
        char* ngram = (char*) calloc(sizeof(char), ngramLength + 1);
        memcpy(ngram, token, strLen);
        ngramStore->push_back(unique_ptr<char>(ngram));
      }
      else {
        for (size_t i = 0; i < strLen - (ngramLength - 1); i++) {
          char* ngram = (char*) calloc(sizeof(char), ngramLength + 1);
          memcpy(ngram, token + i, ngramLength);
          ngramStore->push_back(unique_ptr<char>(ngram));
        }
      }
    }
    free(copyOrig);
    return 0;
  }

  int MultiKeyExtractor::recurseCustomFields(
    WT_CURSOR* result_cursor,
    std::vector<QueryValue>* valueValues,
    std::vector<WT_ITEM>* wtItems,
    std::vector<unique_ptr<std::vector<unique_ptr<char>>>>* multikeys,
    size_t writeIndex,
    size_t multikeyIndex,
    std::vector<QueryValue> tempValues // intentionally *NOT* a ptr or ref
  ) {
    if (writeIndex == columnConfigs->size()) {
      // we're done - issue the call and return.
      av_alist setValueList;
      av_start_void(setValueList, result_cursor->set_key);
      av_ptr(setValueList, WT_CURSOR*, result_cursor);
      for (size_t i = 0; i < tempValues.size(); i++) {
        populateAvListForPackingOrUnPacking(
          &tempValues,
          wtItems,
          &setValueList,
          i,
          { tempValues[i].dataType, 0 },
          true
        );
      }
      av_call(setValueList);
      int error = result_cursor->insert(result_cursor);
      if (error == WT_NOTFOUND || error == WT_DUPLICATE_KEY) {
        return 0;
      }
      return error;
    }
    if ((*columnConfigs)[writeIndex].extractorType != ExtractorType::NONE) {
      // recurse for each and return
      std::vector<unique_ptr<char>>* keys = multikeys->at(multikeyIndex).get();
      for (size_t i = 0; i < keys->size(); i++) {
        std::vector<QueryValue> newTempValues = tempValues;
        QueryValue qv;
        qv.noFree = true;
        qv.value.valuePtr = keys->at(i).get();
        qv.dataType = (*columnConfigs)[writeIndex].format.format;
        newTempValues.push_back(qv);
        RETURN_IF_ERROR(recurseCustomFields(
          result_cursor,
          valueValues,
          wtItems,
          multikeys,
          writeIndex + 1,
          multikeyIndex + 1,
          newTempValues
        ));
      }
      return 0;
    }

    // set the value and recurse.

    std::vector<QueryValue> newTempValues = tempValues;
    size_t index = (*columnConfigs)[writeIndex].columns[0];
    QueryValue qv = (*valueValues)[index];
    newTempValues.push_back(qv);
    qv.dataType = extractFormat->at(index).format;
    return recurseCustomFields(
      result_cursor,
      valueValues,
      wtItems,
      multikeys,
      writeIndex + 1,
      multikeyIndex,
      newTempValues
    );

  }

  int MultiKeyExtractor::extract(
    WT_SESSION *session,
    const WT_ITEM *key,
    const WT_ITEM *value,
    WT_CURSOR *result_cursor
  ) {
    /*
      1. extract the values
      2. filter the values to exclude padding
      3. set those values on the cursor
    */
    // TODO: unpack only up to the last non x.
    av_alist unpackList;
    std::vector<QueryValue> valueValues(extractFormat->size());
    std::vector<WT_ITEM> wtItems(extractFormat->size());
    RETURN_IF_ERROR(unpackWtItem(
      session,
      value,
      tableFormatStr,
      extractFormat,
      &valueValues
    ));

    std::vector<unique_ptr<std::vector<unique_ptr<char>>>> multikeys;

    for (size_t i = 0; i < columnConfigs->size(); i++) {
      if (columnConfigs->at(i).extractorType == ExtractorType::NGRAM) {
        std::vector<unique_ptr<char>>* columnNgrams = new std::vector<unique_ptr<char>>();
        multikeys.push_back(unique_ptr<std::vector<unique_ptr<char>>>(columnNgrams));
        for (size_t a = 0; a < columnConfigs->at(i).columns.size(); a++) {
          extractNgramsForString(
            (char*)valueValues[columnConfigs->at(i).columns[a]].value.valuePtr,
            columnConfigs->at(i).ngrams,
            columnNgrams
          );
        }
      }
      else if (columnConfigs->at(i).extractorType == ExtractorType::WORDS) {
        std::vector<unique_ptr<char>>* columnWords = new std::vector<unique_ptr<char>>();
        multikeys.push_back(unique_ptr<std::vector<unique_ptr<char>>>(columnWords));
        for (size_t a = 0; a < columnConfigs->at(i).columns.size(); a++) {
          extractWordsForString(
            (char*)valueValues[columnConfigs->at(i).columns[a]].value.valuePtr,
            columnWords
          );
        }

      }
    }
    if (multikeys.size() != 0) {
      std::vector<QueryValue> initialTempValues;
      return recurseCustomFields(
        result_cursor,
        &valueValues,
        &wtItems,
        &multikeys,
        0,
        0,
        initialTempValues
      );
    }
    else {
      // we only get here if there is no custom per column extractor
      av_alist setValueList;
      av_start_void(setValueList, result_cursor->set_key);
      av_ptr(setValueList, WT_CURSOR*, result_cursor);
      for (size_t i = 0; i < columnConfigs->size(); i++) {
        int index = columnConfigs->at(i).columns[0];
        Format& format = keyFormat->at(i);
        populateAvListForPackingOrUnPacking(&valueValues, &wtItems, &setValueList, index, format, true);
      }
      av_call(setValueList);
      return result_cursor->insert(result_cursor);
    }
  }

  int MultiKeyExtractor::Extract(
    WT_EXTRACTOR *extractor,
    WT_SESSION *session,
    const WT_ITEM *key,
    const WT_ITEM *value,
    WT_CURSOR *result_cursor
  ) {
    MultiKeyExtractor* exRef = (MultiKeyExtractor*)extractor;
    return exRef->extract(session, key, value, result_cursor);
  }


  int MultiKeyExtractor::Terminate(
    WT_EXTRACTOR* extractor,
    WT_SESSION* session
  ) {
    MultiKeyExtractor* exRef = (MultiKeyExtractor*)extractor;
    exRef->references--;
    if (exRef->references == 0) {
      delete exRef;
    }
    return 0;
  }
}
