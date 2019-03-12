// Copyright 2019 Samsung Electronics Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_MEDIA_MEDIA_PLAYER_MESSAGES_CASTANETS_H_
#define CONTENT_COMMON_MEDIA_MEDIA_PLAYER_MESSAGES_CASTANETS_H_

// IPC messages for castanets media player.
// Multiply-included message file, hence no include guard.

#include "base/time/time.h"
#include "content/common/content_export.h"
#include "content/common/media/media_param_traits_castanets.h"
#include "content/common/media/media_player_init_config.h"
#include "ipc/ipc_message_macros.h"
#include "media/base/castanets/demuxer_stream_player_params_castanets.h"
#include "media/base/ipc/media_param_traits_macros.h"
#include "media/base/ranges.h"
#include "media/blink/renderer_media_player_interface.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"

#undef IPC_MESSAGE_EXPORT
#define IPC_MESSAGE_EXPORT CONTENT_EXPORT
#define IPC_MESSAGE_START MediaPlayerMsgStart

IPC_ENUM_TRAITS(blink::WebMediaPlayer::ReadyState)
IPC_ENUM_TRAITS(blink::WebMediaPlayer::NetworkState)

IPC_STRUCT_TRAITS_BEGIN(content::MediaPlayerInitConfig)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(url)
  IPC_STRUCT_TRAITS_MEMBER(mime_type)
  IPC_STRUCT_TRAITS_MEMBER(demuxer_client_id)
  IPC_STRUCT_TRAITS_MEMBER(has_encrypted_listener_or_cdm)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::DemuxerConfigs)
  IPC_STRUCT_TRAITS_MEMBER(audio_codec)
  IPC_STRUCT_TRAITS_MEMBER(audio_channels)
  IPC_STRUCT_TRAITS_MEMBER(audio_sampling_rate)
  IPC_STRUCT_TRAITS_MEMBER(audio_bit_rate)
  IPC_STRUCT_TRAITS_MEMBER(is_audio_encrypted)
  IPC_STRUCT_TRAITS_MEMBER(audio_extra_data)

  IPC_STRUCT_TRAITS_MEMBER(video_codec)
  IPC_STRUCT_TRAITS_MEMBER(video_size)
  IPC_STRUCT_TRAITS_MEMBER(is_video_encrypted)
  IPC_STRUCT_TRAITS_MEMBER(video_extra_data)
  //  For TIZEN TV
  IPC_STRUCT_TRAITS_MEMBER(webm_hdr_info)
  IPC_STRUCT_TRAITS_MEMBER(framerate_num)
  IPC_STRUCT_TRAITS_MEMBER(framerate_den)
  IPC_STRUCT_TRAITS_MEMBER(is_framerate_changed)

  IPC_STRUCT_TRAITS_MEMBER(duration_ms)
IPC_STRUCT_TRAITS_END()

IPC_STRUCT_TRAITS_BEGIN(media::DemuxedBufferMetaData)
  IPC_STRUCT_TRAITS_MEMBER(size)
  IPC_STRUCT_TRAITS_MEMBER(end_of_stream)
  IPC_STRUCT_TRAITS_MEMBER(timestamp)
  IPC_STRUCT_TRAITS_MEMBER(time_duration)
  IPC_STRUCT_TRAITS_MEMBER(type)
  IPC_STRUCT_TRAITS_MEMBER(status)
  //  For TIZEN TV
  IPC_STRUCT_TRAITS_MEMBER(tz_handle)
IPC_STRUCT_TRAITS_END()

// Initialize Castanets player.
IPC_MESSAGE_ROUTED2(MediaPlayerCastanetsHostMsg_Init,
                    int /* player_id */,
                    content::MediaPlayerInitConfig /* config */)

// Requests the player to enter fullscreen.
IPC_MESSAGE_ROUTED1(MediaPlayerCastanetsHostMsg_EnteredFullscreen,
                    int /* player_id */)
