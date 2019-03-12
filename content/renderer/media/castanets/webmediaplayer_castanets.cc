// Copyright 2019 Samsung Electronics Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/castanets/webmediaplayer_castanets.h"

#include "cc/blink/web_layer_impl.h"
#include "cc/layers/video_layer.h"
#include "components/viz/common/gpu/context_provider.h"
#include "content/renderer/media/castanets/renderer_media_player_manager_castanets.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/media_content_type.h"
#include "media/blink/webmediaplayer_util.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerClient.h"
#include "third_party/WebKit/public/platform/WebMediaPlayerSource.h"

#if defined(VIDEO_HOLE)
#include "third_party/WebKit/public/web/WebLocalFrame.h"
#include "third_party/WebKit/public/web/WebView.h"
#endif

namespace content {

#define BIND_TO_RENDER_LOOP(function)                   \
  (DCHECK(main_task_runner_->BelongsToCurrentThread()), \
   media::BindToCurrentLoop(base::Bind(function, AsWeakPtr())))

const base::TimeDelta kLayerBoundUpdateInterval =
    base::TimeDelta::FromMilliseconds(50);

GURL GetCleanURL(std::string url) {
  // FIXME: Need to consider "app://" scheme.
  CHECK(url.compare(0, 6, "app://"));
  if (!url.compare(0, 7, "file://")) {
    int position = url.find("?");
    if (position != -1)
      url.erase(url.begin() + position, url.end());
  }
  GURL url_(url);
  return url_;
}

WebMediaPlayerCastanets::WebMediaPlayerCastanets(
    blink::WebLocalFrame* frame,
    media::RendererMediaPlayerManagerInterface* manager,
    blink::WebMediaPlayerClient* client,
    blink::WebMediaPlayerEncryptedMediaClient* encrypted_client,
    base::WeakPtr<media::WebMediaPlayerDelegate> delegate,
    std::unique_ptr<media::WebMediaPlayerParams> params,
    bool video_hole)
    : frame_(frame),
      network_state_(blink::WebMediaPlayer::kNetworkStateEmpty),
      ready_state_(blink::WebMediaPlayer::kReadyStateHaveNothing),
      main_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      media_task_runner_(params->media_task_runner()),
      manager_(manager),
      client_(client),
      encrypted_client_(encrypted_client),
      media_log_(params->take_media_log()),
      delegate_(delegate),
      defer_load_cb_(params->defer_load_cb()),
      context_provider_(params->context_provider()),
      // Threaded compositing isn't enabled universally yet.
      compositor_task_runner_(params->compositor_task_runner()
                                  ? params->compositor_task_runner()
                                  : base::ThreadTaskRunnerHandle::Get()),
      compositor_(base::MakeUnique<media::VideoFrameCompositor>(
#if defined(VIDEO_HOLE)
          BIND_TO_RENDER_LOOP(
              &WebMediaPlayerCastanets::OnDrawableContentRectChanged),
#endif
          compositor_task_runner_,
          params->context_provider_callback())),
      player_type_(MEDIA_PLAYER_TYPE_NONE),
      video_width_(0),
      video_height_(0),
      audio_(false),
      video_(false),
      is_paused_(true),
      is_seeking_(false),
      pending_seek_(false),
      opaque_(false),
      is_fullscreen_(false),
#if defined(VIDEO_HOLE)
      is_draw_ready_(false),
      pending_play_(false),
      is_video_hole_(video_hole),
#endif
      natural_size_(0, 0),
      buffered_(static_cast<size_t>(1)),
      did_loading_progress_(false),
      delegate_id_(0),
      volume_(1.0),
      volume_multiplier_(1.0),
      init_data_type_(media::EmeInitDataType::UNKNOWN),
      weak_factory_(this) {
  if (delegate_) {
    delegate_id_ = delegate_->AddObserver(this);
    delegate_->SetIdle(delegate_id_, true);
  }
  player_id_ = manager_->RegisterMediaPlayer(this);
  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_CREATED));
}

