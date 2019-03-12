// Copyright 2019 Samsung Electronics Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/castanets/renderer_media_player_manager_castanets.h"
#include "content/common/media/media_player_init_config.h"
#include "content/common/media/media_player_messages_castanets.h"

namespace content {

RendererMediaPlayerManager::RendererMediaPlayerManager(
    RenderFrame* render_frame)
    : RenderFrameObserver(render_frame), next_media_player_id_(0) {}

RendererMediaPlayerManager::~RendererMediaPlayerManager() {
  DCHECK(media_players_.empty())
      << "RendererMediaPlayerManager is owned by RenderFrameImpl and is "
         "destroyed only after all media players are destroyed.";
}

void RendererMediaPlayerManager::Initialize(
    int player_id,
    MediaPlayerHostMsg_Initialize_Type type,
    const GURL& url,
    const std::string& mime_type,
    int demuxer_client_id) {
  bool has_encrypted_listener_or_cdm = false;
  MediaPlayerInitConfig config{type, url, mime_type, demuxer_client_id,
                               has_encrypted_listener_or_cdm};
  LOG(INFO) << __FUNCTION__ << " [RENDERER] route:" << routing_id()
            << ", player:" << player_id << ", type:" << static_cast<int>(type);
  Send(new MediaPlayerCastanetsHostMsg_Init(routing_id(), player_id, config));
}

void RendererMediaPlayerManager::Start(int player_id) {
  Send(new MediaPlayerCastanetsHostMsg_Play(routing_id(), player_id));
}

void RendererMediaPlayerManager::Pause(int player_id,
                                       bool is_media_related_action) {
  Send(new MediaPlayerCastanetsHostMsg_Pause(routing_id(), player_id,
                                             is_media_related_action));
}

void RendererMediaPlayerManager::Seek(int player_id,
                                      const base::TimeDelta& time) {
  Send(new MediaPlayerCastanetsHostMsg_Seek(routing_id(), player_id, time));
}

void RendererMediaPlayerManager::SetVolume(int player_id, double volume) {
  Send(new MediaPlayerCastanetsHostMsg_SetVolume(routing_id(), player_id,
                                                 volume));
}

void RendererMediaPlayerManager::SetRate(int player_id, double rate) {
  Send(new MediaPlayerCastanetsHostMsg_SetRate(routing_id(), player_id, rate));
}

bool RendererMediaPlayerManager::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RendererMediaPlayerManager, message)
    IPC_MESSAGE_HANDLER(MediaPlayerCastanetsMsg_MediaDataChanged,
                        OnMediaDataChange)
    IPC_MESSAGE_HANDLER(MediaPlayerCastanetsMsg_DurationChanged,
                        OnDurationChange)
    IPC_MESSAGE_HANDLER(MediaPlayerCastanetsMsg_TimeUpdate, OnTimeUpdate)
    IPC_MESSAGE_HANDLER(MediaPlayerCastanetsMsg_BufferUpdate, OnBufferUpdate)
    IPC_MESSAGE_HANDLER(MediaPlayerCastanetsMsg_ReadyStateChange,
                        OnReadyStateChange)
    IPC_MESSAGE_HANDLER(MediaPlayerCastanetsMsg_NetworkStateChange,
                        OnNetworkStateChange)
    IPC_MESSAGE_HANDLER(MediaPlayerCastanetsMsg_TimeChanged, OnTimeChanged)
    IPC_MESSAGE_HANDLER(MediaPlayerCastanetsMsg_PauseStateChanged,
                        OnPauseStateChange)
    IPC_MESSAGE_HANDLER(MediaPlayerCastanetsMsg_OnSeekComplete, OnSeekComplete)
    IPC_MESSAGE_HANDLER(MediaPlayerCastanetsMsg_SeekRequest, OnRequestSeek)
    IPC_MESSAGE_HANDLER(MediaPlayerCastanetsMsg_PlayerSuspend, OnPlayerSuspend)
    IPC_MESSAGE_HANDLER(MediaPlayerCastanetsMsg_PlayerResumed, OnPlayerResumed)
    IPC_MESSAGE_HANDLER(MediaPlayerCastanetsMsg_PlayerDestroyed,
                        OnPlayerDestroyed)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

blink::WebMediaPlayer* RendererMediaPlayerManager::GetMediaPlayer(
    int player_id) {
  std::map<int, blink::WebMediaPlayer*>::iterator iter =
      media_players_.find(player_id);
  if (iter != media_players_.end())
    return iter->second;
  return NULL;
}