// Requests the player to exit fullscreen.
IPC_MESSAGE_ROUTED1(MediaPlayerCastanetsHostMsg_ExitedFullscreen,
                    int /* player_id */)

// Initialize Castanets player synchronously with result.
IPC_SYNC_MESSAGE_ROUTED5_1(MediaPlayerCastanetsHostMsg_InitSync,
                           int /* player_id */,
                           MediaPlayerHostMsg_Initialize_Type /* type */,
                           GURL /* URL */,
                           std::string /*mime_type*/,
                           int /* demuxer client id */,
                           bool /* success */)

#if defined(VIDEO_HOLE)
IPC_MESSAGE_ROUTED2(MediaPlayerCastanetsHostMsg_SetGeometry,
                    int /* player_id */,
                    gfx::RectF /* position and size */)
#endif

// Deinitialize Gst player.
IPC_MESSAGE_ROUTED1(MediaPlayerCastanetsHostMsg_DeInit, int /* player_id */)

// Start playback.
IPC_MESSAGE_ROUTED1(MediaPlayerCastanetsHostMsg_Play, int /* player_id */)

// Pause playback.
IPC_MESSAGE_ROUTED2(MediaPlayerCastanetsHostMsg_Pause,
                    int /* player_id */,
                    bool /* is_media_related_action */)

// Suspend media player.
IPC_MESSAGE_ROUTED1(MediaPlayerCastanetsHostMsg_Suspend, int /* player_id */)

// Resume media player.
IPC_MESSAGE_ROUTED1(MediaPlayerCastanetsHostMsg_Resume, int /* player_id*/)

// Player was activated by an user or an app.
IPC_MESSAGE_ROUTED1(MediaPlayerCastanetsHostMsg_Activate, int /* player_id*/)

// Player should deactivate (ex. save power).
IPC_MESSAGE_ROUTED1(MediaPlayerCastanetsHostMsg_Deactivate, int /* player_id*/)

#if defined(TIZEN_VD_MULTIPLE_MIXERDECODER)
// Player should reload
IPC_MESSAGE_ROUTED2(MediaPlayerCastanetsMsg_PrePlayerReload,
                    int /* player_id */,
                    bool /*reload*/)
#endif

// Set volume.
IPC_MESSAGE_ROUTED2(MediaPlayerCastanetsHostMsg_SetVolume,
                    int /* player_id */,
                    double /* volume */)

// Set playback rate.
IPC_MESSAGE_ROUTED2(MediaPlayerCastanetsHostMsg_SetRate,
                    int /* player_id */,
                    double /* rate */)

// Playback duration.
IPC_MESSAGE_ROUTED2(MediaPlayerCastanetsMsg_DurationChanged,
                    int /* player_id */,
                    base::TimeDelta /* time */)

// Current  duration.
IPC_MESSAGE_ROUTED2(MediaPlayerCastanetsMsg_TimeUpdate,
                    int /* player_id */,
                    base::TimeDelta /* time */)

// Pause state.
IPC_MESSAGE_ROUTED2(MediaPlayerCastanetsMsg_PauseStateChanged,
                    int /* player_id */,
                    bool /* state */)

// Seek state.
IPC_MESSAGE_ROUTED1(MediaPlayerCastanetsMsg_OnSeekComplete, int /* player_id */)

// Current buffer range.
IPC_MESSAGE_ROUTED2(MediaPlayerCastanetsMsg_BufferUpdate,
                    int /* player_id */,
                    int /* buffering_percentage */)

// Playback completed.
IPC_MESSAGE_ROUTED1(MediaPlayerCastanetsMsg_TimeChanged, int /* player_id */)

IPC_MESSAGE_ROUTED1(MediaPlayerCastanetsMsg_PlayerDestroyed,
                    int /* player_id */)
// Ready state change.
IPC_MESSAGE_ROUTED2(MediaPlayerCastanetsMsg_ReadyStateChange,
                    int /* player_id */,
                    blink::WebMediaPlayer::ReadyState /* state */)

