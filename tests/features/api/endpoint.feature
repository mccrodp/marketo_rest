@api
Feature: Marketo REST API endpoint features
  In order to prove that the api endpoint methods function properly
  I need all of these tests to run successfully

  Background: Modules are clean and users are ready to test
    Given all Marketo REST modules are clean and using "marketo_rest_test_settings"
    And I have instantiated the Marketo rest client using "marketo_rest_test_settings"
    And I request an access token

  @api @lead_describe
  Scenario: Ensure we receive all fields and the data type of a lead object
    Given I request all fields on the lead object
    Then the response should have success 'true' and element 'result' containing json:
    """
      {
        "id":@integer@,
        "displayName":@string@,
        "dataType":@string@,
        "length":@integer@,
        "rest": {
          "name":@string@,
          "readOnly":false
        },
        "soap": {
          "name":@string@,
          "readOnly":false
        }
      }
    """

  @api @lead_createupdate
  Scenario: Ensure we create / update a lead with all fields submitted
    Given I have the action 'createOrUpdate' and the lookupField: 'email'
    And I have the input:
      |          email          | firstName | postalCode |
      |  test@marketo_rest.com  |   Jason   |    04828   |
    When I sync leads
    Then the return value 'success' should be 'true'

  @api @lead_get
  Scenario: Ensure we retrieve multiple leads for a given search criteria
    Given I have the input:
      |    filterType    |        filterValues       |
      |      email       |   test@marketo_rest.com   |
    When I get leads
    Then I should have a lead with 'firstName' equal to 'Jason'
    And the return value 'success' should be 'true'

  @api @activity_types
  Scenario: Ensure we retrieve meta data about activity types
    When I request activity types
    Then the return value 'success' should be 'true'
    And I should have stored an array of activity types

  @api @paging_token
  Scenario: Ensure we retrieve new paging token
    When I request a paging token
    Then the return value 'success' should be 'true'
    And I should have a page token containing '===='

  @api @lead_activities_get
  Scenario: Ensure we retrieve activities for a lead
    Given I have a valid paging token
    And I have stored activity types
    When I request all activities for a lead with field: 'email' equal to 'test@marketo_rest.com'
    Then the return value 'success' should be 'true'