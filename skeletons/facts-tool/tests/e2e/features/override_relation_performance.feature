Feature: Override relation extraction avoids system-header file resolution
  Scenario: Preserve project facts while skipping filtered override locations
    Given a compile database for the override relation performance fixture
    When verbose override relation extraction runs
    Then system-header override resolution is skipped and project extraction commits
