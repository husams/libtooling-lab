@s021
Feature: Recovery reuses production C++ evidence
  Scenario: Automatic Guard destruction is recovered and reused
    Given an S-021 app calls an unextracted registered library
    And the S-021 library uses an automatic Guard destructor
    And S-021 body generation is recorded by the fixture
    And S-021 match-index writes are guarded
    When S-021 missing recovery is requested
    Then S-021 recovers and reuses the C++ variant three times

  Scenario: std::string construction is recovered and reused
    Given an S-021 app calls an unextracted registered library
    And the S-021 library constructs a std::string
    And S-021 body generation is recorded by the fixture
    And S-021 match-index writes are guarded
    When S-021 missing recovery is requested
    Then S-021 recovers and reuses the C++ variant three times

  Scenario: Virtual Base and Derived dispatch is recovered and reused
    Given an S-021 app calls an unextracted registered library
    And the S-021 library uses virtual Base and Derived dispatch
    And S-021 body generation is recorded by the fixture
    And S-021 match-index writes are guarded
    When S-021 missing recovery is requested
    Then S-021 recovers and reuses the C++ variant three times
