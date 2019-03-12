// Copyright 2019 Samsung Electronics Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#ifndef CONTENT_RENDERER_MEDIA_CASTANETS_RENDERER_MEDIA_PLAYER_MANAGER_CASTANETS_H_
#define CONTENT_RENDERER_MEDIA_CASTANETS_RENDERER_MEDIA_PLAYER_MANAGER_CASTANETS_H_

#include "base/time/time.h"
#include "content/public/renderer/render_frame_observer.h"
#include "media/blink/renderer_media_player_interface.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"

namespace media {
class WebMediaPlayerCastanets;
}

namespace content {

class RendererMediaPlayerManager
    : public RenderFrameObserver,
      public media::RendererMediaPlayerManagerInterface {
 public:
  // Constructs a RendererMediaPlayerManager object for the |render_frame|.
  explicit RendererMediaPlayerManager(RenderFrame* render_frame);
  ~RendererMediaPlayerManager() override;

  // Initializes a MediaPlayerCastanets object in browser process.
  void Initialize(int player_id,
                  MediaPlayerHostMsg_Initialize_Type type,
                  const GURL& url,
                  const std::string& mime_type,
                  int demuxer_client_id) override;

  // Starts the player.
  void Start(int player_id) override;

  // Pauses the player.
  // is_media_related_action should be true if this pause is coming from an
  // an action that explicitly pauses the video (user pressing pause, JS, etc.)
  // Otherwise it should be false if Pause is being called due to other reasons
  // (cleanup, freeing resources, etc.)
  void Pause(int player_id, bool is_media_related_action) override;

  // Performs seek on the player.
  void Seek(int player_id, const base::TimeDelta& time) override;

  // Sets the player volume.
  void SetVolume(int player_id, double volume) override;

  // Releases resources for the player after being suspended.
  void SuspendAndReleaseResources(int player_id) override {}

  // Sets the playback rate.
  void SetRate(int player_id, double rate) override;

  // Destroys the player in the browser process
  void DestroyPlayer(int player_id) override;

  // Registers and unregisters a WebMediaPlayerCastanets object.
  int RegisterMediaPlayer(blink::WebMediaPlayer* player) override;
  void UnregisterMediaPlayer(int player_id) override;

  // Requests the player to enter/exit fullscreen.
  void EnteredFullscreen(int player_id) override;
  void ExitedFullscreen(int player_id) override;

#if defined(VIDEO_HOLE)
  void SetMediaGeometry(int player_id, const gfx::RectF& rect) override;
#endif

  // RenderFrameObserver overrides.
  void OnDestruct() override {}
  bool OnMessageReceived(const IPC::Message& message) override;
  void WasHidden() override {}
  void WasShown() override {}
  void OnStop() override {}

  blink::WebMediaPlayer* GetMediaPlayer(int player_id);

 private:
  // Message Handlers.
  void OnMediaDataChange(int player_id, int width, int height, int media);
  void OnDurationChange(int player_id, base::TimeDelta duration);
  void OnTimeUpdate(int player_id, base::TimeDelta current_time);
  void OnBufferUpdate(int player_id, int percentage);
  void OnTimeChanged(int player_id);
  void OnPauseStateChange(int player_id, bool state);
  void OnSeekComplete(int player_id);
  void OnRequestSeek(int player_id, base::TimeDelta seek_time);
  void OnPlayerSuspend(int player_id, bool is_preempted);
  void OnPlayerResumed(int player_id, bool is_preempted);
  void OnReadyStateChange(int player_id,
                          blink::WebMediaPlayer::ReadyState state);
  void OnNetworkStateChange(int player_id,
                            blink::WebMediaPlayer::NetworkState state);
  void OnPlayerDestroyed(int player_id);

  std::map<int, blink::WebMediaPlayer*> media_players_;

  int next_media_player_id_;

  DISALLOW_COPY_AND_ASSIGN(RendererMediaPlayerManager);
};
}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CASTANETS_RENDERER_MEDIA_PLAYER_MANAGER_CASTANETS_H_
