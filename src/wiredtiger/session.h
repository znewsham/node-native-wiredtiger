#include <wiredtiger.h>
#include "helpers.h"
#include "cursor.h"
#include "../helpers.h"

#include <cstring>
#include <vector>

using namespace std;

#ifndef WIREDTIGER_SESSION_H
#define WIREDTIGER_SESSION_H

namespace wiredtiger {
class WiredTigerSession {
  private:
    WT_SESSION *session;

  public:
    bool _isInTransaction = false;

    WiredTigerSession(WT_SESSION *session);

    ~WiredTigerSession();

    bool isInTransaction();
    int create(char* typeAndName, char* specs);
    int close(char* config);
    int join(Cursor* joinCursor, Cursor* refCursor, char* config);

    int openCursor(char* cursorSpec, Cursor** cursor);
    int openCursor(char* cursorSpec, char* config, Cursor** cursor);

    int beginTransaction(char* config);
    int prepareTransaction(char* config);
    int commitTransaction(char* config);
    int rollbackTransaction(char* config);
    inline const char* strerror(int error) {
      return this->session->strerror(this->session, error);
    }

    // aim to avoid if possible
    inline WT_SESSION* getWTSession() {
      return this->session;
    }

    static WiredTigerSession* WiredTigerSessionForSession(WT_SESSION* session) {
      if (session->app_private) {
        return (WiredTigerSession*)session->app_private;
      }
      return new WiredTigerSession(session);
    }
};
}
#endif
