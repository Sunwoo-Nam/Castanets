// Copyright 2019 Samsung Electronics Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_TIZEN_RENDERER_DEMUXER_CASTANETS_H_
#define CONTENT_RENDERER_MEDIA_TIZEN_RENDERER_DEMUXER_CASTANETS_H_

#include "base/atomic_sequence_num.h"
#include "base/containers/id_map.h"
#include "base/memory/shared_memory.h"
#include "ipc/message_filter.h"
#include "media/base/castanets/demuxer_stream_player_params_castanets.h"
#include "media/base/ranges.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace content {

class MediaSourceDelegateCastanets;
class ThreadSafeSender;

// Represents the renderer process half of an IPC-based implementation of
// media::DemuxerCastanets.
//
// Refer to BrowserDemuxerCastanets for the browser process half.
class RendererDemuxerCastanets : public IPC::MessageFilter {
 public:
  RendererDemuxerCastanets();

  // Returns the next available demuxer client ID for use in IPC messages.
  //
  // Safe to call on any thread.
  int GetNextDemuxerClientID();

  // Associates |delegate| with |demuxer_client_id| for handling incoming IPC
  // messages.
  //
  // Must be called on media thread.
  void AddDelegate(int demuxer_client_id,
                   MediaSourceDelegateCastanets* delegate);

  // Removes the association created by AddDelegate().
  //
  // Must be called on media thread.
  void RemoveDelegate(int demuxer_client_id);

  // IPC::ChannelProxy::MessageFilter overrides.
  bool OnMessageReceived(const IPC::Message& message) override;

  // media::DemuxerCastanetsClient "implementation".
  void DemuxerReady(int demuxer_client_id,
                    const media::DemuxerConfigs& configs);

  void DemuxerBufferedChanged(int demuxer_client_id,
                              const media::Ranges<base::TimeDelta>& ranges);

  bool ReadFromDemuxerAck(int demuxer_client_id,
                          const std::vector<unsigned char>& stream_data,
                          const media::DemuxedBufferMetaData& meta_data);
  void DemuxerSeekDone(int demuxer_client_id,
                       const base::TimeDelta& actual_browser_seek_time,
                       const base::TimeDelta& video_key_frame);
  void DurationChanged(int demuxer_client_id, const base::TimeDelta& duration);

 protected:
  friend class base::RefCountedThreadSafe<RendererDemuxerCastanets>;
  ~RendererDemuxerCastanets() override;

 private:
  void DispatchMessage(const IPC::Message& message);
  void OnReadFromDemuxer(int demuxer_client_id,
                         media::DemuxerStream::Type type);
  void OnDemuxerSeekRequest(int demuxer_client_id,
                            const base::TimeDelta& time_to_seek);

  base::AtomicSequenceNumber next_demuxer_client_id_;

  base::IDMap<MediaSourceDelegateCastanets*> delegates_;
  scoped_refptr<ThreadSafeSender> thread_safe_sender_;
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(RendererDemuxerCastanets);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_TIZEN_RENDERER_DEMUXER_CASTANETS_H_
