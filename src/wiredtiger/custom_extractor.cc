#include "custom_extractor.h"

namespace wiredtiger {
  int CustomExtractor::Extract (
    WT_EXTRACTOR *extractor,
    WT_SESSION *session,
    const WT_ITEM *key,
    const WT_ITEM *value,
    WT_CURSOR *result_cursor
  ) {
    CustomExtractor *actualExtractor = CustomExtractor::CustomExtractorForExtractor(extractor);
    return actualExtractor->extract(session, key, value, result_cursor);
  }

  int CustomExtractor::Customize(
    WT_EXTRACTOR *extractor,
    WT_SESSION *session,
    const char *uri,
    WT_CONFIG_ITEM *appcfg,
    WT_EXTRACTOR **customp
  ) {
    CustomExtractor *actualExtractor = CustomExtractor::CustomExtractorForExtractor(extractor);
    return actualExtractor->customize(session, uri, appcfg, customp);
  }


  int CustomExtractor::Terminate(
    WT_EXTRACTOR* extractor,
    WT_SESSION* session
  ) {
    CustomExtractor *actualExtractor = CustomExtractor::CustomExtractorForExtractor(extractor);
    return actualExtractor->terminate(session);
  }
  static WT_EXTRACTOR* baselineNgramExtractor;

  static map<string, ExtractorReferences*> ngramExtractors;

  int terminateNgrams(
    WT_EXTRACTOR* extractor,
    WT_SESSION* session
  ) {
    if (extractor == baselineNgramExtractor) {
      return 0;
    }
    ExtractorReferences* exRef = (ExtractorReferences*)extractor;
    exRef->references--;
    if (exRef->references == 0) {
      ngramExtractors.erase(exRef->key);
      free(exRef->key);
      free(exRef);
    }
    return 0;
  }


  int extractNgrams(
    WT_EXTRACTOR *extractor,
    WT_SESSION *session,
    const WT_ITEM *key,
    const WT_ITEM *value,
    WT_CURSOR *result_cursor
  ) {
    ExtractorReferences* exRef = (ExtractorReferences*)extractor;
    int ngramLength = exRef->config.ngram;
    char* str = (char*) value->data;
    char* ngram = (char*) calloc(sizeof(char), ngramLength + 1);
    char* token;
    while ((token = strtok_r(str, " ", &str))) {
      int strLen = strlen(token);
      if (strLen == 0) {
        continue;
      }
      if (strLen <= ngramLength) {
          result_cursor->set_key(result_cursor, token);
          RETURN_AND_FREE_IF_ERROR_NOT_NOTFOUND_OR_DUPLICATE(result_cursor->insert(result_cursor), ngram);
      }
      else {
        for (int i = 0; i < strLen - (ngramLength - 1); i++) {
          memcpy(ngram, token + i, ngramLength);
          result_cursor->set_key(result_cursor, ngram);
          RETURN_AND_FREE_IF_ERROR_NOT_NOTFOUND_OR_DUPLICATE(result_cursor->insert(result_cursor), ngram);
        }
      }
    }
    free(ngram);
    return 0;
  }

  int customizeNgrams(
    WT_EXTRACTOR *extractor,
    WT_SESSION *session,
    const char *uri,
    WT_CONFIG_ITEM *appcfg,
    WT_EXTRACTOR **customp
  ) {
    // for some reason we get the same extractor over and over - we always "return" a different one,
    // but this one still gets terminated - we need to not do anything for it (and dont want to read past its memory)
    baselineNgramExtractor = extractor;
    char* cfg = (char*)calloc(appcfg->len - 1, 1);
    memcpy(cfg, appcfg->str + 1, appcfg->len - 2);
    WT_CONFIG_PARSER* config_parser;
    WT_CONFIG_ITEM ngrams;
    RETURN_AND_FREE_IF_ERROR(wiredtiger_config_parser_open(
      NULL,
      cfg,
      appcfg->len - 2,
      &config_parser
    ), cfg);
    RETURN_AND_FREE_IF_ERROR(config_parser->get(config_parser, "ngrams", &ngrams), cfg);
    RETURN_AND_FREE_IF_ERROR(config_parser->close(config_parser), cfg);
    char* ngramStr = copyCfgValue(&ngrams);

    ExtractorReferences* exRef;
    if (ngramExtractors.find(ngramStr) != ngramExtractors.end()) {
      exRef = ngramExtractors.at(ngramStr);
      exRef->references++;
      *customp=&exRef->extractor;
      free(ngramStr);
      free(cfg);
      return 0;
    }

    exRef = (ExtractorReferences*)malloc(sizeof(ExtractorReferences));
    exRef->extractor.extract = extractor->extract;
    exRef->extractor.terminate = extractor->terminate;
    exRef->references = 1;
    exRef->config.ngram = strtol(ngramStr, NULL, 10);
    exRef->key = ngramStr;
    ngramExtractors.insert_or_assign(ngramStr, exRef);
    *customp=&exRef->extractor;
    free (cfg);
    return 0;
  }

  int extractWords(
    WT_EXTRACTOR *extractor,
    WT_SESSION *session,
    const WT_ITEM *key,
    const WT_ITEM *value,
    WT_CURSOR *result_cursor
  ) {
    char* buffer;
    RETURN_IF_ERROR(wiredtiger_struct_unpack(session, value->data, value->size, "S", &buffer));
    char* token;
    while ((token = strtok_r(buffer, " ", &buffer))) {
      result_cursor->set_key(result_cursor, token);
      RETURN_IF_ERROR_NOT_NOTFOUND_OR_DUPLICATE(result_cursor->insert(result_cursor));
    }

    return 0;
  }
}
