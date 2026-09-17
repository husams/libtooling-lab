@ast_cache
Feature: Persist ASTs used by missing call graph recovery
  Recovery freshness scans share the configured AST persistence policy.

  Scenario: First and repeated missing recovery scans reuse ASTs prepared by import
    Given a recovery project has import-prepared ASTs and a first recovery scan
    When call graph recovery scans the cached project again
    Then call graph recovery reuses the persisted AST and preserves recovered edges

  Scenario: Cached symlink-parent header paths keep recovery limited to its real inputs
    Given cached recovery uses a symlink-parent header and an unrelated source is missing
    When call graph recovery scans the cached project again
    Then call graph recovery reuses the persisted AST and preserves recovered edges
