#ifndef _GGPONET_H_
#define _GGPONET_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>

typedef struct GGPOSession GGPOSession;

/*
 * The GGPOEventCode enumeration describes what type of event just happened.
 *
 * GGPO_EVENTCODE_CONNECTED_TO_PEER - Handshake with the game running on the
 * other side of the network has been completed.
 * 
 * GGPO_EVENTCODE_SYNCHRONIZING_WITH_PEER - Beginning the synchronization
 * process with the client on the other end of the networking.  The count
 * and total fields in the u.synchronizing struct of the GGPOEvent
 * object indicate progress.
 *
 * GGPO_EVENTCODE_RUNNING - The clients have synchronized.  You may begin
 * sending inputs with ggpo_synchronize_inputs.
 *
 * GGPO_EVENTCODE_DISCONNECTED_FROM_PEER - The network connection on 
 * the other end of the network has closed.
 *
 * GGPO_EVENTCODE_TIMESYNC - The time synchronziation code has determined
 * that this client is too far ahead of the other one and should slow
 * down to ensure fairness.  The u.timesync.frames_ahead parameter in
 * the GGPOEvent object indicates how many frames the client is.
 *
 */
typedef enum {
   GGPO_EVENTCODE_CONNECTED_TO_PEER            = 1000,
   GGPO_EVENTCODE_SYNCHRONIZING_WITH_PEER      = 1001,
   GGPO_EVENTCODE_RUNNING                      = 1002,
   GGPO_EVENTCODE_DISCONNECTED_FROM_PEER       = 1003,
   GGPO_EVENTCODE_TIMESYNC                     = 1004
} GGPOEventCode;

/*
 * The GGPOEvent structure contains an asynchronous event notification sent
 * by the on_event callback.  See GGPOEventCode, above, for a detailed
 * explanation of each event.
 */
typedef struct {
   GGPOEventCode code;
   union {
      struct {
         int      count;
         int      total;
      } synchronizing;
      struct {
         int      frames_ahead;
      } timesync;
   } u;
} GGPOEvent;

/*
 * The GGPOSessionCallbacks structure contains the callback functions that
 * your application must implement.  GGPO.net will periodically call these
 * functions during the game.  All callback functions must be implemented.
 */
typedef struct {
   /*
    * begin_game callback - This callback has been deprecated.  You must
    * implement it, but should ignore the 'game' parameter.
    */
   bool (__cdecl *begin_game)(char *game);

   /*
    * save_game_state - The client should allocate a buffer, copy the
    * entire contents of the current game state into it, and copy the
    * length into the *len parameter.  Optionally, the client can compute
    * a checksum of the data and store it in the *checksum argument.
    */
   bool (__cdecl *save_game_state)(unsigned char **buffer, int *len, int *checksum, int frame);

   /*
    * load_game_state - GGPO.net will call this function at the beginning
    * of a rollback.  The buffer and len parameters contain a previously
    * saved state returned from the save_game_state function.  The client
    * should make the current game state match the state contained in the
    * buffer.
    */
   bool (__cdecl *load_game_state)(unsigned char *buffer, int len);

   /*
    * log_game_state - Used in diagnostic testing.  The client should use
    * the ggpo_log function to write the contents of the specified save
    * state in a human readible form.
    */
   bool (__cdecl *log_game_state)(char *filename, unsigned char *buffer, int len);

   /*
    * free_buffer - Frees a game state allocated in save_game_state.  You
    * should deallocate the memory contained in the buffer.
    */
   void (__cdecl *free_buffer)(void *buffer);

   /*
    * advance_frame - Called during a rollback.  You should advance your game
    * state by exactly one frame.  Before each frame, call ggpo_synchronize_input
    * to retrieve the inputs you should use for that frame.  After each frame,
    * you should call ggpo_advance_frame to notify GGPO.net that you're
    * finished.
    *
    * The flags parameter is reserved.  It can safely be ignored at this time.
    */
   bool (__cdecl *advance_frame)(int flags);

   /* 
    * on_event - Notification that something has happened.  See the GGPOEventCode
    * structure above for more information.
    */
   bool (__cdecl *on_event)(GGPOEvent *info);
} GGPOSessionCallbacks;

/*
 * The GGPONetworkStats function contains some statistics about the current
 * session.
 *
 * network.predict_queue_len - The length of the prediction queue containing
 * the predicted inputs for the remote player.  The number of frames that
 * get rolled back on a misprediction is capped by the length of the 
 * prediction queue.
 *
 * network.send_queue_len - The length of the queue containing UDP packets
 * which have not yet been acknowledged by the end client.  The length of
 * the send queue is a rough indication of the quality of the connection.
 * The longer the send queue, the higher the round-trip time between the
 * clients.  The send queue will also be longer than usual during high
 * packet loss situations.
 *
 * network.recv_queue_len - The number of inputs currently buffered by the
 * GGPO.net network layer which have yet to be validated.  The length of
 * the prediction queue is roughly equal to the current frame number
 * minus the frame number of the last packet in the remote queue.
 *
 * network.ping - The roundtrip packet transmission time as calcuated
 * by GGPO.net.  This will be roughly equal to the actual round trip
 * packet transmission time + 2 the interval at which you call ggpo_idle
 * or ggpo_advance_frame.
 *
 * network.kbps - The estimated bandwidth used to synchronize the two
 * clients, in kilobits per second.
 *
 * timesync.local_frames_behind - The number of frames GGPO.net calculates
 * that the local client is behind the remote client at this instant in
 * time.  For example, if at this instant the current game client is running
 * frame 1002 and the remote game client is running frame 1009, this value
 * will mostly likely roughly equal 7.
 *
 * timesync.remote_frames_behind - The same as local_frames_behind, but
 * calculated from the perspective of the remote player.
 *
 */
