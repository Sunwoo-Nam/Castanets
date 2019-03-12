// Copyright 2019 Samsung Electronics Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CASTANETS_WEBMEDIAPLAYER_CASTANETS_H_
#define CONTENT_RENDERER_MEDIA_CASTANETS_WEBMEDIAPLAYER_CASTANETS_H_

#include <vector>

#include "base/time/time.h"
#include "components/viz/common/gpu/context_provider.h"
#include "content/renderer/media/castanets/media_source_delegate_castanets.h"
#include "content/renderer/media/castanets/renderer_media_player_manager_castanets.h"
#include "media/blink/media_blink_export.h"
#include "media/blink/renderer_media_player_interface.h"
#include "media/blink/video_frame_compositor.h"
#include "media/blink/webmediaplayer_delegate.h"
#include "media/blink/webmediaplayer_params.h"
#include "media/blink/webmediaplayer_util.h"
#include "media/renderers/skcanvas_video_renderer.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerEncryptedMediaClient.h"

namespace blink {
class WebLocalFrame;
class WebMediaPlayerClient;
class WebMediaPlayerEncryptedMediaClient;
}  // namespace blink

namespace cc_blink {
class WebLayerImpl;
}  // namespace cc_blink

namespace media {
class WebMediaPlayerDelegate;
class WebMediaPlayerEncryptedMediaClient;
}  // namespace media

