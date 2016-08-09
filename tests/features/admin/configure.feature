@api
Feature: Module configuration
  In order to use the Marketo REST modules
  As an administrator
  I must configure the module settings

  Background: Fresh module install
    Given all Marketo REST modules are clean
    
  @config
  Scenario: Configure module settings
    When I am on the homepage
    
    Given I am logged in as an administrator
    And I go to "/admin/config/search/marketo_rest"
    When I press "Save configuration"
    Then I should see "Account ID field is required."
    And I should see "API Private Key field is required."
    
    Given I am logged in as an administrator
    And I go to "/admin/config/search/marketo_rest"
    And I fill in "marketo_rest_munchkin_account_id" with "bogus"
    And I fill in "marketo_rest_munchkin_api_private_key" with "bogus"
    When I press "Save configuration"
    Then I should see "The configuration options have been saved."

    Given I am logged in as an administrator
    When I go to "/admin/config/search/marketo_rest"
    And I fill in "marketo_rest_munchkin_account_id" with "bogus"
    And I fill in "marketo_rest_munchkin_api_private_key" with "bogus"
    And I select the radio button "REST API" with the id "edit-marketo-rest-tracking-method-rest"
    When I press "Save configuration"
    Then I should see "Unable to validate REST API settings."
  
  @config @field_definitions
  Scenario: Configure field definition settings
    Given all Marketo REST modules are clean and using "marketo_settings"
    And I have instantiated the Marketo rest client using "marketo_settings"
    When I am logged in as an administrator
    And I go to "/admin/config/search/marketo_rest/field_definition"
    When I press "Retrieve from Marketo"
    Then I should see have field definition:
     | marketo_id |     name      |   marketo_rest_key    |    marketo_munchkin_key      |    enabled      |
     |    2       | Company Name  |       company         |        Company               |      0          |
     |    3       | Site          |       site            |        Site                  |      0          |

  @config @live
  Scenario: Configure live module settings
    Given I populate the Marketo REST config using "marketo_settings"
    When I am logged in as an administrator
    And I go to "/admin/config/search/marketo_rest"
    When I press "Save configuration"
    Then I should see "The configuration options have been saved."
