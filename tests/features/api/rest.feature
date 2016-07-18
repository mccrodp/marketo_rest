@api
Feature: Marketo REST API features
  In order to prove that the module REST API methods function properly
  I need all of these tests to run successfully

  Background: Modules are clean and users are ready to test
    Given all Marketo REST modules are clean and using "marketo_test_settings"

  @api
  Scenario: Ensure we request, receive and store access token correctly
    Given I send a GET request to Access Token Request URL

    Then I have a valid access token value
    And I have a valid token expiry value
    
    When I receive the access_token response string
    And I save the valid access_token response data
    Then I should have a cached version of the 'access_token'
    And I should have stored the calculated 'access_token_expiry'

