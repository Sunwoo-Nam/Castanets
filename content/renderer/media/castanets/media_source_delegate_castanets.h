// Copyright 2019 Samsung Electronics Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_TIZEN_MEDIA_SOURCE_DELEGATE_CASTANETS_H_
#define CONTENT_RENDERER_MEDIA_TIZEN_MEDIA_SOURCE_DELEGATE_CASTANETS_H_

#include <atomic>
#include "content/renderer/media/castanets/renderer_demuxer_castanets.h"
#include "media/base/decoder_buffer.h"
#include "media/base/demuxer_stream.h"
#include "media/filters/chunk_demuxer.h"
#include "media/filters/decrypting_demuxer_stream.h"
#include "third_party/WebKit/public/platform/WebMediaPlayer.h"

namespace content {

class MediaSourceDelegateCastanets : public media::DemuxerHost {
 public:
  typedef base::Callback<void(blink::WebMediaSource*)> MediaSourceOpenedCB;
  typedef base::Callback<void(blink::WebMediaPlayer::NetworkState)>
      UpdateNetworkStateCB;
  typedef base::Callback<void(base::TimeDelta)> DurationChangeCB;

  // Callback to notify that a CDM is ready. CdmAttachedCB is called when the
  // CDM has been completely attached to the media pipeline.
  typedef base::Callback<void(media::CdmContext*, const media::CdmAttachedCB&)>
      CdmReadyCB;

  // Callback to set a CdmReadyCB, which will be called when a CDM is ready.
  typedef base::Callback<void(const CdmReadyCB&)> SetCdmReadyCB;

  MediaSourceDelegateCastanets(
      RendererDemuxerCastanets* demuxer_client,
      int demuxer_client_id,
      const scoped_refptr<base::SingleThreadTaskRunner>& media_task_runner,
      media::MediaLog* media_log);
  ~MediaSourceDelegateCastanets() override;

  // DemuxerHost implementation.
  void OnBufferedTimeRangesChanged(
      const media::Ranges<base::TimeDelta>& ranges) override;

  // Sets the duration of the media in microseconds.
  // Duration may be kInfiniteDuration() if the duration is not known.
  void SetDuration(base::TimeDelta duration) override;

  // Stops execution of the pipeline due to a fatal error.  Do not call this
  // method with PIPELINE_OK.
  void OnDemuxerError(media::PipelineStatus error) override;

  // Add |text_stream| to the collection managed by the text renderer.
  void AddTextStream(media::DemuxerStream* text_stream,
                     const media::TextTrackConfig& config) override;

  // Remove |text_stream| from the presentation.
  void RemoveTextStream(media::DemuxerStream* text_stream) override;

  void InitializeMediaSource(
      const MediaSourceOpenedCB& media_source_opened_cb,
      const media::Demuxer::EncryptedMediaInitDataCB& emedia_init_data_cb,
      const SetCdmReadyCB& set_cdm_ready_cb,
      const UpdateNetworkStateCB& update_network_state_cb,
      const DurationChangeCB& duration_change_cb,
      const base::Closure& waiting_for_decryption_key_cb);

  blink::WebTimeRanges Buffered() const;

  // Called when DemuxerStreamPlayer needs to read data from ChunkDemuxer.
  void OnReadFromDemuxer(media::DemuxerStream::Type type);

  // Must be called explicitly before |this| can be destroyed.
  void Stop(const base::Closure& stop_cb);

  // In MSE case, calls ChunkDemuxer::StartWaitingForSeek(), sets the
  // expectation that a regular seek will be arriving.
  void StartWaitingForSeek(const base::TimeDelta& seek_time);

  // Calls ChunkDemuxer::CancelPendingSeek(). Also sets the
  // expectation that a regular seek will be arriving.
  void CancelPendingSeek(const base::TimeDelta& seek_time);

  // Sets the expectation that a regular seek will be arriving.
  void StartSeek(const base::TimeDelta& seek_time,
                 bool is_seeking_pending_seek);

  // Callback for ChunkDemuxer::Seek().
  void OnDemuxerSeekDone(base::TimeDelta demuxer_seek_time,
                         media::PipelineStatus status);

