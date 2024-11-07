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

  public:
    WT_SESSION *session;

    WiredTigerSession(WT_SESSION *session);

    ~WiredTigerSession();

    int create(char* typeAndName, char* specs);

    int join(Cursor* joinCursor, Cursor* refCursor, char* config);
    int beginTransaction(char* config);
    int prepareTransaction(char* config);
    int commitTransaction(char* config);
    int rollbackTransaction(char* config);

    static WiredTigerSession* WiredTigerSessionForSession(WT_SESSION* session) {
      if (session->app_private) {
        return (WiredTigerSession*)session->app_private;
      }
      return new WiredTigerSession(session);
    }
};
}
#endif
