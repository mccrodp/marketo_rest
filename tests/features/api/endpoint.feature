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
    Then the response should contain json:
    """
    {
      "result": [
        {
          "id": @integer@,
          "displayName": @string@,
          "dataType": @string@,
          "rest": {
            "name": @string@,
            "readOnly": @string@
          }
        },
      ],
      "success": true
    }
    """
