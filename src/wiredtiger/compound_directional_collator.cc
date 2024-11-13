#include "compound_directional_collator.h"
#include "helpers.h"
#include "wt_item.h"
#include <avcall.h>
#include <memory>
#include <map>

namespace wiredtiger {
  static map<string, CompoundDirectionalCollator*> compoundDirectionalCollators;

  int CompoundDirectionalCollator::extractColumnConfiguration(
    WT_SESSION *session,
    WT_CONFIG_ITEM *columnCfg
  ) {
    char* cfg = (char*)calloc(columnCfg->len - 1, 1);
    memcpy(cfg, columnCfg->str + 1, columnCfg->len - 2);
    WT_CONFIG_PARSER* config_parser;
    WT_CONFIG_ITEM directionCfg;

    RETURN_AND_FREE_IF_ERROR(wiredtiger_config_parser_open(
      NULL,
      cfg,
      columnCfg->len - 2,
      &config_parser
    ), cfg);


    config_parser->get(config_parser, "direction", &directionCfg);
    char* directionStr = copyCfgValue(&directionCfg);
    free(cfg);

    CompoundDirectionalCollatorColumnConfig columnConfig;
    if (directionStr != NULL) {
      columnConfig.direction = (CollatorDirection)strtol(directionStr, NULL, 10);
    }
    columnConfigs->push_back(columnConfig);
    return 0;
  }

  int CompoundDirectionalCollator::extractColumnsConfiguration(
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

  int CompoundDirectionalCollator::compare(
    WT_SESSION *session,
    const WT_ITEM *v1,
    const WT_ITEM *v2,
    int* compare
) {
    if (direction == CollatorDirection::FORWARD) {
      *compare = wiredtiger_lex_compare(v1, v2);
      return 0;
    }
    if (direction == CollatorDirection::REVERSE) {
      *compare = -wiredtiger_lex_compare(v1, v2);
      return 0;
    }

    std::vector<QueryValue> v1Values(extractFormat->size());
    std::vector<QueryValue> v2Values(extractFormat->size());
    RETURN_IF_ERROR(unpackWtItem(
      session,
      v1,
      keyFormat,
      extractFormat,
      &v1Values
    ));
    RETURN_IF_ERROR(unpackWtItem(
      session,
      v2,
      keyFormat,
      extractFormat,
      &v2Values
    ));
    for (size_t i = 0; i < columnConfigs->size(); i++) {
      CollatorDirection direction = columnConfigs->at(i).direction;
      char format = extractFormat->at(i).format;
      int cmp;
      if (format == 'S') {
        cmp = strcmp((char*)v1Values[i].value.valuePtr, (char*)v2Values[i].value.valuePtr);
        if (cmp > 0) { cmp = 1; }
        else if (cmp < 0) { cmp = -1; }
      }
      else if (format == 's') {
        cmp = strncmp((char*)v1Values[i].value.valuePtr, (char*)v2Values[i].value.valuePtr, min(v1Values[i].size, v2Values[i].size));
        if (cmp > 0) { cmp = 1; }
        else if (cmp < 0) { cmp = -1; }
      }
      else if (FieldIsWTItem(format)) {
        WT_ITEM item1;
        WT_ITEM item2;
        item1.size = v1Values[i].size;
        item1.data = v1Values[i].value.valuePtr;
        item2.size = v2Values[i].size;
        item2.data = v2Values[i].value.valuePtr;
        cmp = wiredtiger_lex_compare(&item1, &item2);
      }
      else {
        // numeric comparison
        // TODO - I don't think this is quite right with signed vs unsigned
        cmp = (v2Values[i].value.valueUint - v2Values[i].value.valueUint);
        if (cmp > 0) { cmp = 1; }
        else if (cmp < 0) { cmp = -1; }
      }

      if (cmp != 0) {
        *compare = direction == CollatorDirection::FORWARD ? cmp : -cmp;
        return 0;
      }
    }
    *compare = 0;
    return 0;
  }

  int CompoundDirectionalCollator::Compare(
    WT_COLLATOR *collator,
    WT_SESSION *session,
    const WT_ITEM *v1,
    const WT_ITEM *v2,
    int* compare
  ) {
    CompoundDirectionalCollator* colRef = (CompoundDirectionalCollator*)collator;
    return colRef->compare(session, v1, v2, compare);
  }

  int CompoundDirectionalCollator::Customize(
    WT_COLLATOR *collator,
    WT_SESSION *session,
    const char *uri,
    WT_CONFIG_ITEM *appcfg,
    WT_COLLATOR **customp
  ) {
    char* cfg = (char*)calloc(appcfg->len - 1, 1);
    memcpy(cfg, appcfg->str + 1, appcfg->len - 2);
    WT_CONFIG_PARSER* config_parser;
    WT_CONFIG_ITEM keyFormatCfg;
    WT_CONFIG_ITEM indexIdCfg;
    WT_CONFIG_ITEM directionCfg;


    RETURN_AND_FREE_IF_ERROR(wiredtiger_config_parser_open(
      NULL,
      cfg,
      appcfg->len - 2,
      &config_parser
    ), cfg);

    config_parser->get(config_parser, "index_id", &indexIdCfg);
    char* indexId = copyCfgValue(&indexIdCfg);

    config_parser->get(config_parser, "direction", &directionCfg);
    char* direction = copyCfgValue(&directionCfg);

    config_parser->get(config_parser, "key_format", &keyFormatCfg);
    char* keyFormat = copyCfgValue(&keyFormatCfg);

    CompoundDirectionalCollator* colRef;
    if (compoundDirectionalCollators.count(indexId) != 0) {
      colRef = compoundDirectionalCollators.at(indexId);
      colRef->references++;
      *customp=&colRef->collator;
      free(indexId);
      free(keyFormat);
      free(direction);
      return 0;
    }

    colRef = new CompoundDirectionalCollator(indexId);
    RETURN_IF_ERROR(colRef->extractColumnsConfiguration(session, config_parser));

    colRef->references = 1;
    colRef->keyFormat = keyFormat;
    colRef->direction = (CollatorDirection)strtol(direction, NULL, 10);
    RETURN_IF_ERROR(parseFormat(keyFormat, colRef->extractFormat));

    compoundDirectionalCollators.insert_or_assign(indexId, colRef);
    *customp=&colRef->collator;
    free(direction);
    return 0;
  }

  int CompoundDirectionalCollator::Terminate(
    WT_COLLATOR* collator,
    WT_SESSION* session
  ) {
    CompoundDirectionalCollator* colRef = (CompoundDirectionalCollator*)collator;
    colRef->references--;
    if (colRef->references == 0) {
      // delete colRef;
    }
    return 0;
  }
}
