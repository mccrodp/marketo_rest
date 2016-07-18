<?php
/**
 * @file
 * Test methods for Marketo REST.
 */
use Behat\Behat\Tester\Exception\PendingException;
use Drupal\DrupalExtension\Context\RawDrupalContext;
use Drupal\DrupalExtension\Context\DrupalContext;
use Drupal\DrupalExtension\Context\DrushContext;
use Behat\Behat\Context\SnippetAcceptingContext;
use Behat\Gherkin\Node\PyStringNode;
use Behat\Gherkin\Node\TableNode;
use Behat\Behat\Hook\Scope\BeforeScenarioScope;

class FeatureContext extends RawDrupalContext implements SnippetAcceptingContext {
  private $params = array();

  /** @var DrupalContext */
  private $drupalContext;

  /** @var DrushContext */
  private $drushContext;

  /** @var MarketoMockClient */
  private $client;

  /**
   * Keep track of fields so they can be cleaned up.
   *
   * @var array
   */
  protected $fields = array();

  /** @BeforeScenario */
  public function gatherContexts(BeforeScenarioScope $scope) {
    $environment = $scope->getEnvironment();
    $this->drupalContext = $environment->getContext('Drupal\DrupalExtension\Context\DrupalContext');
    $this->drushContext = $environment->getContext('Drupal\DrupalExtension\Context\DrushContext');
  }

  /**
   * Remove any created fields.
   *
   * @AfterScenario
   */
  public function cleanFields() {
    // Remove any fields that were created.
    foreach ($this->fields as $field) {
      $this->drushContext->assertDrushCommandWithArgument("field-delete", "$field --y");
    }
    $this->fields = array();
  }

  /**
   * Initializes context.
   *
   * Every scenario gets its own context instance.
   * You can also pass arbitrary arguments to the
   * context constructor through behat.yml.
   */
  public function __construct(array $parameters) {
    $this->params = $parameters;
  }

  /**
   * Resets all Marketo REST modules to their default enabled state.
   *
   * @Given all Marketo REST modules are clean
   * @Given all Marketo REST modules are clean and using :config
   */
  public function allMarketoRESTModulesClean($config = 'marketo_default_settings') {
    $module_list = array('marketo_rest', 'marketo_rest_user', 'marketo_rest_webform');

    foreach ($module_list as $module) {
      if (!module_exists($module)) {
        module_enable(array($module));
      }
    }

    $this->iPopulateConfigFromBehatYml($config);
    drupal_flush_all_caches();

    foreach ($module_list as $module) {
      if (!module_exists($module)) {
        $message = sprintf('Module "%s" could not be enabled.', $module);
        throw new \Exception($message);
      }
    }
  }

  /**
   * Reinstalls Marketo REST modules.
   *
   * @Given I reinstall all Marketo REST modules
   */
  public function reinstallMarketoRESTModules() {
    $module_list = array('marketo_rest', 'marketo_rest_user', 'marketo_rest_webform');

    $this->uninstallMarketoRESTModules();
    module_enable($module_list);
    drupal_flush_all_caches();

    foreach ($module_list as $module) {
      if (!module_exists($module)) {
        $message = sprintf('Module "%s" could not be enabled.', $module);
        throw new \Exception($message);
      }
    }
  }

  /**
   * Uninstalls all Marketo REST modules.
   *
   * @Given I uninstall all Marketo REST modules
   */
  public function uninstallMarketoRESTModules() {
    $module_list = array('marketo_rest', 'marketo_rest_user', 'marketo_rest_webform');

    module_disable($module_list);
    drupal_uninstall_modules($module_list);
    drupal_flush_all_caches();

    foreach ($module_list as $module) {
      if (module_exists($module)) {
        $message = sprintf('Module "%s" could not be uninstalled.', $module);
        throw new \Exception($message);
      }
    }
  }

  /**
   * Reinstalls the given modules and asserts that they are enabled.
   *
   * @Given the :modules module(s) is/are clean
   */
  public function assertModulesClean($modules) {
    $this->assertModulesUninstalled($modules);
    $this->assertModulesEnabled($modules);
  }

