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
use Coduo\PHPMatcher\Factory\SimpleFactory;

class FeatureContext extends RawDrupalContext implements SnippetAcceptingContext {
  private $params = array();

  /** @var DrupalContext */
  private $drupalContext;

  /** @var DrushContext */
  private $drushContext;

  /** @var MarketoMockClient */
  private $client;

  // Persist POST data, request and response.
  private $data;
  private $request;
  private $response;

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
   * @Given I have instantiated the Marketo rest client using :config
   */
  public function iHaveInstantiatedTheMarketoRestClientUsing($config = 'marketo_default_settings') {
    module_load_include('inc', 'marketo_rest', 'includes/marketo_rest.rest');

    // Set default as the rest class.
    $clientClass = 'MarketoRestClient';

    // If we are not using the REST mock client.
    if(!empty($this->params[$config]['marketo_rest_mock'])) {
      $clientClass = 'MarketoMockClient';
      module_load_include('inc', 'marketo_rest', 'tests/includes/marketo_rest.mock');
    }

    // Instantiate our client class with our init values.
    try {
      $this->client = new $clientClass(
        $this->params[$config]['marketo_rest_client_id'],
        $this->params[$config]['marketo_rest_client_secret'],
        $this->params[$config]['marketo_rest_endpoint'],
        $this->params[$config]['marketo_rest_identity']
      );
    }
    catch (Exception $e) {
      throw new Exception('Could not instantiate a new "'. $clientClass . '"" class.');
    }
  }

  /**
   * @Given /^I request an access token$/
   */
  public function iRequestAnAccessToken() {
    try {
      $this->client->getAccessToken();
    }
    catch (Exception $e) {
      throw new Exception('Error requesting access token.');
    }
  }

  /**
   * @Then I should have generated a valid Access Token Request URL
   */
  public function iShouldHaveGeneratedAValidAccessTokenRequestURL() {
    $options = $this->client->getIdentityTokenOptions();
    $query = $this->client->buildQuery($options);
    $expected_value = $this->client->getIdentityEndpoint() . '/' . MARKETO_REST_IDENTITY_API . '?' . $query;
    try {
      $request = json_decode($this->client->getLastRequest());
      if ($request->url != $expected_value) {
        $message = sprintf('URL: "%s" was not expected: "%s"', $request['url'], $expected_value);
        throw new \Exception($message);
      }
      $this->response = $this->client->getLastResponse();
    }
    catch (Exception $e) {
      throw new Exception('Could not access previous request data.');
    }
  }

  /**
   * @Then the response should contain json:
   */
  public function theResponseShouldContainJson(PyStringNode $string) {
    try {
      $expected_value = $string->getRaw();
      $response = json_decode($this->response);
      $factory = new SimpleFactory();
      $matcher = $factory->createMatcher();
      // Use the pattern matcher to verify JSON.
      if(!$matcher->match($response->data, $expected_value)) {
        $message = sprintf('JSON mismatch: ', $matcher->getError());
        throw new \Exception($message);
      }
    }
    catch (Exception $e) {
      throw new Exception('Could not access previous request data.');
    }
  }

  /**
   * @Then I have stored the access token: :expected_token
   */
  public function iHaveStoredTheAccessToken($expected_token) {
    try {
      $token = $this->client->getStoredAccessToken();
      if ($token != $expected_token) {
        $message = sprintf('Token: "%s" was not expected: "%s"', $token, $expected_token);
        throw new \Exception($message);
      }
    }
    catch (Exception $e) {
      throw new Exception('Could not access token.');
    }
  }

  /**
   * @Then /^I have stored a valid token expiry timestamp/
   */
  public function iHaveStoredAValidTokenExpiryTimestamp() {
    try {
      $expiry = $this->client->getStoredAccessTokenExpiry();
      $now = time();
      if ($now >= $expiry) {
        $message = sprintf('Token Expired: current timestamp "%s" after token expired timestamp: "%s"', $now, $expiry);
        throw new \Exception($message);
      }
    }
    catch (Exception $e) {
      throw new Exception('Could not access token expiry.');
    }
  }

