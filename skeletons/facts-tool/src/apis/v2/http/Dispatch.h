#pragma once
#include "apis/http/Router.h"
#include "apis/domain/Selection.h"

namespace facts::apis::v2::http {
struct Route {
  std::string resource, id, child;
  bool jobs = false;
  Json query = Json::object();
};
domain::Result<Route> parse(std::string_view target);
domain::Result<Json> body(const Request &request);
domain::Result<Json> page(const Json &items, const Json &query,
                          std::string_view identity);
void handle(Router &, const Request &, Reply);
void jobs(Router &, const Request &, const Route &, Reply);
void catalog(Router &, const Request &, const Route &, Reply);
void symbols(Router &, const Request &, const Route &, Reply);
void settings(Router &, const Request &, const Route &, Reply);
Json watchSettings(const Settings &);
domain::Result<Settings> patchSettings(const Settings &, const Json &, bool replace);
}