  /**
   * Asserts that the given modules are enabled
   *
   * @Given the :modules module(s) is/are enabled
   */
  public function assertModulesEnabled($modules) {
    $module_list = preg_split("/,\s*/", $modules);
    module_enable($module_list, TRUE);
    foreach ($module_list as $module) {
      if (!module_exists($module)) {
        $this->drushContext->assertDrushCommandWithArgument("pm-list", '--package="Marketo"');
        echo $this->drushContext->readDrushOutput();
        $message = sprintf('Module "%s" is not enabled.', $module);
        throw new \Exception($message);
      }
    }
  }

  /**
   * Asserts that the given modules are disabled
   *
   * @Given the :modules module(s) is/are disabled
   */
  public function assertModulesDisabled($modules) {
    $module_list = preg_split("/,\s*/", $modules);
    module_disable($module_list, TRUE);
    foreach ($module_list as $module) {
      if (module_exists($module)) {
        $this->drushContext->assertDrushCommandWithArgument("pm-list", '--package="Marketo"');
        echo $this->drushContext->readDrushOutput();
        $message = sprintf('Module "%s" is not disabled.', $module);
        throw new \Exception($message);
      }
    }
  }

  /**
   * Asserts that the given modules are uninstalled
   *
   * @Given the :modules module(s) is/are uninstalled
   */
  public function assertModulesUninstalled($modules) {
    $module_list = preg_split("/,\s*/", $modules);
    $this->assertModulesDisabled($modules);
    drupal_uninstall_modules($module_list, TRUE);
    foreach ($module_list as $module) {
      if (module_exists($module)) {
        $this->drushContext->assertDrushCommandWithArgument("pm-list", '--package="Marketo"');
        echo $this->drushContext->readDrushOutput();
        $message = sprintf('Module "%s" could not be uninstalled.', $module);
        throw new \Exception($message);
      }
    }
  }

  /**
   * Creates content of the given type and navigates to a path belonging to it.
   *
   * @Given I am accessing :path belonging to a/an :type (content) with the title :title
   */
  public function accessNodePath($path, $type, $title) {
    // @todo make this easily extensible.
    $node = (object) array(
      'title' => $title,
      'type' => $type,
      'body' => $this->getRandom()->string(255),
    );
    $saved = $this->nodeCreate($node);
    // Set internal page on the new node.
    $this->getSession()->visit($this->locatePath('/node/' . $saved->nid . $path));
  }

  /**
   * @Given Marketo REST is configured using settings from :config
   */
  public function marketoMaIsConfiguredUsingSettingsFrom($config) {
    $this->assertModulesClean("marketo_rest, marketo_rest_user, marketo_rest_webform");

    $settings = array_merge($this->params['marketo_default_settings'], $this->params[$config]);
    foreach ($settings as $key => $value) {
      variable_set($key, $value);
    }
  }

  /**
   * @Given I populate the Marketo REST config using :config
   */
  public function iPopulateConfigFromBehatYml($config) {
    $settings = array_merge($this->params['marketo_default_settings'], $this->params[$config]);
    foreach ($settings as $key => $value) {
      variable_set($key, $value);
    }
  }

  /**
   * Creates fields for the given entity type.
   * | bundle | entity | field_name    | field_type | widget_type |
   * | user   | user   | field_company | text       | text_field  |
   * | ...    | ...    | ...           | ...        | ...         |
   *
   * @Given fields:
   */
  public function createCustomUserFields(TableNode $fieldTable) {
    foreach ($fieldTable->getHash() as $fieldHash) {
      $field = (object) $fieldHash;
      array_push($this->fields, $field->field_name);
      $this->drushContext->assertDrushCommandWithArgument("field-create", "$field->bundle $field->field_name,$field->field_type,$field->widget_type --entity_type=$field->entity");
    }
  }

  /**
   * @Then Munchkin tracking should be enabled
   */
  public function assertMunchkinTrackingEnabled() {
    $enabled = $this->getSession()->evaluateScript("return (Drupal.settings.marketo_rest === undefined) ? false : Drupal.settings.marketo_rest.track;");
    if ($enabled !== TRUE) {
      throw new Exception("Munchkin tracking is excpected to be ON but is currently OFF");
    }
  }

  /**
   * @Then Munchkin tracking should not be enabled
   * @Then Munchkin tracking should be disabled
   */
  public function assertMunchkinTrackingNotEnabled() {
    $enabled = $this->getSession()->evaluateScript("return (Drupal.settings.marketo_rest === undefined) ? false : Drupal.settings.marketo_rest.track;");
    if ($enabled !== FALSE) {
      throw new Exception("Munchkin tracking is expected to be OFF but is currently ON");
    }
  }