  /**
   * @Given I request all fields on the lead object
   */
  public function iRequestAllFieldsOnTheLeadObject() {
    try {
      $this->client->getFields();
      $this->response = $this->client->getLastResponse();
    }
    catch (Exception $e) {
      throw new Exception('Could not set token and expiry.');
    }
  }

  /**
   * @Then the response should have success :arg1 and element :arg2 containing json:
   *
   * @param $arg1
   * @param $arg2
   * @param \Behat\Gherkin\Node\PyStringNode $string
   * @throws \Exception
   */
  public function theResponseShouldHaveSuccessAndContainJson($arg1, $arg2, PyStringNode $string) {
    try {
      $expected_value = $string->getRaw();

      // Extract the elements we want to check.
      $data = json_decode($this->response);
      $element = json_encode($data->{$arg2}[0]);
      $success = $data->{'success'};

      $factory = new SimpleFactory();
      $matcher = $factory->createMatcher();
      // Use the pattern matcher to verify JSON.
      if($success == $arg1 && !$matcher->match($element, $expected_value)) {
        $message = sprintf('JSON mismatch: ', $matcher->getError());
        throw new \Exception($message);
      }
    }
    catch (Exception $e) {
      throw new Exception('Could not access previous request data. ' . $e->getMessage());
    }
  }

  /**
   * @Given /^I have the action \'([^\']*)\' and the lookupField: \'([^\']*)\'$/
   */
  public function iHaveTheActionAndTheLookupField($arg1, $arg2) {
    $this->data = (object) ['action' => $arg1, 'lookupField' => $arg2];
  }

  /**
   * @Given /^I have the input:$/
   */
  public function iHaveTheInput(TableNode $table) {
    // Ensure our data object is created.
    if (empty($this->data) || !is_object($this->data)) {
      $this->data = new stdClass();
    }
    $this->data->input = $this->getAssocDataArrayObjs($table->getRows());
  }

  /**
   * @When /^I sync leads$/
   */
  public function iSyncLeads() {
    try {
      $this->response = $this->client->syncLead($this->data->input, $this->data->action, $this->data->lookupField);
    }
    catch (Exception $e) {
      throw new Exception($e->getMessage());
    }
  }

  /**
   * @Then /^the return value \'([^\']*)\' should be \'([^\']*)\'$/
   */
  public function theReturnValueShouldBe($arg1, $arg2) {
    try {
      // Use the pattern matcher to verify JSON.
      if(!$this->response[$arg1] == $arg2) {
        throw new \Exception($this->response['result']);
      }
    }
    catch (Exception $e) {
      throw new Exception($e->getMessage());
    }
  }

  /**
   * @When /^I get leads$/
   */
  public function iGetLeads() {
    try {
      if (empty($this->data)) {
        throw new Exception('No lead data found in request.');
      }
      $values = array();
      foreach ($this->data->input as $value) {
        $values[] = $value->filterValues;
      }
      $this->response = $this->client->getLead($values, $this->data->input[0]->filterType);
    }
    catch (Exception $e) {
      throw new Exception($e->getMessage());
    }
  }

  /**
   * @Then /^I should have a lead with \'([^\']*)\' equal to \'([^\']*)\'$/
   */
  public function iShouldHaveALeadWithEqualTo($arg1, $arg2) {
    try {
      // Check response for lead data.
      if (!empty($this->response) && is_array($this->response)) {
        foreach ($this->response as $result) {
          if ($result[$arg1] != $arg2) {
            throw new Exception('Value for "' . $arg1 . '"" does not match "' . $arg2 . '".');
          }
        }
      }
    }
    catch (Exception $e) {
      throw new Exception($e->getMessage());
    }
  }

  /**
   * @When /^I request a paging token$/
   */
  public function iRequestAPagingToken() {
    try {
      $this->client->getPagingToken();
      $this->response = (array) json_decode($this->client->getLastResponse());
    }
    catch (Exception $e) {
      throw new Exception($e->getMessage());
    }
  }

