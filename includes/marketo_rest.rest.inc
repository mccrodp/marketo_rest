<?php

/**
 * @file
 * Class and methods for interacting with the Marketo REST API.
 */

/**
 * Marketo Interface
 */
interface MarketoRestInterface {

  /**
   * Retrieves list of defined fields.
   */
  public function getFields();

  /**
   * Retrieves lead information.
   * 
   * @param string $type
   *   Filter (key) type, typically email address
   * @param string $values
   *   Filter values
   */
  public function getLead($values, $type = null);

  /**
   * Retrieves activity information.
   *
   * @param string $ids
   *   Comma-separated list of activity ids
   * @param string $token
   *   Next page token from getPagingToken call
   * @param string|null $lead_ids
   *   A comma-separated list of lead IDs.
   * @param string|null $list_id
   *   For leads with in this given list
   * @param int $batch_size
   *   Default and max = 300
   * @return mixed
   */
  public function getLeadActivity($ids, $token, $lead_ids = null, $list_id = null, $batch_size = 300);

  /**
   * Inserts or updates a lead.
   *
   * @param $input
   *   Array of lead records to create or update
   * @param string $action
   *   Request type: createOnly, updateOnly, createOrUpdate, createDuplicate
   * @param null $field
   *   Lookup field used to find duplicate, defaults to email if unspecified
   * @param bool $async
   *   Used to specify post processing when calling the creating/updating leads
   * @param null $partition
   *   If instance has lead partitions set up, you need to specify partitionName
   * @return mixed
   */
  public function syncLead($input, $action = 'createOrUpdate', $field = null, $async = FALSE, $partition = null);
}

/**
 * Marketo REST Client
 */
class MarketoRestClient implements MarketoRestInterface {

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
   * @throws Exception
   * @return MarketoRestClient
   */
  public function __construct($client_id, $client_secret, $endpoint, $identity) {
    // When any of our required params are empty, throw exception.
    if (empty($client_id) || empty($client_secret) || empty($endpoint) || empty($identity)) {
      throw new Exception(t('One of the fields were empty: Client Id, Client Secret, Endpoint, Identity'));
    }
    $this->clientId = $client_id;
    $this->clientSecret = $client_secret;
    $this->endpoint = $endpoint;
    $this->identity = $identity;

    return $this;
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
      $this->loadStoredAccessToken($token);
    }

