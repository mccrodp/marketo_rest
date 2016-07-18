@api @marketo_rest_drush
Feature: Marketo REST Drush features
  In order to prove that drush functions are working properly
  As a developer
  I need all of these tests to run successfully

  Background: Fresh module install
    Given all Marketo REST modules are clean and using "marketo_test_settings"
    
  Scenario Outline: Ensure all expected drush commands are available and functioning
    When I run drush "help" "<command>"
    Then drush output should contain "<description>"
    
    When I run drush "help" "<alias>"
    Then drush output should contain "<description>"
    
  Examples:
    | command      | alias | description        |
    | mrest-fields   | mrestf  | Get Marketo fields |
    | mrest-get-lead | mrestl  | Get Marketo lead   |
    | mrest-verify   | mrestv  | Verify this site   |

  @live
  Scenario: Execute drush commands
    Given I populate the Marketo REST config using "marketo_settings"

    When I run drush "mrest-verify"
    Then drush output should contain "Successfully connected to Marketo"
    
    When I run drush "mrest-fields"
    Then drush output should contain " Name "
    And drush output should contain " Label "