#include "apis/jobs/Process.h"
#include <boost/asio/buffer.hpp>
#include <algorithm>

namespace facts::apis {
void Process::read(Stream &stream) {
  stream.descriptor.async_read_some(boost::asio::buffer(stream.buffer),
      [self = shared_from_this(), channel = &stream](auto error, std::size_t size) {
    if (self->done_) return;
    constexpr std::size_t limit = 4 * 1024 * 1024;
    const auto accepted = std::min(size, limit - channel->text.size());
    channel->text.append(channel->buffer.data(), accepted);
    channel->truncated |= accepted != size;
    if (error) {
      channel->closed = true;
      boost::system::error_code ignored;
      channel->descriptor.close(ignored);
      self->finish();
      return;
    }
    self->read(*channel);
  });
}
void Process::closeStreams() {
  for (auto *stream : {&output_, &error_}) {
    boost::system::error_code ignored;
    stream->descriptor.close(ignored);
    stream->closed = true;
  }
}
}