typedef struct {
   struct {
      int   predict_queue_len;
      int   send_queue_len;
      int   recv_queue_len;
      int   ping;
      int   kbps_sent;
   } network;
   struct {
      int   local_frames_behind;
      int   remote_frames_behind;
   } timesync;
} GGPONetworkStats;

/*
 * ggpo_start_session --
 *
 * Used to being a new GGPO.net session.  The ggpo object returned by ggpo_start_session
 * uniquely identifies the state for this session and should be passed to all other
 * functions.
 *
 * cb - A GGPOSessionCallbacks structure which contains the callbacks you implement
 * to help GGPO.net synchronize the two games.  You must implement all functions in
 * cb, even if they do nothing but 'return true';
 *
 * localport - The port you'd like to bind to on the local machine.
 *
 * remoteip - A string containing the ip address of the remote client you'd like
 * to connect to.
 *
 * remoteport - The port on the remote machine that you'd like to connect to.
 *
 * player_num - Which player you'll be providing inputs for in ggpo_synchronize_input.
 * Pass 0 for player 1.   Pass 1 for player 2.
 *
 */
__declspec(dllexport) GGPOSession * __cdecl ggpo_start_session(GGPOSessionCallbacks *cb,
                                                               char *game,
                                                               int localport,
                                                               char *remoteip,
                                                               int remoteport,
                                                               int player_num);

__declspec(dllexport) GGPOSession * __cdecl ggpo_start_synctest(GGPOSessionCallbacks *cb,
                                                                char *game,
                                                                int frames);

__declspec(dllexport) GGPOSession * __cdecl ggpo_start_streaming(GGPOSessionCallbacks *cb,
                                                                 char *game,
                                                                 char *matchid,
                                                                 int port);

__declspec(dllexport) GGPOSession * __cdecl ggpo_start_replay(GGPOSessionCallbacks *cb,
                                                              char *file);

/*
 * ggpo_close_session --
 * Used to close a session.  You must call ggpo_close_session to
 * free the resources allocated in ggpo_start_session.
 */
__declspec(dllexport) void __cdecl ggpo_close_session(GGPOSession *);

/*
 * ggpo_idle --
 * Should be called periodically by your application to give GGPO.net
 * a chance to do some work.  Most packet transmissions and rollbacks occur
 * in ggpo_idle.
 *
 * timeout - The amount of time GGPO.net is allowed to spend in this function,
 * in milliseconds.
 */
__declspec(dllexport) bool __cdecl ggpo_idle(GGPOSession *,
                                             int timeout);

/*
 * ggpo_synchronize_input --
 *
 * Used both to notify GGPO.net of inputs that should be trasmitted by
 * the remote end and to notify the client of the other player's inputs.
 * You should call ggpo_synchronize_input before every frame of execution,
 * including those frames which happen during rollback.
 *
 * values - Before calling ggpo_synchronize_input, the values parameter should
 * contain the values for the player this client represents.  The inputs should
 * be packed at the beginning of the values parameter.  When the function
 * returns, the values parameter will contain inputs for this frame for both
 * players.  Therefore, the values array must be at least (size * players)
 * large.
 *
 * size - The size of a single player's inputs in bytes.  
 *
 * players - The number of players participating in the game.  You must pass
 * '2' for this parameter.
 */
__declspec(dllexport) bool __cdecl ggpo_synchronize_input(GGPOSession *,
                                                          void *values,
                                                          int size,
                                                          int players);

/*
 * ggpo_advance_frame --
 *
 * You should call ggpo_advance_frame to notify GGPO.net that you have
 * advanced your gamestate by a single frame.  You should call this everytime
 * you advance the gamestate by a frame, even during rollbacks.  GGPO.net
 * may call your save_state callback before this function returns.
 */
__declspec(dllexport) bool __cdecl ggpo_advance_frame(GGPOSession *);

/*
 * ggpo_get_stats --
 *
 * Used to fetch some statistics about the quality of the network connection.
 */
__declspec(dllexport) bool __cdecl ggpo_get_stats(GGPOSession *,
                                                  GGPONetworkStats *stats);

/*
 * ggpo_log --
 *
 * Used to write to the ggpo.net log.  In the current versions of the
 * SDK, a log file is only generated if the "quark.log" environment
 * variable is set to 1.  This will change in future versions of the
 * SDK.
 */
__declspec(dllexport) void __cdecl ggpo_log(GGPOSession *,
                                            char *fmt, ...);
/*
 * ggpo_logv --
 *
 * A varargs compatible version of ggpo_log.  See ggpo_log for
 * more details.
 */
__declspec(dllexport) void __cdecl ggpo_logv(GGPOSession *,
                                             char *fmt,
                                             va_list args);

#ifdef __cplusplus
}
#endif

#endif
