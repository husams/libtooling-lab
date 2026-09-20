#pragma once
#include "apis/runtime/Service.h"
#include <boost/asio/steady_timer.hpp>

namespace facts::apis::runtime {
void drain(boost::asio::io_context &io, Service &service,
           boost::asio::steady_timer &timer);
}