 private:
  // callback handlers
  void OnEncryptedMediaInitData(media::EmeInitDataType init_data_type,
                                const std::vector<uint8_t>& init_data);
  void OnDemuxerProgress();
  void OnDemuxerOpened();

  void InitializeDemuxer();
  void OnDemuxerInitDone(media::PipelineStatus status);

  bool HasEncryptedStream();

  // Callback to set CDM and fires |cdm_attached_cb| with the result.
  void SetCdm(media::CdmContext* cdm_context,
              const media::CdmAttachedCB& cdm_attached_cb);
  // Stops and clears objects on the media thread.
  void StopDemuxer(const base::Closure& stop_cb);
  bool CanNotifyDemuxerReady();
  void NotifyDemuxerReady(bool is_cdm_attached);
  void OnDurationChanged(const base::TimeDelta& duration);
  void OnBufferReady(media::DemuxerStream::Type type,
                     media::DemuxerStream::Status status,
                     const scoped_refptr<media::DecoderBuffer>& buffer);
  void SeekInternal(const base::TimeDelta& seek_time);
  void GetDemuxerStreamInfo();

  // Initializes DecryptingDemuxerStreams if audio/video stream is encrypted.
  void InitAudioDecryptingDemuxerStream();
  void InitVideoDecryptingDemuxerStream();

  // Callbacks for DecryptingDemuxerStream::Initialize().
  void OnAudioDecryptingDemuxerStreamInitDone(media::PipelineStatus status);
  void OnVideoDecryptingDemuxerStreamInitDone(media::PipelineStatus status);

  // Runs on the media thread.
  void ResetAudioDecryptingDemuxerStream();
  void ResetVideoDecryptingDemuxerStream();
  void FinishResettingDecryptingDemuxerStreams();
  media::DemuxerStream* GetStreamByType(media::DemuxerStream::Type type);

  RendererDemuxerCastanets* demuxer_client_;
  int demuxer_client_id_;
  media::MediaLog* media_log_;

  MediaSourceOpenedCB media_source_opened_cb_;
  UpdateNetworkStateCB update_network_state_cb_;
  DurationChangeCB duration_change_cb_;
  base::Closure waiting_for_decryption_key_cb_;

  std::unique_ptr<media::ChunkDemuxer> chunk_demuxer_;
  std::vector<media::DemuxerStream*> media_stream_;
  media::DemuxerStream* audio_stream_;
  media::DemuxerStream* video_stream_;
  media::Ranges<base::TimeDelta> buffered_time_ranges_;

  SetCdmReadyCB set_cdm_ready_cb_;
  media::CdmContext* cdm_context_;
  media::CdmAttachedCB pending_cdm_attached_cb_;
  media::Demuxer::EncryptedMediaInitDataCB emedia_init_data_cb_;
  std::unique_ptr<media::DecryptingDemuxerStream>
      audio_decrypting_demuxer_stream_;
  std::unique_ptr<media::DecryptingDemuxerStream>
      video_decrypting_demuxer_stream_;

  mutable base::Lock seeking_lock_;
  base::TimeDelta seek_time_;
  bool pending_seek_;
  std::atomic<bool> is_seeking_;

  // Will handle internal seek coming from |MediaSourcePlayerGstreamer|
  // if new seek has been fired by |HTMLMediaElement|.
  // Always one should seek to latest time and ignore previous seeks.
  bool seeking_pending_seek_;

  // Will handle |seek| request coming after |chunk_demuxer|
  // has requested |gstreamer| to seek.
  bool is_demuxer_seek_done_;
  base::TimeDelta pending_seek_time_;

  bool is_audio_read_fired_;
  bool is_video_read_fired_;

  bool is_demuxer_ready_;
  base::TimeDelta video_key_frame_;

  // Message loop for media thread and corresponding weak pointer.
  const scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  // Message loop for main renderer thread and corresponding weak pointer.
  const scoped_refptr<base::SingleThreadTaskRunner> main_loop_;
  base::WeakPtr<MediaSourceDelegateCastanets> main_weak_this_;
  base::WeakPtrFactory<MediaSourceDelegateCastanets> main_weak_factory_;

  base::WeakPtrFactory<MediaSourceDelegateCastanets> media_weak_factory_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_TIZEN_MEDIA_SOURCE_DELEGATE_CASTANETS_H_
