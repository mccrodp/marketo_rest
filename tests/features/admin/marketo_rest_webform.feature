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
    And I have created a webform node with the mapped fields:
      |    marketo_rest_email    |    marketo_rest_last_name      |    marketo_rest_first_name  |
      |    email                 |        lastName                |           firstName         |
    When I submit a webform node with the input:
      |    marketo_rest_email    |    marketo_rest_last_name      |   marketo_rest_first_name   |
      |    test@marketo-rest.com |      Testerman                 |           Jason             |
    Then the return value 'success' should be 'true'
    And I delete the test data