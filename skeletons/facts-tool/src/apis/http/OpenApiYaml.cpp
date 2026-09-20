#include "apis/http/OpenApi.h"
#include <yaml-cpp/yaml.h>
#include <stdexcept>

namespace facts::apis {
namespace {
void emit(YAML::Emitter &output, const Json &value) {
  if (value.is_object()) {
    output << YAML::BeginMap;
    for (const auto &[key, item] : value.items()) {
      output << YAML::Key << YAML::DoubleQuoted << key << YAML::Value;
      emit(output, item);
    }
    output << YAML::EndMap;
  } else if (value.is_array()) {
    output << YAML::BeginSeq;
    for (const auto &item : value) emit(output, item);
    output << YAML::EndSeq;
  } else if (value.is_string()) output << YAML::DoubleQuoted << value.get<std::string>();
  else if (value.is_boolean()) output << value.get<bool>();
  else if (value.is_number_unsigned()) output << value.get<std::uint64_t>();
  else if (value.is_number_integer()) output << value.get<std::int64_t>();
  else if (value.is_number_float()) output << value.get<double>();
  else output << YAML::Null;
}
}
std::string openApiYaml(const Json &document) {
  YAML::Emitter output;
  output.SetIndent(2);
  emit(output, document);
  if (!output.good()) throw std::runtime_error(output.GetLastError());
  return std::string(output.c_str()) + '\n';
}
}