namespace content {
class RendererMediaPlayerManager;

enum MediaType {
  Video = 0x1,
  Audio = Video << 1,
  Text = Video << 2,
  Neither = Video << 3,
};

// This class implements blink::WebMediaPlayer by keeping the Castanets
// media player in the browser process. It listens to all the status changes
// sent from the browser process and sends playback controls to the media
// player.
class MEDIA_BLINK_EXPORT WebMediaPlayerCastanets
    : public blink::WebMediaPlayer,
      public base::SupportsWeakPtr<WebMediaPlayerCastanets>,
      public media::WebMediaPlayerDelegate::Observer {
 public:
  // Construct a WebMediaPlayerCastanets object. This class communicates
  // with the WebMediaPlayerCastanets object in the browser process through
  // |proxy|.
  WebMediaPlayerCastanets(
      blink::WebLocalFrame* frame,
      media::RendererMediaPlayerManagerInterface* manager,
      blink::WebMediaPlayerClient* client,
      blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
      base::WeakPtr<media::WebMediaPlayerDelegate> delegate,
      std::unique_ptr<media::WebMediaPlayerParams> params,
      bool video_hole);
  ~WebMediaPlayerCastanets() override;

  void Load(LoadType load_type,
            const blink::WebMediaPlayerSource& source,
            CORSMode cors_mode) override;

  // Playback controls.
  void Play() override;
  void Pause() override;
  bool SupportsSave() const override;
  void Seek(double seconds) override;
  void SetRate(double rate) override;
  void SetVolume(double volume) override;

  void SetPreload(blink::WebMediaPlayer::Preload preload) override{};
  blink::WebTimeRanges Buffered() const override;
  blink::WebTimeRanges Seekable() const override;

  void SetSinkId(const blink::WebString& sink_id,
                 const blink::WebSecurityOrigin& security_origin,
                 blink::WebSetSinkIdCallbacks* web_callback) override{};

  // True if the loaded media has a playable video/audio track.
  bool HasVideo() const override;
  bool HasAudio() const override;

  // Dimensions of the video.
  blink::WebSize NaturalSize() const override;
  blink::WebSize VisibleRect() const override;

  // Getters of playback state.
  bool Paused() const override;
  bool Seeking() const override;
  double Duration() const override;
  double CurrentTime() const override;
  void Suspend() {}
  void Resume() {}

  // Internal states of loading and network.
  blink::WebMediaPlayer::NetworkState GetNetworkState() const override;
  blink::WebMediaPlayer::ReadyState GetReadyState() const override;

  blink::WebString GetErrorMessage() const override;
  bool DidLoadingProgress() override;

  bool HasSingleSecurityOrigin() const override;
  bool DidPassCORSAccessCheck() const override;

  double MediaTimeForTimeValue(double timeValue) const override;

  unsigned DecodedFrameCount() const override;
  unsigned DroppedFrameCount() const override;
  size_t AudioDecodedByteCount() const override;
  size_t VideoDecodedByteCount() const override;

  void Paint(blink::WebCanvas* canvas,
             const blink::WebRect& rect,
             cc::PaintFlags& flags,
             int already_uploaded_id,
             VideoFrameUploadMetadata* out_metadata) override;

  void EnteredFullscreen() override;
  void ExitedFullscreen() override;

  // WebMediaPlayerDelegate::Observer implementation.
  void OnFrameHidden() override;
  void OnFrameClosed() override;
  void OnFrameShown() override;
  void OnIdleTimeout() override {}
  void OnPlay() override;
  void OnPause() override;
  void OnVolumeMultiplierUpdate(double multiplier) override;
  void OnBecamePersistentVideo(bool value) override{};

  void OnMediaDataChange(int width, int height, int media) override;
  void OnDurationChange(base::TimeDelta duration) override;
  void OnTimeUpdate(base::TimeDelta current_time) override;
  void OnBufferUpdate(int percentage) override;
  void OnTimeChanged() override;
  void OnPauseStateChange(bool state) override;
  void OnSeekComplete() override;
  void OnRequestSeek(base::TimeDelta seek_time) override;
  void OnPlayerSuspend(bool is_preempted) override;
  void OnPlayerResumed(bool is_preempted) override;

  void OnMediaSourceOpened(blink::WebMediaSource* web_media_source);
  void OnEncryptedMediaInitData(media::EmeInitDataType init_data_type,
                                const std::vector<uint8_t>& init_data);
  void SetCdmReadyCB(
      const MediaSourceDelegateCastanets::CdmReadyCB& cdm_ready_cb);
  void OnWaitingForDecryptionKey();

  // Called whenever there is new frame to be painted.
  void FrameReady(const scoped_refptr<media::VideoFrame>& frame);

#if defined(VIDEO_HOLE)
  void OnDrawableContentRectChanged(gfx::Rect rect, bool is_video);
  void CreateVideoHoleFrame();
  // Calculate the boundary rectangle of the media player (i.e. location and
  // size of the video frame).
  // Returns true if the geometry has been changed since the last call.
  bool UpdateBoundaryRectangle();
  gfx::RectF GetBoundaryRectangle() const;
  void StartLayerBoundUpdateTimer();
  void StopLayerBoundUpdateTimer();
  void OnLayerBoundUpdateTimerFired();
  bool ShouldCreateVideoHoleFrame() const;
#endif

 private:
  // Called after |defer_load_cb_| has decided to allow the load. If
  // |defer_load_cb_| is null this is called immediately.
  void DoLoad(LoadType load_type, const blink::WebURL& url);

  void SetReadyState(WebMediaPlayer::ReadyState state) override;
  void SetNetworkState(WebMediaPlayer::NetworkState state) override;

  // Returns the current video frame from |compositor_|. Blocks until the
  // compositor can return the frame.
  scoped_refptr<media::VideoFrame> GetCurrentFrameFromCompositor() const;

  blink::WebLocalFrame* frame_;

  blink::WebMediaPlayer::NetworkState network_state_;
  blink::WebMediaPlayer::ReadyState ready_state_;

  // Message loops for posting tasks on Chrome's main thread. Also used
  // for DCHECKs so methods calls won't execute in the wrong thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // Manager for managing this object and for delegating method calls on
  // Render Thread.
  media::RendererMediaPlayerManagerInterface* manager_;

  blink::WebMediaPlayerClient* client_;
  blink::WebMediaPlayerEncryptedMediaClient* encrypted_client_;

  std::unique_ptr<media::MediaLog> media_log_;

  base::WeakPtr<media::WebMediaPlayerDelegate> delegate_;

  media::WebMediaPlayerParams::DeferLoadCB defer_load_cb_;
  scoped_refptr<viz::ContextProvider> context_provider_;

  // Video rendering members.
  // The |compositor_| runs on the compositor thread, or if
  // kEnableSurfaceLayerForVideo is enabled, the media thread. This task runner
  // posts tasks for the |compositor_| on the correct thread.
  scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner_;

  // Deleted on |compositor_task_runner_|.
  std::unique_ptr<media::VideoFrameCompositor> compositor_;
  media::SkCanvasVideoRenderer skcanvas_video_renderer_;

  // The compositor layer for displaying the video content when using composited
  // playback.
  std::unique_ptr<cc_blink::WebLayerImpl> video_weblayer_;

  std::unique_ptr<content::MediaSourceDelegateCastanets> media_source_delegate_;
  MediaPlayerHostMsg_Initialize_Type player_type_;

  // References the CDM while player is alive (keeps cdm_context_ valid)
  // scoped_refptr<media::ContentDecryptionModule> pending_cdm_;

  // Player ID assigned by the |manager_|.
  int player_id_;

  int video_width_;
  int video_height_;

  bool audio_;
  bool video_;

  base::TimeDelta current_time_;
  base::TimeDelta duration_;
  bool is_paused_;

  bool is_seeking_;
  base::TimeDelta seek_time_;
  bool pending_seek_;
  base::TimeDelta pending_seek_time_;

  // Whether the video is known to be opaque or not.
  bool opaque_;
  bool is_fullscreen_;

#if defined(VIDEO_HOLE)
  bool is_draw_ready_;
  bool pending_play_;
  bool is_video_hole_;

  // A rectangle represents the geometry of video frame, when computed last
  // time.
  gfx::RectF last_computed_rect_;
  base::RepeatingTimer layer_bound_update_timer_;
#endif

  gfx::Size natural_size_;
  blink::WebTimeRanges buffered_;
  mutable bool did_loading_progress_;
  int delegate_id_;

  // The last volume received by setVolume() and the last volume multiplier from
  // OnVolumeMultiplierUpdate(). The multiplier is typical 1.0, but may be less
  // if the WebMediaPlayerDelegate has requested a volume reduction (ducking)
  // for a transient sound.  Playout volume is derived by volume * multiplier.
  double volume_;
  double volume_multiplier_;

  media::CdmContext* cdm_context_;
  media::EmeInitDataType init_data_type_;
  MediaSourceDelegateCastanets::CdmReadyCB pending_cdm_ready_cb_;

  base::WeakPtrFactory<WebMediaPlayerCastanets> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebMediaPlayerCastanets);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CASTANETS_WEBMEDIAPLAYER_CASTANETS_H_
