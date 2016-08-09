@api
Feature: Marketo REST Webform features
  In order to prove that the marketo_rest_webform module is working properly
  As a developer
  I need all of these tests to run successfully

  Background: Fresh module install
    Given all Marketo REST modules are clean and using "marketo_test_settings"
    And I have instantiated the Marketo rest client using "marketo_test_settings"

  @api @webform_submit
  Scenario: Ensure Marketo receives the correct data when submitting a webform
    Given all Marketo REST modules are clean and using "marketo_test_settings"
    And the database table 'marketo_rest_lead_fields' with primary key 'marketo_id' contains:
      | marketo_id |     name      |   marketo_rest_key    |    marketo_munchkin_key      |    enabled      |
      |   51	     | Email Address |       email	         |        Email	                |      1          |
      |    2       | Company Name  |       company         |        Company               |      1          |
      |    3       | Site          |       site            |        Site                  |      1          |
    When I have created a webform node with the mapped fields:
      |    marketo_rest_email    |    marketo_rest_company_name   |    marketo_rest_site        |
      |    51                    |        2                       |           3                 |
    And I submit a webform node with the input:
      |    marketo_rest_email    |    marketo_rest_company_name      |   marketo_rest_site                |
      |    test@marketo-rest.com |      Test Name Ltd.               |   http://marketo-rest.com          |
    Then the return value 'success' should be 'true'
    And I delete the test data
