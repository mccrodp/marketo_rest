<?php

/**
 * @file
 * Class and methods to mock interacting with the Marketo REST API.
 */

/**
 * Marketo Mock Client
 */
class MarketoMockClient implements MarketoRestInterface {

  // Identity and endpoints.
  protected $clientId;
  protected $clientSecret;
  protected $endpoint;
  protected $identity;

  // Temporary access token.
  private $access_token;
  private $access_token_expiry;

  // Latest request and response.
  private $request;
  private $response;

  /**
   * Constructor for MarketoClient class.
   * 
   * @param string $client_id
   *   A Client ID defined in Marketo admin
   * @param string $client_secret
   *   A secret-key defined in Marketo admin
   * @param string $endpoint
   *   REST endpoint URL
   * @param $identity
   *   REST identity endpoint URL
   * @param array $options
   *   Optional array of SoapClient options
   * @param string|null $token
   *   REST access token
   */
  public function __construct($client_id, $client_secret, $endpoint, $identity, $options = array(), $token = null) {
    // When any of our required params are empty, throw exception.
    if (empty($client_id) || empty($client_secret) || empty($endpoint) || empty($identity)) {
      throw new Exception(t('One of the fields were empty: Client Id, Client Secret, Endpoint, Identity'));
    }
    $this->clientId = $client_id;
    $this->clientSecret = $client_secret;
    $this->endpoint = $endpoint;
    $this->identity = $identity;
    $this->getAccessToken($options, $token);
  }

  /**
   * Get new REST access token.
   *
   * @param array|null $options
   * @param array|null $token
   *   Full access token JSON.
   * @return bool
   */
  public function getAccessToken(array $options = array(), array $token = null) {
    // Load the access token from the (JSON) full token.
    if ($token) {
      if (!empty($token['access_token'])) {
        // Set new access token.
        $this->access_token = $token['access_token'];
      }
      if (!empty($token['token_expiry'])) {
        $this->access_token_expiry = $token['token_expiry'];
      }
    }
    // Check if we have an access token or if token is valid.
    if (!$this->access_token || $this->isAccessTokenExpired()) {
      // Query Marketo for token.
      $default_options = array(
        'client_id' => $this->clientId,
        'client_secret' => $this->clientSecret,
        'grant_type' => 'client_credentials',
      );

      $rest_options = array_merge($default_options, $options);
      $response = $this->requestToken(MARKETO_REST_IDENTITY_API, $rest_options, 'GET', $this->getIdentityEndpoint());

      // Get the JSON access token from response.
      if ($response->data) {
        $token = drupal_json_decode($response->data);
        if (!empty($token['access_token'])) {
          // Set new access token.
          $this->access_token = $token['access_token'];
          $this->access_token_expiry = $this->calculateTokenExpiry($token['expires_in']);
        }
      }
    }

    return $this->access_token;
  }

  /**
   * Get the expiry time of access token.
   *
   * @return mixed
   */
  public function getAccessTokenExpiry() {
    return $this->access_token_expiry;
  }

  /**
   * Check if the given / current token is expired.
   *
   * @return bool
   */
  public function isAccessTokenExpired() {
    // Check token expiry greater than current time.
    if ($this->access_token_expiry && $this->access_token_expiry > time()) {
      // Token still valid.
      return FALSE;
    }
    // Token expired.
    return TRUE;
  }

  /**
   * Determines lead key type for a given key.
   *
   * @param string $key
   *   The key to examine
   *
   * @return string
   *   Lead key type
   */
  protected function keyType($key) {
    // If the key is an array, just check the first item.
    if (is_array($key)) {
      $key = array_shift($key);
    }

    if (filter_var($key, FILTER_VALIDATE_EMAIL)) {
      $type = 'EMAIL';
    }
    elseif (is_int($key) || (is_string($key) && ctype_digit($key))) {
      $type = 'IDNUM';
    }
    elseif (filter_var($key, FILTER_VALIDATE_REGEXP, array('options' => array('regexp' => '/^id:.*&token:.*/')))) {
      $type = 'COOKIE';
    }
    else {
      $type = 'UNKNOWN';
    }

    return $type;
  }

  /**
   * Returns the last REST request as JSON string.
   * 
   * @return string
   */
  public function getLastRequest() {
    return $this->request;
  }

  /**
   * Returns the last REST response as json string.
   *
   * @return string
   */
  public function getLastResponse() {
    return $this->response;
  }

  /**
   * {@inheritdoc}
   */
  public function getLead($values, $type = null) {
    // If we have no filterType key, determine key.
    if (!$type) {
      $type = $this->keyType($values);
    }

    $params = array(
      'filterType' => $type,
      'filterValues' => is_array($values) ? implode(',', $values) : $values,
    );

    try {
      $result = $this->request(MARKETO_REST_LEADS_API, $params);
      $leads = $this->prepareLeadResults($result);
    }
    catch (Exception $e) {

      if (isset($e->detail->serviceException->code) && $e->detail->serviceException->code == MarketoRestError::ERR_LEAD_NOT_FOUND) {
        // No leads were found.
        $leads = array();
      }
      else {
        throw new Exception($e);
      }
    }

    return $leads;
  }