WebMediaPlayerCastanets::~WebMediaPlayerCastanets() {
  if (manager_) {
    manager_->DestroyPlayer(player_id_);
    manager_->UnregisterMediaPlayer(player_id_);
  }

  client_->SetWebLayer(NULL);

  if (delegate_) {
    delegate_->PlayerGone(delegate_id_);
    delegate_->RemoveObserver(delegate_id_);
  }

  compositor_task_runner_->DeleteSoon(FROM_HERE, std::move(compositor_));
  if (media_source_delegate_) {
    // Part of |media_source_delegate_| needs to be stopped on the media thread.
    // Wait until |media_source_delegate_| is fully stopped before tearing
    // down other objects.
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    media_source_delegate_->Stop(
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&waiter)));
    waiter.Wait();
  }
  media_log_->AddEvent(
      media_log_->CreateEvent(media::MediaLogEvent::WEBMEDIAPLAYER_DESTROYED));
}

void WebMediaPlayerCastanets::Load(LoadType load_type,
                                   const blink::WebMediaPlayerSource& source,
                                   CORSMode /* cors_mode */) {
  // Only URL or MSE blob URL is supported.
  DCHECK(source.IsURL());
  blink::WebURL url = source.GetAsURL();
  if (!defer_load_cb_.is_null()) {
    defer_load_cb_.Run(base::Bind(&WebMediaPlayerCastanets::DoLoad, AsWeakPtr(),
                                  load_type, url));
    return;
  }
  DoLoad(load_type, url);
}

void WebMediaPlayerCastanets::DoLoad(LoadType load_type,
                                     const blink::WebURL& url) {
  int demuxer_client_id = 0;
  blink::WebString content_mime_type =
      blink::WebString(client_->GetContentMIMEType());
  if (load_type == kLoadTypeMediaSource) {
    // FIXME: MediaSourceExtension for Castanets will be implemented.
    player_type_ = MEDIA_PLAYER_TYPE_MEDIA_SOURCE;
    content::RendererDemuxerCastanets* demuxer =
        static_cast<content::RendererDemuxerCastanets*>(
            content::RenderThreadImpl::current()->renderer_demuxer());
    demuxer_client_id = demuxer->GetNextDemuxerClientID();
    media_source_delegate_.reset(new content::MediaSourceDelegateCastanets(
        demuxer, demuxer_client_id, media_task_runner_, media_log_.get()));
    media_source_delegate_->InitializeMediaSource(
        base::Bind(&WebMediaPlayerCastanets::OnMediaSourceOpened,
                   weak_factory_.GetWeakPtr()),
        base::Bind(&WebMediaPlayerCastanets::OnEncryptedMediaInitData,
                   weak_factory_.GetWeakPtr()),
        base::Bind(&WebMediaPlayerCastanets::SetCdmReadyCB,
                   weak_factory_.GetWeakPtr()),
        base::Bind(&WebMediaPlayerCastanets::SetNetworkState,
                   weak_factory_.GetWeakPtr()),
        base::Bind(&WebMediaPlayerCastanets::OnDurationChange,
                   weak_factory_.GetWeakPtr()),
        base::Bind(&WebMediaPlayerCastanets::OnWaitingForDecryptionKey,
                   weak_factory_.GetWeakPtr()));
  } else if (load_type == kLoadTypeURL) {
    player_type_ = MEDIA_PLAYER_TYPE_URL;
  } else {
    LOG(ERROR) << "Unsupported load type : " << load_type;
    return;
  }
#if defined(VIDEO_HOLE)
  LOG(INFO) << "Video Hole : " << is_video_hole_;
  if (is_video_hole_)
    player_type_ = (player_type_ == MEDIA_PLAYER_TYPE_URL)
                       ? MEDIA_PLAYER_TYPE_URL_WITH_VIDEO_HOLE
                       : MEDIA_PLAYER_TYPE_MEDIA_SOURCE_WITH_VIDEO_HOLE;
#endif
  GURL gurl(url);
  manager_->Initialize(player_id_, player_type_,
                       GetCleanURL(gurl.spec().c_str()),
                       content_mime_type.Utf8(), demuxer_client_id);

  if (delegate_->IsFrameHidden())
    Suspend();
}

