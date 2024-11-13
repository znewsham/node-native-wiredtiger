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
    printf("Reset: %p\n", this);
    // TODO: close session if not already
  }

  bool WiredTigerSession::isInTransaction() {
    return _isInTransaction;
  }

  int WiredTigerSession::create(char* typeAndName, char* specs) {
    return session->create(
      session,
      typeAndName,
      specs
    );
  }

  int WiredTigerSession::close(char* config) {
    return session->close(session, config);
  }
  int WiredTigerSession::join(Cursor* joinCursor, Cursor* refCursor, char* config) {
    return session->join(session, joinCursor->getWTCursor(), refCursor->getWTCursor(), config);
  }
  int WiredTigerSession::openCursor(char* cursorSpec, Cursor** cursor) {
    WT_CURSOR* wtCursor;
    RETURN_IF_ERROR(session->open_cursor(session, cursorSpec, NULL, NULL, &wtCursor));
    *cursor = new Cursor(wtCursor);
    return 0;
  }
  int WiredTigerSession::openCursor(char* cursorSpec, char* config, Cursor** cursor) {
    WT_CURSOR* wtCursor;
    RETURN_IF_ERROR(session->open_cursor(session, cursorSpec, NULL, config, &wtCursor));
    *cursor = new Cursor(wtCursor);
    return 0;
  }

  int WiredTigerSession::beginTransaction(char* config) {
    _isInTransaction = true;
    return session->begin_transaction(session, config);
  }
  int WiredTigerSession::prepareTransaction(char* config) {
    return session->prepare_transaction(session, config);
  }
  int WiredTigerSession::commitTransaction(char* config) {
    _isInTransaction = false;
    return session->commit_transaction(session, config);
  }
  int WiredTigerSession::rollbackTransaction(char* config) {
    _isInTransaction = false;
    return session->rollback_transaction(session, config);
  }
}