  /**
   * @Given /^I should have a page token containing \'([^\']*)\'$/
   */
  public function iShouldHaveAPageTokenContaining($arg1) {
    try {
      $token = $this->client->getPagingToken();
      $factory = new SimpleFactory();
      $matcher = $factory->createMatcher();
      // Use the pattern matcher to verify token.
      if(!$matcher->match($token, "@string@.contains('" . $arg1 . "')")) {
        throw new Exception('Token "' . $token . '" does not contain expected "' . $arg1 . '"');
      }
    }
    catch (Exception $e) {
      throw new Exception($e->getMessage());
    }
  }

  /**
   * @Given /^I have a valid paging token$/
   */
  public function iHaveAValidPagingToken() {
    try {
      if(!$this->client->getPagingToken()) {
        throw new Exception('Paging token not found.');
      }
    }
    catch (Exception $e) {
      throw new Exception($e->getMessage());
    }
  }

  /**
   * @When /^I request activity types$/
   * @Given /^I have stored activity types$/
   */
  public function iRequestActivityTypes() {
    try {
      $this->client->getActivityTypes();
      $this->response = (array) json_decode($this->client->getLastResponse());
    }
    catch (Exception $e) {
      throw new Exception($e->getMessage());
    }
  }

  /**
   * @Given /^I should have stored an array of activity types$/
   */
  public function iShouldHaveStoredAnArrayOfActivityTypes() {
    try {
      $types = $this->client->getActivityTypes();
      // Check we have a types array with min one entry with an id attribute.
      if (!is_array($types) || empty($types[0]['id'])) {
        throw new Exception('Activity types were not stored in the expected format.');
      }
    }
    catch (Exception $e) {
      throw new Exception($e->getMessage());
    }
  }

  /**
   * @When /^I request all activities for a lead with field: \'([^\']*)\' equal to \'([^\']*)\'$/
   */
  public function iRequestAllActivitiesForALeadWithFieldEqualTo($arg1, $arg2) {
    try {
      $this->client->getAllLeadActivities($arg2, $arg1);
      $this->response = (array) json_decode($this->client->getLastResponse());
    }
    catch (Exception $e) {
      throw new Exception($e->getMessage());
    }
  }

  /**
   * Get a new webform node.
   *
   * @return \stdClass
   */
  private function createWebform() {
    // Create webform node following: https://www.drupal.org/node/2030697.
    $node = new stdClass();
    $node->type = 'webform';
    node_object_prepare($node);
    $node->title = 'Marketo REST test Webform';
    $node->language = 'en';
    $node->body[LANGUAGE_NONE][0]['value']   = '';
    $node->body[LANGUAGE_NONE][0]['format']  = 'full_html';
    $node->uid = 1;
    $node->promote = 0;
    $node->comment = 0;

    // Create the webform components.
    $components = array(
      1 => array(
        'name' => 'Email',
        'form_key' => 'marketo_rest_email',
        'type' => 'textfield',
        'mandatory' => 1,
        'weight' => 0,
        'pid' => 0,
        'extra' => array(
          'title_display' => 'inline',
          'private' => 0,
          'aslist' => 1,
        ),
      ),
      2 => array(
        'name' => 'Company name',
        'form_key' => 'marketo_rest_company_name',
        'type' => 'textfield',
        'mandatory' => 1,
        'weight' => 5,
        'pid' => 0,
        'extra' => array(
          'title_display' => 'inline',
          'private' => 0,
        ),
      ),
      3 => array(
        'name' => 'Website URL',
        'form_key' => 'marketo_rest_site',
        'type' => 'textfield',
        'mandatory' => 1,
        'weight' => 10,
        'pid' => 0,
        'extra' => array(
          'title_display' => 'inline',
          'private' => 0,
        ),
      ),
    );

    // Setup notification email.
    $emails = array(
      array(
        'email' => 'admin@marketo-rest.com',
        'subject' => 'default',
        'from_name' => 'default',
        'from_address' => 'default',
        'template' => 'default',
        'excluded_components' => array(),
      ),
    );

    // Attach the webform to the node.
    $node->webform = array(
      'confirmation' => '',
      'confirmation_format' => NULL,
      'redirect_url' => '',
      'status' => '1',
      'block' => '0',
      'teaser' => '0',
      'allow_draft' => '0',
      'auto_save' => '0',
      'submit_notice' => '1',
      'submit_text' => '',
      'submit_limit' => '-1', // User can submit more than once.
      'submit_interval' => '-1',
      'total_submit_limit' => '-1',
      'total_submit_interval' => '-1',
      'record_exists' => TRUE,
      'roles' => array(
        0 => '1', // Anonymous user can submit this webform.
      ),
      'emails' => $emails,
      'components' => $components,
    );

    // Save the node.
    node_save($node);

    return $node;
  }