  /**
   * {@inheritdoc}
   */
  public function getLeadActivity($ids, $token, $lead_ids = null, $list_id = null, $batch_size = 300) {
    try {
      $result = $this->request(MARKETO_REST_ACTIVITIES_API);
      $activity = $this->prepareLeadActivityResults($result);
    }
    catch (Exception $e) {

      if (isset($e->detail->serviceException->code) && $e->detail->serviceException->code == MarketoRestError::ERR_LEAD_NOT_FOUND) {
        // No leads were found.
        $activity = array();
      }
      else {
        throw new Exception($e);
      }
    }

    return $activity;
  }

  /**
   * {@inheritdoc}
   */
  public function getFields() {
    try {
      $result = $this->request('leads/describe.json');
      $fields = $this->prepareResults($result->data);
    }
    catch (Exception $e) {
      $fields = array();
    }

    return $fields;
  }

  /**
   * {@inheritdoc}
   */
  public function syncLead($input, $action = 'createOrUpdate', $field = null, $async = FALSE, $partition = null) {
    // Generate an array of leads.
    $params = array(
      'input' => $this->leadRecord($input, $field),
      'action' => $action,
      'lookupField' => $field,
      'asyncProcessing' => $async,
      'partitionName' => $partition,
    );

    try {
      $response = $this->request(MARKETO_REST_LEADS_API, $params);
      $result = $this->prepareResults($response);
      $ret['success'] = $result['success'];
    }
    catch (Exception $e) {
      $ret['result'] = $e->getMessage();
      $ret['success'] = FALSE;
    }
    return $ret;
  }

  /**
   * Converts response into a more useful structure.
   * 
   * @param string $data
   *   LeadRecord object description
   * 
   * @return array
   *   Key value pairs of fields
   */
  protected function prepareResults($data) {
    return drupal_json_decode($data);
  }

  /**
   * Creates proper structure for submitting a lead.
   * 
   * @param array $attributes
   *   Key value pairs to be associated withi this lead
   * @param string $key
   *   Lead Key
   * 
   * @return \stdClass
   *   Lead Record
   */
  protected function leadRecord($attributes, $key = NULL) {
    $record = new stdClass();

    if ($key) {
      if (is_numeric($key)) {
        $record->Id = $key;
      }
      else {
        $record->Email = $key;
      }
    }

    $record->leadAttributeList = new stdClass();
    $record->leadAttributeList->attribute = array();

    foreach ($attributes as $attribute => $value) {
      $lead_attribute = new stdClass();
      $lead_attribute->attrName = $attribute;
      $lead_attribute->attrValue = $value;

      array_push($record->leadAttributeList->attribute, $lead_attribute);
    }

    return $record;
  }

  /**
   * Parses lead results into a more useful format.
   * 
   * @param object $marketo_result
   *   REST response
   * 
   * @return array
   *   An array of objects defining lead data
   */
  protected function prepareLeadResults($marketo_result) {
    if ($marketo_result->result->count == 1) {
      $leads[] = $marketo_result->result->leadRecordList->leadRecord;
    }
    elseif ($marketo_result->result->count > 1) {
      $leads = $marketo_result->result->leadRecordList->leadRecord;
    }
    else {
      $leads = array();
    }

    foreach ($leads as &$lead) {
      $lead->attributes = array();
      foreach ($lead->leadAttributeList->attribute as $attribute) {
        $lead->attributes[$attribute->attrName] = $attribute->attrValue;
      }
      unset($lead->leadAttributeList);
    }

    return $leads;
  }

  /**
   * Parses lead activity results into a more useful format.
   * 
   * @param object $marketo_result
   *   REST response
   * 
   * @return array
   *   An array of objects defining lead activity data
   */
  protected function prepareLeadActivityResults($marketo_result) {
    if ($marketo_result->leadActivityList->returnCount > 1) {
      $activity = $marketo_result->leadActivityList->activityRecordList->activityRecord;
    }
    elseif ($marketo_result->leadActivityList->returnCount == 1) {
      $activity[] = $marketo_result->leadActivityList->activityRecordList->activityRecord;
    }
    else {
      $activity = array();
    }

    foreach ($activity as &$event) {
      $event->attributes = array();
      foreach ($event->activityAttributes->attribute as $attribute) {
        $event->attributes[$attribute->attrName] = $attribute->attrValue;
      }
      unset($event->activityAttributes);
    }

    return $activity;
  }

  /**
   * Get identity endpoint as per Marketo docs.
   *
   * @return string
   */
  private function getIdentityEndpoint() {
    return $this->identity;
  }

  /**
   * Calculate and return token expiry time based on time from now in seconds.
   *
   * @param $expires_in
   * @return int
   */
  private function calculateTokenExpiry($expires_in) {
    return time() + $expires_in;
  }

  /**
   * Generate a build query.
   *
   * @param $params
   * @return string
   */
  private function buildQuery($params) {
    return drupal_http_build_query($params);
  }

  /**
   * Get the full REST API Endpoint.
   *
   * Rest endpoint with API version.
   *
   * @return string
   */
  private function getFullRestEndpoint() {
    return $this->endpoint . '/' . MARKETO_REST_API_VERSION;
  }

  private function requestToken($MARKETO_REST_IDENTITY_API, $rest_options, $string, $getIdentityEndpoint) {
    // @todo: return a JSON token.
  }

}
