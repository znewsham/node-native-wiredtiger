#include "custom_collator.h"

namespace wiredtiger {
  int CustomCollator::Compare(
    WT_COLLATOR *collator,
    WT_SESSION *session,
    const WT_ITEM *v1,
    const WT_ITEM *v2,
    int* cmp
  ) {
    CustomCollator* actualCollator = CustomCollator::CustomCollatorForCollator(collator);
    return actualCollator->compare(session, v1, v2, cmp);
  }

  int CustomCollator::Customize(
    WT_COLLATOR *collator,
    WT_SESSION *session,
    const char *uri,
    WT_CONFIG_ITEM *appcfg,
    WT_COLLATOR **customp
  ) {
    CustomCollator* actualCollator = CustomCollator::CustomCollatorForCollator(collator);
    return actualCollator->customize(session, uri, appcfg, customp);
  }


  int CustomCollator::Terminate(
    WT_COLLATOR *collator,
    WT_SESSION* session
  ) {
    CustomCollator* actualCollator = CustomCollator::CustomCollatorForCollator(collator);
    return actualCollator->terminate(session);
  }

  int compareNoCase(
    WT_COLLATOR *collator,
    WT_SESSION *session,
    const WT_ITEM *v1,
    const WT_ITEM *v2,
    int *cmp
  ) {
    // TODO: will only work if the string column is first.
    const char *s1 = (const char *)v1->data;
    const char *s2 = (const char *)v2->data;
    *cmp = strcasecmp(s1, s2);
    return (0);
  }

  int compareReverse(
    WT_COLLATOR *collator,
    WT_SESSION *session,
    const WT_ITEM *v1,
    const WT_ITEM *v2,
    int *cmp
  ) {
    *cmp = -wiredtiger_lex_compare(v1, v2);
    return 0;
  }

  int compareRegular(
    WT_COLLATOR *collator,
    WT_SESSION *session,
    const WT_ITEM *v1,
    const WT_ITEM *v2,
    int *cmp
  ) {
    *cmp = wiredtiger_lex_compare(v1, v2);
    return 0;
  }
}