  /**
   * Generate an assoc array from table rows.
   *
   * @param $rows
   * @return array
   */
  private function getAssocDataArrayObjs($rows) {
    $header = array_shift($rows);
    $input = array();

    // Generate array of input objects.
    foreach ($rows as $num => $row) {
      $input[$num] = new stdClass();
      foreach ($row as $key => $field) {
        $input[$num]->{$header[$key]} = $field;
      }
    }
    return $input;
  }

  /**
   * @Given /^I have created a webform node with the mapped fields:$/
   */
  public function iHaveCreatedAWebformNodeWithTheMappedFields(TableNode $table) {
    try {
      // Create a webform node representing our from.
      $node = $this->createWebform();
      $data = $this->getAssocDataArrayObjs($table->getRows());
      // Map a component to a marketo field.
      $this->mapMarketoFields($node, (array) array_shift($data));
      $this->data = $node;
    }
    catch (Exception $e) {
      throw new Exception($e->getMessage());
    }
  }

  /**
   * Map the Marketo fields on the webform node.
   *
   * @param \stdClass $node
   * @param array $mapped_fields
   */
  public function mapMarketoFields(stdClass $node, array $mapped_fields) {
    $default_options = array(MARKETO_REST_WEBFORM_COMPONENT_NONE => '- None -');
    $marketo_options = _marketo_rest_get_field_options();
    $options = array_merge($default_options, $marketo_options);
    $fieldmap = _marketo_rest_webform_get_mapped_fields($node->nid);
    $form = array();

    // Cycle through components and get / set field mapping.
    foreach ($node->webform['components'] as $cid => $component) {
      $form['components'][$cid] = array(
        '#title' => filter_xss($component['name'] . ' (' . $component['form_key'] . ')'),
        '#title_display' => 'invisible',
        '#type' => 'select',
        '#options' => $options,
      );
      if (array_key_exists($cid, $fieldmap)) {
        if (!array_key_exists($fieldmap[$cid], $options)) {
          $form['components'][$cid]['#options'] = array_merge(
            array($fieldmap[$cid] => "Undefined Field ($fieldmap[$cid])"), $options
          );
        }
        $form['components'][$cid]['#default_value'] = $fieldmap[$cid];
      }
      else {
        // This is the likely case in test env; map our fields in order.
        $form['components'][$cid]['#default_value'] = array_shift($mapped_fields);
      }
    }

    db_merge(MARKETO_REST_SCHEMA_WEBFORM)
      ->key(array('nid' => $node->nid))
      ->fields(array(
        MARKETO_REST_WEBFORM_FIELD_ACTIVE => TRUE,
        MARKETO_REST_WEBFORM_OPTIONS => serialize($options),
      ))
      ->execute();

    // Set the component id to map to Marketo id.
    foreach ($form['components'] as $cid => $marketo) {
      db_merge(MARKETO_REST_SCHEMA_WEBFORM_COMPONENT)
        ->key(array(
          'nid' => $node->nid,
          'cid' => $cid,
        ))
        ->fields(array(
          MARKETO_REST_LEAD_FIELD_ID => $marketo['#default_value'],
        ))
        ->execute();
    }

    // Set "Capture Data" to active.
    db_merge(MARKETO_REST_SCHEMA_WEBFORM)
      ->key(array('nid' => $node->nid))
      ->fields(array('is_active' => 1))
      ->execute();
  }

