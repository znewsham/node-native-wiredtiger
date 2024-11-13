#include "custom_extractor.h"
#include "helpers.h"
#include <avcall.h>
#include <memory>

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
}
