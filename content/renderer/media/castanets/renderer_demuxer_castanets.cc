// Copyright 2019 Samsung Electronics Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/castanets/renderer_demuxer_castanets.h"

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "content/child/thread_safe_sender.h"
#include "content/common/media/media_player_messages_castanets.h"
#include "content/renderer/media/castanets/media_source_delegate_castanets.h"
#include "content/renderer/media/castanets/renderer_media_player_manager_castanets.h"
#include "content/renderer/render_thread_impl.h"

namespace content {

scoped_refptr<IPC::MessageFilter> CreateRendererDemuxerCastanets() {
  return new RendererDemuxerCastanets();
}

RendererDemuxerCastanets::RendererDemuxerCastanets()
    : thread_safe_sender_(RenderThreadImpl::current()->thread_safe_sender()),
      media_task_runner_(
          RenderThreadImpl::current()->GetMediaThreadTaskRunner()) {}

RendererDemuxerCastanets::~RendererDemuxerCastanets() {}

int RendererDemuxerCastanets::GetNextDemuxerClientID() {
  // Don't use zero for IDs since it can be interpreted as having no ID.
  return next_demuxer_client_id_.GetNext() + 1;
}

void RendererDemuxerCastanets::AddDelegate(
    int demuxer_client_id,
    MediaSourceDelegateCastanets* delegate) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  delegates_.AddWithID(delegate, demuxer_client_id);
}

void RendererDemuxerCastanets::RemoveDelegate(int demuxer_client_id) {
  DCHECK(media_task_runner_->BelongsToCurrentThread());
  delegates_.Remove(demuxer_client_id);
}

bool RendererDemuxerCastanets::OnMessageReceived(const IPC::Message& message) {
  switch (message.type()) {
    case MediaPlayerCastanetsMsg_ReadFromDemuxer::ID:
    case MediaPlayerCastanetsMsg_DemuxerSeekRequest::ID:
      media_task_runner_->PostTask(
          FROM_HERE, base::Bind(&RendererDemuxerCastanets::DispatchMessage,
                                this, message));
      return true;
  }
  return false;
}

void RendererDemuxerCastanets::DemuxerReady(
    int demuxer_client_id,
    const media::DemuxerConfigs& configs) {
  thread_safe_sender_->Send(
      new MediaPlayerCastanetsHostMsg_DemuxerReady(demuxer_client_id, configs));
}

void RendererDemuxerCastanets::DemuxerBufferedChanged(
    int demuxer_client_id,
    const media::Ranges<base::TimeDelta>& ranges) {
  thread_safe_sender_->Send(
      new MediaPlayerCastanetsHostMsg_DemuxerBufferedChanged(demuxer_client_id,
                                                             ranges));
}

bool RendererDemuxerCastanets::ReadFromDemuxerAck(
    int demuxer_client_id,
    const std::vector<unsigned char>& stream_data,
    const media::DemuxedBufferMetaData& meta_data) {
  return thread_safe_sender_->Send(
      new MediaPlayerCastanetsHostMsg_ReadFromDemuxerAck(
          demuxer_client_id, stream_data, meta_data));
}

void RendererDemuxerCastanets::DemuxerSeekDone(
    int demuxer_client_id,
    const base::TimeDelta& actual_browser_seek_time,
    const base::TimeDelta& video_key_frame) {
  thread_safe_sender_->Send(new MediaPlayerCastanetsHostMsg_DemuxerSeekDone(
      demuxer_client_id, actual_browser_seek_time, video_key_frame));
}

void RendererDemuxerCastanets::DurationChanged(
    int demuxer_client_id,
    const base::TimeDelta& duration) {
  thread_safe_sender_->Send(new MediaPlayerCastanetsHostMsg_DurationChanged(
      demuxer_client_id, duration));
}

void RendererDemuxerCastanets::DispatchMessage(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(RendererDemuxerCastanets, message)
    IPC_MESSAGE_HANDLER(MediaPlayerCastanetsMsg_ReadFromDemuxer,
                        OnReadFromDemuxer)
    IPC_MESSAGE_HANDLER(MediaPlayerCastanetsMsg_DemuxerSeekRequest,
                        OnDemuxerSeekRequest)
  IPC_END_MESSAGE_MAP()
}

void RendererDemuxerCastanets::OnReadFromDemuxer(
    int demuxer_client_id,
    media::DemuxerStream::Type type) {
  MediaSourceDelegateCastanets* delegate = delegates_.Lookup(demuxer_client_id);
  if (delegate)
    delegate->OnReadFromDemuxer(type);
}

void RendererDemuxerCastanets::OnDemuxerSeekRequest(
    int demuxer_client_id,
    const base::TimeDelta& time_to_seek) {
  MediaSourceDelegateCastanets* delegate = delegates_.Lookup(demuxer_client_id);
  if (delegate)
    delegate->StartSeek(time_to_seek, false);
}

}  // namespace content