void RendererMediaPlayerManager::OnMediaDataChange(int player_id,
                                                   int width,
                                                   int height,
                                                   int media) {
  auto* player = GetMediaPlayer(player_id);
  if (player)
    player->OnMediaDataChange(width, height, media);
}

void RendererMediaPlayerManager::OnDurationChange(int player_id,
                                                  base::TimeDelta duration) {
  auto* player = GetMediaPlayer(player_id);
  if (player)
    player->OnDurationChange(duration);
}

void RendererMediaPlayerManager::OnTimeUpdate(int player_id,
                                              base::TimeDelta current_time) {
  auto* player = GetMediaPlayer(player_id);
  if (player)
    player->OnTimeUpdate(current_time);
}

void RendererMediaPlayerManager::OnBufferUpdate(int player_id, int percentage) {
  auto* player = GetMediaPlayer(player_id);
  if (player)
    player->OnBufferUpdate(percentage);
}

void RendererMediaPlayerManager::OnTimeChanged(int player_id) {
  auto* player = GetMediaPlayer(player_id);
  if (player)
    player->OnTimeChanged();
}

void RendererMediaPlayerManager::OnPauseStateChange(int player_id, bool state) {
  auto* player = GetMediaPlayer(player_id);
  if (player)
    player->OnPauseStateChange(state);
}

void RendererMediaPlayerManager::OnSeekComplete(int player_id) {
  auto* player = GetMediaPlayer(player_id);
  if (player)
    player->OnSeekComplete();
}

void RendererMediaPlayerManager::OnRequestSeek(int player_id,
                                               base::TimeDelta seek_time) {
  auto* player = GetMediaPlayer(player_id);
  if (player)
    player->OnRequestSeek(seek_time);
}

void RendererMediaPlayerManager::OnPlayerSuspend(int player_id,
                                                 bool is_preempted) {
  if (auto* player = GetMediaPlayer(player_id))
    player->OnPlayerSuspend(is_preempted);
}

void RendererMediaPlayerManager::OnPlayerResumed(int player_id,
                                                 bool is_preempted) {
  if (auto* player = GetMediaPlayer(player_id))
    player->OnPlayerResumed(is_preempted);
}

void RendererMediaPlayerManager::OnReadyStateChange(
    int player_id,
    blink::WebMediaPlayer::ReadyState state) {
  auto* player = GetMediaPlayer(player_id);
  if (player)
    player->SetReadyState(
        static_cast<blink::WebMediaPlayer::ReadyState>(state));
}

void RendererMediaPlayerManager::OnNetworkStateChange(
    int player_id,
    blink::WebMediaPlayer::NetworkState state) {
  auto* player = GetMediaPlayer(player_id);
  if (player)
    player->SetNetworkState(
        static_cast<blink::WebMediaPlayer::NetworkState>(state));
}

void RendererMediaPlayerManager::OnPlayerDestroyed(int player_id) {
  auto* player = GetMediaPlayer(player_id);
  if (player)
    player->OnPlayerDestroyed();
}

void RendererMediaPlayerManager::DestroyPlayer(int player_id) {
  Send(new MediaPlayerCastanetsHostMsg_DeInit(routing_id(), player_id));
}

int RendererMediaPlayerManager::RegisterMediaPlayer(
    blink::WebMediaPlayer* player) {
  media_players_[next_media_player_id_] = player;
  return next_media_player_id_++;
}

void RendererMediaPlayerManager::UnregisterMediaPlayer(int player_id) {
  media_players_.erase(player_id);
}

void RendererMediaPlayerManager::EnteredFullscreen(int player_id) {
  Send(new MediaPlayerCastanetsHostMsg_EnteredFullscreen(routing_id(),
                                                         player_id));
}

void RendererMediaPlayerManager::ExitedFullscreen(int player_id) {
  Send(new MediaPlayerCastanetsHostMsg_ExitedFullscreen(routing_id(),
                                                        player_id));
}

#if defined(VIDEO_HOLE)
void RendererMediaPlayerManager::SetMediaGeometry(int player_id,
                                                  const gfx::RectF& rect) {
  gfx::RectF video_rect = rect;
  Send(new MediaPlayerCastanetsHostMsg_SetGeometry(routing_id(), player_id,
                                                   video_rect));
}
#endif

}  // namespace content
