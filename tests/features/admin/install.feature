@api
Feature: Module setup
  In order to prove that this module can be installed and uninstalled cleanly
  As an administrator
  I need to do the following

  Background: Reset to a clean state
    Given I reinstall all Marketo REST modules

  @install
  Scenario: Install all Marketo REST modules
    Given I am logged in as an administrator
    When I go to "/admin/config/search/marketo_rest"
    Then I should see the heading "Marketo REST"
    And I should see a "#marketo-ma-admin-settings-form" element

  @uninstall
  Scenario: Disable and uninstall all Marketo REST modules
    Given I run drush "vset" "marketo_rest_bogus 'bogus'"

    Given I am logged in as an administrator
    And I go to "/admin/config/search/marketo_rest"
    And I fill in "marketo_rest_munchkin_account_id" with "bogus"
    And I fill in "marketo_rest_munchkin_api_private_key" with "bogus"
    When I press "Save configuration"
    Then I should see "The configuration options have been saved."

    Given I uninstall all Marketo REST modules
    And I run drush "vget" "marketo_rest --format=json"
    Then drush output should contain '{"marketo_rest_bogus":"bogus"}'