    // Check if we have an access token or if token is valid.
    if (!$this->access_token || $this->isAccessTokenExpired()) {
      $this->getNewAccessToken($options);
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
   * Load access token values from array into properties.
   *
   * @param array $token
   */
  protected function loadStoredAccessToken(array $token) {
    if (!empty($token['access_token']) && !empty($token['token_expiry'])) {
      // Set new access token.
      $this->access_token = $token['access_token'];
      $this->access_token_expiry = $token['token_expiry'];
    }
  }

  /**
   * Get the stored access token value.
   *
   * @return mixed
   */
  public function getStoredAccessToken() {
    return $this->access_token;
  }

  /**
   * Set the last REST request as JSON string.
   *
   * @param $request
   * @return mixed
   */
  public function setLastRequest($request) {
    return $this->request = $request;
  }

  /**
   * Returns the last REST response as json string.
   *
   * @param $response
   * @return mixed
   */
  public function setLastResponse($response) {
    return $this->response = $response;
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
   * Extract the token data from the response array and load into properties.
   *
   * @param $data
   */
  protected function extractTokenDataFromResponse($data) {
    // Get the JSON access token from response string.
    $token = drupal_json_decode($data);
    if (!empty($token['access_token'])) {
      // Set new access token.
      $this->access_token = $token['access_token'];
      $this->access_token_expiry = $this->calculateTokenExpiry($token['expires_in']);
    }
  }

  /**
   * Get the stored access token expiry value.
   *
   * @return mixed
   */
  public function getStoredAccessTokenExpiry() {
    return $this->access_token_expiry;
  }

  /**
   * Get the options to send in access token request.
   *
   * @param array $options
   * @return array
   */
  protected function getIdentityTokenOptions(array $options) {
    // Query Marketo for token.
    $rest_options = array(
      'client_id' => $this->clientId,
      'client_secret' => $this->clientSecret,
      'grant_type' => 'client_credentials',
    );

    // Merge extra options if present.
    if ($options) {
      $rest_options = array_merge($rest_options, $options);
    }

    return $rest_options;
  }

  /**
   * Get identity endpoint as per Marketo docs.
   *
   * @return string
   */
  protected function getIdentityEndpoint() {
    return $this->identity;
  }

  /**
   * Get the full REST API Endpoint.
   *
   * Rest endpoint with API version.
   *
   * @return string
   */
  protected function getFullRestEndpoint() {
    return $this->endpoint . '/' . MARKETO_REST_API_VERSION;
  }

  /**
   * Generate a build query.
   *
   * @param $params
   * @return string
   */
  protected function buildQuery($params) {
    return drupal_http_build_query($params);
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
   * Make a REST request to the server.
   *
   * @param $url
   * @param $options
   * @return object
   */
  private function restCall($url, $options) {
    // Record the latest request.
    $this->request = drupal_json_encode(
      array(
        'url' => $url,
        'options' => $options
    ));
    $response = drupal_http_request($url, $options);
    // Record the latest response.
    $this->response = drupal_json_encode($response);
    return $response;
  }

  /**
   * Request a new access token.
   *
   * @param array $options
   */
  private function getNewAccessToken(array $options) {
    // Send a GET request for the access token.
    $response = $this->request(
      MARKETO_REST_IDENTITY_API,
      $this->getIdentityTokenOptions($options),
      'GET',
      $this->getIdentityEndpoint()
    );

    // Extract response data into properties and return raw data json string.
    $this->extractTokenDataFromResponse($response->data);
  }

  /**
   * Constructs the request to create the REST call to Marketo.
   *
   * @param $api
   *   The API endpoint
   * @param array $params
   *   Parameters to be sent with the request
   * @param string $operation
   *   The operation to execute
   * @param null|string $url
   * @return object
   *   Response object
   */
  private function request($api, $params = array(), $operation = 'GET', $url = null) {

    $options = array();

    // Set our REST endpoint if empty.
    if (!$url) {
      $url = $this->getFullRestEndpoint();
    }

    // Add the access token unless we are querying for access token.
    if ($api != MARKETO_REST_IDENTITY_API && $url != $this->identity) {
      $params += array('access_token' => $this->access_token);
    }

    // Add the api endpoint to the url.
    $url = $url . '/' . $api;

    // Depending on operation we construct the REST call differently.
    switch ($operation) {
      case 'POST':
        // Add the API endpoint uri to the URL.
        $query = $this->buildQuery($params);
        $options = array(
          'method' => $operation,
          'data' => $query,
        );
        break;
      case 'GET':
      default:
        // GET requires query string rather than params in the body.
        $url = $url . '?' . $this->buildQuery($params);
        break;
    }
    return $this->restCall($url, $options);
  }

}

class MarketoRestError {

  const ERR_REQ_ENTITY_TOO_LARGE = 413;
  const ERR_REQ_URI_TOO_LONG = 414;
  const ERR_BAD_GATEWAY = 502;
  const ERR_ACCESS_TOKEN_EMPTY = 600;
  const ERR_ACCESS_TOKEN_INVALID = 601;
  const ERR_ACCESS_TOKEN_EXPIRED = 602;
  const ERR_ACCESS_DENIED = 603;
  const ERR_REQ_TIMEOUT = 604;
  const ERR_HTTP_METHOD_UNSUPPORTED = 605;
  const ERR_MAX_RATE_LIMIT_EXCEEDED = 606;
  const ERR_DAILY_QUOTA_REACHED = 607;
  const ERR_API_TEMP_UNAVAILABLE = 608;
  const ERR_JSON_INVALID = 609;
  const ERR_REQ_RESOURCE_NOT_FOUND = 610;
  const ERR_SYSTEM_ERROR = 611;
  const ERR_CONTENT_TYPE_INVALID = 612;
  const ERR_MULTIPART_REQ_INVALID = 613;
  const ERR_CONCURRENT_ACCESS_LIMIT = 615;
  const ERR_FIELD_EMPTY = 701;
  const ERR_RECORDS_NOT_FOUND = 702;
  const ERR_FEATURE_NOT_ENABLED = 703;
  const ERR_DATE_FORMAT_INVALID = 704;
  const ERR_BUSINESS_RULE_VIOLATION = 709;
  const ERR_PARENT_FOLDER_NOT_FOUND = 710;
  const ERR_FOLDER_TYPE_INCOMPATIBLE = 711;
  const ERR_PARAM_TYPE_MISMATCH = 1001;
  const ERR_PARAM_MISSING_VALUE = 1002;
  const ERR_DATA_INVALID = 1003;
  const ERR_LEAD_NOT_FOUND = 1004;
  const ERR_LEAD_ALREADY_EXISTS = 1005;
  const ERR_FIELD_NOT_FOUND = 1006;
  const ERR_MULTIPLE_LEAD_MATCH = 1007;
  const ERR_PARTITION_ACCESS_DENIED = 1008;
  const ERR_PARTITION_NAME_UNSPECIFIED = 1009;
  const ERR_PARTITION_UPDATE_NOT_ALLOWED = 1010;
  const ERR_FIELD_NOT_SUPPORTED = 1011;
  const ERR_COOKIE_VAL_INVALID = 1012;
  const ERR_OBJECT_NOT_FOUND = 1013;
  const ERR_OBJECT_CREATE_FAILED = 1014;
  const ERR_LEAD_NOT_IN_LIST = 1015;
  const ERR_TOO_MANY_IMPORTS = 1016;
  const ERR_OBJECT_ALREADY_EXISTS = 1017;
  const ERR_CRM_ENABLED = 1018;
  const ERR_IMPORT_IN_PROGRESS = 1019;
  const ERR_TOO_MANY_CLONE_OP = 1020;
  const ERR_COMPANY_UPDATE_NOT_ALLOWED = 1021;
  const ERR_OBJECT_IN_USE = 1022;

}