void WebMediaPlayerCastanets::Play() {
  LOG(INFO) << "[" << player_id_ << "] " << __FUNCTION__;
#if defined(VIDEO_HOLE)
  if (is_video_hole_) {
    if (HasVideo() && !is_draw_ready_) {
      pending_play_ = true;
      return;
    }
    pending_play_ = false;
  }
#endif

  manager_->Start(player_id_);
  // Has to be updated from |MediaPlayerCastanets| but IPC causes delay.
  // There are cases were play - pause are fired successively and would fail.
  OnPauseStateChange(false);
}

void WebMediaPlayerCastanets::Pause() {
  LOG(INFO) << "[" << player_id_ << "] " << __FUNCTION__;
#if defined(VIDEO_HOLE)
  if (is_video_hole_)
    pending_play_ = false;
#endif
  manager_->Pause(player_id_, false);
  // Has to be updated from |MediaPlayerCastanets| but IPC causes delay.
  // There are cases were play - pause are fired successively and would fail.
  OnPauseStateChange(true);
}

bool WebMediaPlayerCastanets::SupportsSave() const {
  return false;
}

void WebMediaPlayerCastanets::Seek(double seconds) {
  LOG(INFO) << "WebMediaPlayerCastanets::seek() seconds : " << seconds;
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  base::TimeDelta new_seek_time = base::TimeDelta::FromSecondsD(seconds);

  // new_seek_time.SetResolution(base::TimeDelta::Resolution::MILLISECONDS);
  if (is_seeking_) {
    if (new_seek_time == seek_time_) {
      if (media_source_delegate_) {
        // Don't suppress any redundant in-progress MSE seek. There could have
        // been changes to the underlying buffers after seeking the demuxer and
        // before receiving OnSeekComplete() for currently in-progress seek.
        LOG(INFO) << "Detected MediaSource seek to same time as to : "
                  << seek_time_;
      } else {
        // Suppress all redundant seeks if unrestricted by media source
        // demuxer API.
        pending_seek_ = false;
        return;
      }
    }

    pending_seek_ = true;
    pending_seek_time_ = new_seek_time;
    if (media_source_delegate_)
      media_source_delegate_->CancelPendingSeek(pending_seek_time_);
    // Later, OnSeekComplete will trigger the pending seek.
    return;
  }

  is_seeking_ = true;
  seek_time_ = new_seek_time;

  // Once Chunk demuxer seeks |MediaPlayerCastanets| seek will be intiated.
  if (media_source_delegate_)
    media_source_delegate_->StartWaitingForSeek(seek_time_);
  manager_->Seek(player_id_, seek_time_);
}

void WebMediaPlayerCastanets::SetRate(double rate) {
  manager_->SetRate(player_id_, rate);
}

void WebMediaPlayerCastanets::SetVolume(double volume) {
  manager_->SetVolume(player_id_, volume);
}

blink::WebTimeRanges WebMediaPlayerCastanets::Buffered() const {
  return buffered_;
}

blink::WebTimeRanges WebMediaPlayerCastanets::Seekable() const {
  if (ready_state_ < WebMediaPlayer::kReadyStateHaveMetadata)
    return blink::WebTimeRanges();

  const blink::WebTimeRange seekable_range(0.0, Duration());
  return blink::WebTimeRanges(&seekable_range, 1);
}

bool WebMediaPlayerCastanets::HasVideo() const {
  return true;
}

bool WebMediaPlayerCastanets::HasAudio() const {
  return true;
}