// Network state change.
IPC_MESSAGE_ROUTED2(MediaPlayerCastanetsMsg_NetworkStateChange,
                    int /* player_id */,
                    blink::WebMediaPlayer::NetworkState /* state */)

// Gst media data has changed.
IPC_MESSAGE_ROUTED4(MediaPlayerCastanetsMsg_MediaDataChanged,
                    int /* player_id */,
                    int /* width */,
                    int /* height */,
                    int /* media */)

IPC_MESSAGE_ROUTED1(MediaPlayerCastanetsMsg_VideoSlotsAvailableChanged,
                    unsigned /* slots_available */)

// On new frame available.
IPC_MESSAGE_ROUTED4(MediaPlayerCastanetsMsg_NewFrameAvailable,
                    int /* player_id */,
                    base::SharedMemoryHandle /* Handle */,
                    uint32_t /* length */,
                    base::TimeDelta /* time stamp */)

// Seek.
IPC_MESSAGE_ROUTED2(MediaPlayerCastanetsHostMsg_Seek,
                    int /* player_id */,
                    base::TimeDelta /* time */)

// For MSE internal seek request.
IPC_MESSAGE_ROUTED2(MediaPlayerCastanetsMsg_SeekRequest,
                    int /* player_id */,
                    base::TimeDelta /* time_to_seek */)

// Player has begun suspend procedure
IPC_MESSAGE_ROUTED2(MediaPlayerCastanetsMsg_PlayerSuspend,
                    int /* player_id */,
                    bool /* is_preempted */)

// Player has resumed
IPC_MESSAGE_ROUTED2(MediaPlayerCastanetsMsg_PlayerResumed,
                    int /* player_id */,
                    bool /* is_preempted */)

// Sent after the renderer demuxer has seeked.
IPC_MESSAGE_CONTROL3(MediaPlayerCastanetsHostMsg_DemuxerSeekDone,
                     int /* demuxer_client_id */,
                     base::TimeDelta /* actual_browser_seek_time */,
                     base::TimeDelta /* video_key_frame */)

// Inform the media source player that the demuxer is ready.
IPC_MESSAGE_CONTROL2(MediaPlayerCastanetsHostMsg_DemuxerReady,
                     int /* demuxer_client_id */,
                     media::DemuxerConfigs /* configs */)

// Sent when the data was read from the ChunkDemuxer.
IPC_MESSAGE_CONTROL3(MediaPlayerCastanetsHostMsg_ReadFromDemuxerAck,
                     int /* demuxer_client_id */,
                     std::vector<unsigned char> /* stream_data */,
                     media::DemuxedBufferMetaData /* meta data of buffer*/)

// Inform the media source player of changed media duration from demuxer.
IPC_MESSAGE_CONTROL2(MediaPlayerCastanetsHostMsg_DurationChanged,
                     int /* demuxer_client_id */,
                     base::TimeDelta /* duration */)

// The media source player reads data from demuxer
IPC_MESSAGE_CONTROL2(MediaPlayerCastanetsMsg_ReadFromDemuxer,
                     int /* demuxer_client_id */,
                     media::DemuxerStream::Type /* type */)

// Requests renderer demuxer seek.
IPC_MESSAGE_CONTROL2(MediaPlayerCastanetsMsg_DemuxerSeekRequest,
                     int /* demuxer_client_id */,
                     base::TimeDelta /* time_to_seek */)

IPC_MESSAGE_CONTROL2(MediaPlayerCastanetsHostMsg_DemuxerBufferedChanged,
                     int /* demuxer_client_id */,
                     media::Ranges<base::TimeDelta> /* buffered */)

// Use sync ipc message to get start date of current media.
IPC_SYNC_MESSAGE_ROUTED1_1(MediaPlayerCastanetsHostMsg_GetStartDate,
                           int /* player_id */,
                           double /* start date */)

#endif  // #ifndef CONTENT_COMMON_MEDIA_MEDIA_PLAYER_MESSAGES_CASTANETS_H_