  /**
   * @Then Munchkin associateLead action should send data
   */
  public function assertMunchkinAssociateLeadSendData(TableNode $fields) {
    $actions = $this->getSession()->evaluateScript("return Drupal.settings.marketo_rest.actions");
    if ((isset($actions[0]['action']) && $actions[0]['action'] == 'associateLead') == FALSE) {
      throw new \Exception("Munchkin associateLead did not fire as expected");
    }
    foreach ($fields->getHash() as $row) {
      if ($actions[0]['data'][$row['field']] != $row['value']) {
        $message = sprintf('Field "%s" was expected to be "%s" but was "%s".', $row['field'], $row['value'], $actions[0]['data'][$row['field']]);
        throw new \Exception($message);
      }
    }
  }

  /**
   * @Given I evaluate script:
   */
  public function iEvaluateScript(PyStringNode $script) {
    $this->getSession()->evaluateScript($script->getRaw());
  }

  /**
   * @Given I execute script:
   */
  public function iExecuteScript(PyStringNode $script) {
    $this->getSession()->executeScript($script->getRaw());
  }

  /**
   * @Given given javascript variable :variable equals :value
   */
  public function givenJavascriptVariableEquals($variable, $value) {
    $result = $this->getSession()->evaluateScript("$variable == $value");
    if ($result === FALSE) {
      throw new \Exception(sprintf("The variable '%s' was expected to be '%s' but evaluated to %s", $variable, $value, $result));
    }
  }

  /**
   * @Given given javascript variable :variable does not equal :value
   */
  public function givenJavascriptVariableDoesNotEqual($variable, $value) {
    throw new PendingException();
  }

  /**
   * @Given I take a dump
   */
  public function iTakeADump() {
    var_dump($this->params);
  }

  /**
   * @Given /^I send a GET request to Access Token Request URL$/
   */
  public function iSendAGETRequestToAccessTokenRequestURL() {
    module_load_include('inc', 'marketo_rest', 'includes/marketo_rest.rest');
    module_load_include('inc', 'marketo_rest', 'tests/includes/marketo_rest.mock');

    $client_secret = variable_get('marketo_rest_client_secret');
    $client_id = variable_get('marketo_rest_client_id');
    $rest_endpoint = variable_get('marketo_rest_endpoint');
    $rest_identity = variable_get('marketo_rest_identity');
    $rest_token = variable_get('marketo_rest_token');

    try {
      $this->client = new MarketoMockClient($client_id, $client_secret, $rest_endpoint, $rest_identity, _marketo_rest_rest_proxy_settings(), $rest_token);
    }
    catch (Exception $e) {
      throw new PendingException();
    }
  }

  /**
   * @Then /^I have a valid access token value$/
   */
  public function iHaveAValidAccessTokenValue() {
    $expected_value = 'test';
    try {
      $token = $this->client->getAccessToken();
      if ($token != $expected_value) {
        $message = sprintf('Token: "%s" was not expected: "%s"', $token, $expected_value);
        throw new \Exception($message);
      }
    }
    catch (Exception $e) {
      throw new PendingException();
    }
  }

  /**
   * @Given /^I have a valid token expiry value$/
   */
  public function iHaveAValidTokenExpiryValue() {
    $expected_value = '100';
    try {
      $expiry = $this->client->getAccessTokenExpiry();
      if ($expiry != $expected_value) {
        $message = sprintf('Token Expiry: "%s" was not expected: "%s"', $expiry, $expected_value);
        throw new \Exception($message);
      }
    }
    catch (Exception $e) {
      throw new PendingException();
    }
  }

  /**
   * @When /^I receive the access_token response string$/
   */
  public function iReceiveTheAccess_tokenResponseString() {
    throw new PendingException();
  }

  /**
   * @When I save the valid access_token response data
   */
  public function iSaveTheValidAccessTokenResponseData()
  {
    throw new PendingException();
  }

  /**
   * @Then I should have a cached version of the :arg1
   */
  public function iShouldHaveACachedVersionOfThe($arg1)
  {
    throw new PendingException();
  }

  /**
   * @Then I should have stored the calculated :arg1
   */
  public function iShouldHaveStoredTheCalculated($arg1)
  {
    throw new PendingException();
  }

}