blink::WebSize WebMediaPlayerCastanets::NaturalSize() const {
  return blink::WebSize(natural_size_);
}

blink::WebSize WebMediaPlayerCastanets::VisibleRect() const {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  return blink::WebSize();
  scoped_refptr<media::VideoFrame> video_frame =
      GetCurrentFrameFromCompositor();
  if (!video_frame)
    return blink::WebSize();

  const gfx::Rect& visible_rect = video_frame->visible_rect();
  return blink::WebSize(visible_rect.width(), visible_rect.height());
}

bool WebMediaPlayerCastanets::Paused() const {
  return is_paused_;
}

bool WebMediaPlayerCastanets::Seeking() const {
  return is_seeking_;
}

double WebMediaPlayerCastanets::Duration() const {
  return duration_.InSecondsF();
}

double WebMediaPlayerCastanets::CurrentTime() const {
  if (Seeking())
    return pending_seek_ ? pending_seek_time_.InSecondsF()
                         : seek_time_.InSecondsF();
  return current_time_.InSecondsF();
}

blink::WebMediaPlayer::NetworkState WebMediaPlayerCastanets::GetNetworkState()
    const {
  return network_state_;
}

blink::WebMediaPlayer::ReadyState WebMediaPlayerCastanets::GetReadyState()
    const {
  return ready_state_;
}

blink::WebString WebMediaPlayerCastanets::GetErrorMessage() const {
  return blink::WebString::FromUTF8(media_log_->GetErrorMessage());
}

bool WebMediaPlayerCastanets::DidLoadingProgress() {
  if (did_loading_progress_) {
    did_loading_progress_ = false;
    return true;
  }
  return false;
}

void WebMediaPlayerCastanets::SetReadyState(
    blink::WebMediaPlayer::ReadyState state) {
  ready_state_ = state;
  client_->ReadyStateChanged();
}

void WebMediaPlayerCastanets::SetNetworkState(
    blink::WebMediaPlayer::NetworkState state) {
  network_state_ = state;
  client_->NetworkStateChanged();
}

static void GetCurrentFrameAndSignal(
    media::VideoFrameCompositor* compositor,
    scoped_refptr<media::VideoFrame>* video_frame_out,
    base::WaitableEvent* event) {
  *video_frame_out = compositor->GetCurrentFrame();
  event->Signal();
}

scoped_refptr<media::VideoFrame>
WebMediaPlayerCastanets::GetCurrentFrameFromCompositor() const {
  if (compositor_task_runner_->BelongsToCurrentThread()) {
    scoped_refptr<media::VideoFrame> video_frame =
        compositor_->GetCurrentFrameAndUpdateIfStale();
    if (!video_frame) {
      return nullptr;
    }

    return video_frame;
  }

  // Use a posted task and waitable event instead of a lock otherwise
  // WebGL/Canvas can see different content than what the compositor is seeing.
  scoped_refptr<media::VideoFrame> video_frame;
  base::WaitableEvent event(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                            base::WaitableEvent::InitialState::NOT_SIGNALED);
  compositor_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&GetCurrentFrameAndSignal, base::Unretained(compositor_.get()),
                 &video_frame, &event));
  event.Wait();
  if (!video_frame) {
    return nullptr;
  }

  return video_frame;
}

bool WebMediaPlayerCastanets::HasSingleSecurityOrigin() const {
  return true;
}

bool WebMediaPlayerCastanets::DidPassCORSAccessCheck() const {
  return false;
}

double WebMediaPlayerCastanets::MediaTimeForTimeValue(double timeValue) const {
  return base::TimeDelta::FromSecondsD(timeValue).InSecondsF();
}

unsigned WebMediaPlayerCastanets::DecodedFrameCount() const {
  return 0;
}

unsigned WebMediaPlayerCastanets::DroppedFrameCount() const {
  return 0;
}

