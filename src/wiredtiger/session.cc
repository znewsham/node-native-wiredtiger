#include <wiredtiger.h>
#include "helpers.h"
#include "cursor.h"
#include "../helpers.h"
#include "session.h"

#include <vector>

using namespace std;

namespace wiredtiger {
  WiredTigerSession::WiredTigerSession(WT_SESSION *session):
    session(session)
  {
    if (session->app_private != NULL) {
      printf("Binding session multiple times!\n");
      abort();
    }
    session->app_private = this;
  }

  WiredTigerSession::~WiredTigerSession() {
  }

  int WiredTigerSession::create(char* typeAndName, char* specs) {
    return session->create(
      session,
      typeAndName,
      specs
    );
  }
  int WiredTigerSession::join(Cursor* joinCursor, Cursor* refCursor, char* config) {
    return session->join(session, joinCursor->cursor, refCursor->cursor, config);
  }
  int WiredTigerSession::beginTransaction(char* config) {
    return session->begin_transaction(session, config);
  }
  int WiredTigerSession::prepareTransaction(char* config) {
    return session->prepare_transaction(session, config);
  }
  int WiredTigerSession::commitTransaction(char* config) {
    return session->commit_transaction(session, config);
  }
  int WiredTigerSession::rollbackTransaction(char* config) {
    return session->rollback_transaction(session, config);
  }
}
