#include "apis/runtime/Drain.h"

namespace facts::apis::runtime {
void drain(boost::asio::io_context &io, Service &service,
           boost::asio::steady_timer &timer) {
  timer.expires_after(std::chrono::milliseconds(200));
  timer.async_wait([&io, &service, &timer](auto error) {
    if (error) return;
    if (service.busy()) drain(io, service, timer);
    else io.stop();
  });
}
}