size_t WebMediaPlayerCastanets::AudioDecodedByteCount() const {
  return 0;
}

size_t WebMediaPlayerCastanets::VideoDecodedByteCount() const {
  return 0;
}

void WebMediaPlayerCastanets::Paint(blink::WebCanvas* canvas,
                                    const blink::WebRect& rect,
                                    cc::PaintFlags& flags,
                                    int already_uploaded_id,
                                    VideoFrameUploadMetadata* out_metadata) {
  scoped_refptr<media::VideoFrame> video_frame =
      GetCurrentFrameFromCompositor();
  if (!video_frame.get())
    return;

  media::Context3D context_3d;
  if (video_frame->HasTextures()) {
    if (context_provider_) {
      context_3d = media::Context3D(context_provider_->ContextGL(),
                                    context_provider_->GrContext());
    }
    if (!context_3d.gl)
      return;  // Unable to get/create a shared main thread context.
    if (!context_3d.gr_context)
      return;  // The context has been lost since and can't setup a GrContext.
  }

  gfx::RectF gfx_rect(rect);
  skcanvas_video_renderer_.Paint(video_frame, canvas, gfx_rect, flags,
                                 media::VIDEO_ROTATION_0, context_3d);
}

void WebMediaPlayerCastanets::EnteredFullscreen() {
  if (is_fullscreen_)
    return;

  is_fullscreen_ = true;
#if defined(VIDEO_HOLE)
  manager_->EnteredFullscreen(player_id_);
  if (HasVideo() && is_video_hole_) {
    CreateVideoHoleFrame();
  }
#endif
}

void WebMediaPlayerCastanets::ExitedFullscreen() {
  if (!is_fullscreen_)
    return;

  is_fullscreen_ = false;
#if defined(VIDEO_HOLE)
  if (HasVideo() && is_video_hole_) {
    gfx::Size size(video_width_, video_height_);
    scoped_refptr<media::VideoFrame> video_frame =
        media::VideoFrame::CreateBlackFrame(size);
    FrameReady(video_frame);
  }

  manager_->ExitedFullscreen(player_id_);
  client_->Repaint();
#endif
}

void WebMediaPlayerCastanets::OnFrameHidden() {
  LOG(INFO) << "[" << player_id_ << "] " << __FUNCTION__;
  Suspend();
}

void WebMediaPlayerCastanets::OnFrameClosed() {
  LOG(INFO) << "[" << player_id_ << "] " << __FUNCTION__;
  Suspend();
}

void WebMediaPlayerCastanets::OnFrameShown() {
  LOG(INFO) << "[" << player_id_ << "] " << __FUNCTION__;
  Resume();
}

void WebMediaPlayerCastanets::OnPlay() {
  LOG(INFO) << "[" << player_id_ << "] " << __FUNCTION__;
  Play();
}

void WebMediaPlayerCastanets::OnPause() {
  LOG(INFO) << "[" << player_id_ << "] " << __FUNCTION__;
  Pause();
}

void WebMediaPlayerCastanets::OnVolumeMultiplierUpdate(double multiplier) {
  volume_multiplier_ = multiplier;
  SetVolume(volume_);
}

void WebMediaPlayerCastanets::OnMediaDataChange(int width,
                                                int height,
                                                int media) {
  video_height_ = height;
  video_width_ = width;
  //  audio_ = media::MediaTypeFlags(media).MediaType::Audio
  //  video_ = media::MediaTypeFlags(media).test(media::MediaType::Video);
  audio_ = media & MediaType::Audio;
  video_ = media & MediaType::Video;
  natural_size_ = gfx::Size(width, height);
  if (HasVideo() && !video_weblayer_) {
    video_weblayer_.reset(new cc_blink::WebLayerImpl(
        cc::VideoLayer::Create(compositor_.get(), media::VIDEO_ROTATION_0)));
    video_weblayer_->layer()->SetContentsOpaque(opaque_);
    video_weblayer_->SetContentsOpaqueIsFixed(true);
    client_->SetWebLayer(video_weblayer_.get());
  }
#if defined(VIDEO_HOLE)
  if (ShouldCreateVideoHoleFrame()) {
    CreateVideoHoleFrame();
    StartLayerBoundUpdateTimer();
  }
#endif
}

