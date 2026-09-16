@ast_cache
Feature: Persist ASTs used by missing call graph recovery
  Recovery freshness scans share the configured AST persistence policy.

  Scenario: Repeated missing recovery scans reuse the persisted AST
    Given a recovery project has AST caching enabled and a warmed recovery scan
    When call graph recovery scans the cached project again
    Then call graph recovery reuses the persisted AST and preserves recovered edges
