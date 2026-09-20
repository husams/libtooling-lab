Feature: Monitoring follows changes to registered repositories and active clones

  Scenario: Inactive clones do not trigger imports or change the active clone
    Given a catalog server with an active clone and a registered inactive clone
    When I edit the inactive clone and then the active clone
    Then the inactive clone stays inactive and is not imported or extracted

  Scenario: Switching active clones refreshes watch roots without a restart
    Given a catalog server with an active clone and a registered inactive clone
    When I switch the active clone through the REST repository command
    And I edit the former clone and then the newly active clone
    Then only the newly active clone is monitored and its label is preserved
    And facts provenance points at the new active checkout

  Scenario: Clone switching also refreshes facts using stored compilation commands
    Given a catalog server whose inactive clone only has stored compilation commands
    When I switch the active clone through the REST repository command
    And I edit the former clone and then the newly active clone
    Then only the newly active clone is monitored and its label is preserved
    And facts provenance points at the new active checkout

  Scenario: New repositories are discovered from the live project database
    Given two indexed repositories registered with custom names and clone labels
    And the catalog server starts without a monitored directory list
    When I register an additional repository through REST while monitoring
    Then the added repository is discovered imported and indexed automatically

  Scenario: An unavailable checkout recovers without interrupting other repositories
    Given two indexed repositories registered with custom names and clone labels
    And the catalog server starts without a monitored directory list
    When the alpha checkout becomes unavailable and I edit beta
    Then watch status reports alpha unavailable while beta and HTTP remain healthy
    When I restore alpha with source edits made while it was unavailable
    Then monitoring recovers and indexes alpha without a restart or another edit

  Scenario: One recovered checkout resumes indexing while another stays unavailable
    Given two indexed repositories registered with custom names and clone labels
    And the catalog server starts without a monitored directory list
    When both configured checkouts become unavailable
    And I restore alpha with source edits made while it was unavailable
    Then alpha is automatically indexed while beta remains reported unavailable
