@s021
Feature: Recovery validation reports no user fact writes
  Scenario Outline: Temporary validation respects the requested verbosity
    Given an S-021 app calls an unextracted registered library
    And the S-021 library combines a vector loop and an indirect call
    And S-021 body generation is recorded by the fixture
    And S-021 match-index writes are guarded
    When S-021 missing recovery is requested
    Then S-021 validation output respects verbosity <level>

    Examples:
      | level |
      | 0     |
      | 1     |
      | 3     |
