#pragma once
#include "apis/jobs/Queue.h"

namespace facts::apis {
struct OpenApiDocuments { std::string json, yaml; };
Json openApi(const std::vector<std::string> &commands, bool authenticated);
std::string openApiYaml(const Json &document);
OpenApiDocuments openApiDocuments(const std::vector<std::string> &commands,
                                  bool authenticated);
}
