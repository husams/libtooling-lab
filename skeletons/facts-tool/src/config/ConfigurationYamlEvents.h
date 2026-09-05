#pragma once

#include <yaml-cpp/eventhandler.h>

namespace facts::config {

class SafeYamlEvents final : public YAML::EventHandler {
public:
  void OnDocumentStart(const YAML::Mark &) override {}
  void OnDocumentEnd() override {}
  void OnNull(const YAML::Mark &, YAML::anchor_t anchor) override { reject(anchor); }
  void OnAlias(const YAML::Mark &, YAML::anchor_t) override { invalid = true; }
  void OnScalar(const YAML::Mark &, const std::string &tag,
                YAML::anchor_t anchor, const std::string &) override {
    reject(tag, anchor);
  }
  void OnSequenceStart(const YAML::Mark &, const std::string &tag,
                       YAML::anchor_t anchor, YAML::EmitterStyle::value) override {
    reject(tag, anchor);
  }
  void OnSequenceEnd() override {}
  void OnMapStart(const YAML::Mark &, const std::string &tag,
                  YAML::anchor_t anchor, YAML::EmitterStyle::value) override {
    reject(tag, anchor);
  }
  void OnMapEnd() override {}
  void OnAnchor(const YAML::Mark &, const std::string &) override { invalid = true; }

  bool invalid = false;

private:
  void reject(YAML::anchor_t anchor) { invalid = invalid || anchor != 0; }
  void reject(const std::string &tag, YAML::anchor_t anchor) {
    invalid = invalid || (tag != "?" && tag != "!") || anchor != 0;
  }
};

} // namespace facts::config