void WebMediaPlayerCastanets::OnDurationChange(base::TimeDelta duration) {
  duration_ = duration;

  client_->DurationChanged();
}

void WebMediaPlayerCastanets::OnTimeUpdate(base::TimeDelta current_time) {
  current_time_ = current_time;
}

void WebMediaPlayerCastanets::OnBufferUpdate(int percentage) {
  buffered_[0].end = Duration() * percentage / 100;
  did_loading_progress_ = true;
}

void WebMediaPlayerCastanets::OnTimeChanged() {
  client_->TimeChanged();
}

void WebMediaPlayerCastanets::OnPauseStateChange(bool state) {
  if (is_paused_ == state)
    return;

  is_paused_ = state;
  client_->PlaybackStateChanged();
  if (!delegate_)
    return;

  if (is_paused_) {
    delegate_->DidPause(delegate_id_);
  } else {
    delegate_->DidPlay(delegate_id_, HasVideo(), HasAudio(),
                       media::DurationToMediaContentType(duration_));
  }
}

void WebMediaPlayerCastanets::OnSeekComplete() {
  LOG(INFO) << "Seek completed to " << seek_time_.InSecondsF();

  is_seeking_ = false;
  seek_time_ = base::TimeDelta();

  // Handling pending seek for ME. For MSE |CancelPendingSeek|
  // will handle the pending seeks.
  if (pending_seek_) {
    pending_seek_ = false;
    Seek(pending_seek_time_.InSecondsF());
    pending_seek_time_ = base::TimeDelta();
    return;
  }
#if defined(VIDEO_HOLE)
  if (ShouldCreateVideoHoleFrame())
    CreateVideoHoleFrame();
#endif
  client_->TimeChanged();
}

void WebMediaPlayerCastanets::OnRequestSeek(base::TimeDelta seek_time) {
  client_->RequestSeek(seek_time.InSecondsF());
}

void WebMediaPlayerCastanets::OnPlayerSuspend(bool is_preempted) {
  if (!is_paused_ && is_preempted) {
    OnPauseStateChange(true);
  }

  if (!delegate_)
    return;
  delegate_->PlayerGone(delegate_id_);
}

void WebMediaPlayerCastanets::OnPlayerResumed(bool is_preempted) {
  if (!delegate_)
    return;

  if (is_paused_)
    delegate_->DidPause(delegate_id_);
  else
    delegate_->DidPlay(delegate_id_, HasVideo(), HasAudio(),
                       media::DurationToMediaContentType(duration_));
}

void WebMediaPlayerCastanets::OnMediaSourceOpened(
    blink::WebMediaSource* web_media_source) {
  DCHECK(client_);
  client_->MediaSourceOpened(web_media_source);
}

void WebMediaPlayerCastanets::OnEncryptedMediaInitData(
    media::EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  LOG_IF(WARNING, init_data_type_ != media::EmeInitDataType::UNKNOWN &&
                      init_data_type != init_data_type_)
      << "Mixed init data type not supported. The new type is ignored.";
  if (init_data_type_ == media::EmeInitDataType::UNKNOWN)
    init_data_type_ = init_data_type;

  encrypted_client_->Encrypted(ConvertToWebInitDataType(init_data_type),
                               init_data.data(), init_data.size());
}

