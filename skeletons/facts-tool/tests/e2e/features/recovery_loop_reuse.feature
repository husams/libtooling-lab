@s021
Feature: Recovery validates loop evidence using native extraction
  Scenario Outline: Native C++ loop evidence is recovered and reused
    Given an S-021 app calls an unextracted registered library
    And the S-021 library <body>
    And S-021 body generation is recorded by the fixture
    And S-021 match-index writes are guarded
    When S-021 missing recovery is requested
    Then S-021 recovers and reuses the C++ variant three times

    Examples:
      | body                                 |
      | uses a vector range loop             |
      | uses a user container range loop     |
      | uses an explicit iterator loop       |
      | passes an initializer list argument  |