  /**
   * @When /^I submit a webform node with the input:$/
   */
  public function iSubmitAWebformNodeWithTheInput(TableNode $table) {
    try {
      $node = $this->data;
      $data = $this->getAssocDataArrayObjs($table->getRows());
      // Get webform submission data.
      $submission = $this->getWebformData($node, (array) array_shift($data));
      // Submit webform programmatically.
      if (!webform_submission_insert($node, $submission)) {
        throw new Exception('Could not create webform.');
      }
      global $user;
      $marketo_rest_data = marketo_rest_get_queue();

      if ((_marketo_rest_visibility_pages() && _marketo_rest_visibility_roles($user)) || count($marketo_rest_data) > 0) {

        /*
         * @todo handle case where visibility = false, count > 0, and
         * tracking type != munchkin.. we don't need any tracking in this case
         */
        // Basic Munchkin tracking.
        _marketo_rest_output_tracking_code();

        $tracking_method = variable_get('marketo_rest_tracking_method', MARKETO_REST_TRACKING_METHOD_DEFAULT);
        $email_key = MarketoRestData::getEmailKey($tracking_method);

        foreach ($marketo_rest_data as $lead) {
          if (array_key_exists($email_key, $lead)) {
            $this->client = _marketo_rest_associate_lead_rest($lead);
          }
        }

        _marketo_rest_cleanup();
      }
      if (!$response = $this->client->getLastResponse()) {
        throw new Exception('No response could be set.');
      }
      $this->response = (array) json_decode($this->client->getLastResponse());
    }
    catch (Exception $e) {
      throw new Exception($e->getMessage());
    }
  }

  /**
   * Generate webform data based on mapped array data.
   *
   * @param $node
   * @param $data
   * @return object
   */
  private function getWebformData($node, $data) {
    global $user;

    module_load_include('inc', 'webform', 'webform.module');
    module_load_include('inc', 'webform', 'includes/webform.submissions');

    // This methods will arrange $data in the right way
    $data = _webform_client_form_submit_flatten($node, $data);
    $data = webform_submission_data($node, $data);

    return (object) array(
      'nid' => $node->nid,
      'uid' => $user->uid,
      'sid' => NULL,
      'submitted' => REQUEST_TIME,
      'completed' => REQUEST_TIME,
      'remote_addr' => ip_address(),
      'is_draft' => FALSE,
      'data' => $data,
    );
  }

  /**
   * @Given /^I delete the test data$/
   */
  public function iDeleteTheTestData() {
    try {
      if (!empty($this->data->nid)) {
        node_delete($this->data->nid);
      }
    }
    catch (Exception $e) {
      throw new Exception($e->getMessage());
    }
  }

  /**
   * @Then /^I should see have field definition:$/
   */
  public function iShouldSeeHaveFieldDefinition(TableNode $table)
  {
    try {
      $rows = $this->getAssocDataArrayObjs($table->getRows());
      $field_definitions = _marketo_rest_get_field_definitions();
      foreach ($rows as $fields) {
        foreach ($fields as $key => $value) {
          if ($field_definitions[$fields->marketo_id]->{$key} != $value) {
            throw new Exception('Field definitions don\'t contain expected data.');
          }
        }
      }
    }
    catch (Exception $e) {
      throw new Exception($e->getMessage());
    }
  }

  /**
   * @Given /^the database table \'([^\']*)\' with primary key \'([^\']*)\' contains:$/
   */
  public function theDatabaseTableWithPrimaryKeyContains($arg1, $arg2, TableNode $table)
  {
    try {
      $fields = $this->getAssocDataArrayObjs($table->getRows());
      // Check if we have a table check we have an entry for our primary key.
      foreach ($fields as $name => $field) {
        $result = db_select($arg1, 'f')
          ->fields('f')
          ->condition($arg2, $field->{$arg2})
          ->execute()
          ->fetchAll();
        // Add an entry if not.
        if (empty($result)) {
          db_insert($arg1)
            ->fields($field)
            ->execute();
        }
        else {
          // For our lead field db test update our enabled field.
          if ($arg1 == MARKETO_REST_SCHEMA_LEAD_FIELDS) {
            db_update($arg1)
              ->fields(array(
                'enabled' => $field->enabled,
              ))
              ->condition($arg2, $field->{$arg2})
              ->execute();
          }
        }
      }
    }
    catch (Exception $e) {
      throw new Exception($e->getMessage());
    }
  }

}