void WebMediaPlayerCastanets::SetCdmReadyCB(
    const MediaSourceDelegateCastanets::CdmReadyCB& cdm_ready_cb) {
  DCHECK(main_task_runner_->BelongsToCurrentThread());
  LOG(INFO) << "Cdm context " << cdm_context_;
  if (cdm_context_) {
    cdm_ready_cb.Run(cdm_context_, base::Bind(&media::IgnoreCdmAttached));
  } else {
    LOG(INFO) << "Setting pending_cdm_ready_cb_";
    pending_cdm_ready_cb_ = cdm_ready_cb;
  }
}

void WebMediaPlayerCastanets::OnWaitingForDecryptionKey() {
  encrypted_client_->DidBlockPlaybackWaitingForKey();

  // TODO(jrummell): didResumePlaybackBlockedForKey() should only be called
  // when a key has been successfully added (e.g. OnSessionKeysChange() with
  // |has_additional_usable_key| = true). http://crbug.com/461903
  encrypted_client_->DidResumePlaybackBlockedForKey();
}

void WebMediaPlayerCastanets::FrameReady(
    const scoped_refptr<media::VideoFrame>& frame) {
  compositor_->PaintSingleFrame(frame);
}

#if defined(VIDEO_HOLE)
void WebMediaPlayerCastanets::CreateVideoHoleFrame() {
  gfx::Size size(video_width_, video_height_);

  scoped_refptr<media::VideoFrame> video_frame =
      media::VideoFrame::CreateHoleFrame(size);
  if (video_frame)
    FrameReady(video_frame);
}

void WebMediaPlayerCastanets::OnDrawableContentRectChanged(gfx::Rect rect,
                                                           bool is_video) {
  LOG(INFO) << "SetMediaGeometry: " << rect.ToString() << ", " << __FUNCTION__;
  is_draw_ready_ = true;

  StopLayerBoundUpdateTimer();
  gfx::RectF rect_f = static_cast<gfx::RectF>(rect);
  if (manager_)
    manager_->SetMediaGeometry(player_id_, rect_f);

  if (pending_play_)
    Play();
}

bool WebMediaPlayerCastanets::UpdateBoundaryRectangle() {
  if (!video_weblayer_)
    return false;

  // Compute the geometry of video frame layer.
  cc::Layer* layer = video_weblayer_->layer();
  gfx::RectF rect(gfx::SizeF(layer->bounds()));
  while (layer) {
    rect.Offset(layer->position().OffsetFromOrigin());
    rect.Offset(layer->scroll_offset().x() * (-1),
                layer->scroll_offset().y() * (-1));
    layer = layer->parent();
  }

  rect.Scale(frame_->View()->PageScaleFactor());

  // Return false when the geometry hasn't been changed from the last time.
  if (last_computed_rect_ == rect)
    return false;

  // Store the changed geometry information when it is actually changed.
  last_computed_rect_ = rect;
  return true;
}

gfx::RectF WebMediaPlayerCastanets::GetBoundaryRectangle() const {
  LOG(INFO) << "rect : " << last_computed_rect_.ToString();
  return last_computed_rect_;
}

void WebMediaPlayerCastanets::StartLayerBoundUpdateTimer() {
  if (layer_bound_update_timer_.IsRunning())
    return;

  layer_bound_update_timer_.Start(
      FROM_HERE, kLayerBoundUpdateInterval, this,
      &WebMediaPlayerCastanets::OnLayerBoundUpdateTimerFired);
}

void WebMediaPlayerCastanets::StopLayerBoundUpdateTimer() {
  if (layer_bound_update_timer_.IsRunning())
    layer_bound_update_timer_.Stop();
}

void WebMediaPlayerCastanets::OnLayerBoundUpdateTimerFired() {
  if (UpdateBoundaryRectangle()) {
    if (manager_) {
      manager_->SetMediaGeometry(player_id_, GetBoundaryRectangle());
      StopLayerBoundUpdateTimer();
    }
  }
}

bool WebMediaPlayerCastanets::ShouldCreateVideoHoleFrame() const {
  if (HasVideo() && is_video_hole_)
    return true;
  return false;
}
#endif

}  // namespace content
