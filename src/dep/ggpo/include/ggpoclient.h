#ifndef _GGPOCLIENT_H_
#define _GGPOCLIENT_H_

#include <stdarg.h>
#include "ggponet.h"

#define MAX_NUM_PLAYERS 2

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
   GGPOCLIENT_GAMEEVENT_STARTING                     = 6000,
   GGPOCLIENT_GAMEEVENT_PLAYER_1                     = 6002,
   GGPOCLIENT_GAMEEVENT_PLAYER_2                     = 6003,
   GGPOCLIENT_GAMEEVENT_PLAYER_3                     = 6004,
   GGPOCLIENT_GAMEEVENT_PLAYER_4                     = 6005,
   GGPOCLIENT_GAMEEVENT_PLAYER_1_SCORE               = 6006,
   GGPOCLIENT_GAMEEVENT_PLAYER_2_SCORE               = 6007,
   GGPOCLIENT_GAMEEVENT_PLAYER_3_SCORE               = 6008,
   GGPOCLIENT_GAMEEVENT_PLAYER_4_SCORE               = 6009,
   GGPOCLIENT_GAMEEVENT_WINNER                       = 6010,
   GGPOCLIENT_GAMEEVENT_FINISHED                     = 6011,
   GGPOCLIENT_GAMEEVENT_LAST
} GGPOClientGameEventType;

typedef enum {
   GGPOCLIENT_EVENTCODE_FIRST                         = 5000,
   GGPOCLIENT_EVENTCODE_CONNECTING                    = 5000,
   GGPOCLIENT_EVENTCODE_CONNECTED                     = 5001,
   GGPOCLIENT_EVENTCODE_RETREIVING_MATCHINFO          = 5002,
   GGPOCLIENT_EVENTCODE_MATCHINFO                     = 5003,
   GGPOCLIENT_EVENTCODE_SPECTATOR_COUNT_CHANGED       = 5004,
   GGPOCLIENT_EVENTCODE_CHAT                          = 5005,
   GGPOCLIENT_EVENTCODE_DISCONNECTED                  = 5006,
   GGPOCLIENT_EVENTCODE_LAST
} GGPOClientEventCode;

static inline bool
ggpo_is_client_eventcode(int code)
{
   return code >= GGPOCLIENT_EVENTCODE_FIRST && code <= GGPOCLIENT_EVENTCODE_LAST;
}

static inline bool
ggpo_is_client_gameevent(int code)
{
   return code >= GGPOCLIENT_GAMEEVENT_STARTING && code <= GGPOCLIENT_GAMEEVENT_LAST;
}

typedef struct {
   GGPOClientEventCode code;
   union {
      struct {
         char *p1;
         char *p2;
         char *blurb;
      } matchinfo;
      struct {
         int count;
      } spectator_count_changed;
      struct {
         char *username;
         char *text;
      } chat;
   } u;
} GGPOClientEvent;

__declspec(dllexport) GGPOSession * __cdecl ggpo_client_connect(GGPOSessionCallbacks *cb,
                                                                char *game,
                                                                char *matchid,
                                                                int serverport);

__declspec(dllexport) bool __cdecl ggpo_client_chat(GGPOSession *,
                                                    char *text);

__declspec(dllexport) bool __cdecl ggpo_client_set_game_event(GGPOSession *,
                                                              GGPOClientGameEventType type,
                                                              void *data);
#ifdef __cplusplus
}
#endif

#endif
