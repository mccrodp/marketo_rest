@api
Feature: Marketo REST Webform features
  In order to prove that the marketo_rest_webform module is working properly
  As a developer
  I need all of these tests to run successfully

  Background: Fresh module install
    Given all Marketo REST modules are clean and using "marketo_test_settings"
    And I have the input:
      |    email                    |        firstName       |
      |    test@marketo-rest.com    |            Jason       |
    When I submit a webform node
    Then the return value 'success' should be 'true'