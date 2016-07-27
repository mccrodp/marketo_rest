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
   * Retrieves all fields and the data type of a lead object.
   */
  public function getFields();

  /**
   * Retrieves lead information.
   *
   * @param string $type
   *   Filter type: id, cookies, email, twitter / facebookId, custom field, etc.
   * @param string $values
   *   Comma separated list of filter values
   * @param $fields
   *   Comma separated list of field names to be returned in the response.
   * @param string $batch
   *   The number of lead records to be returned in a single call.
   * @param string $token
   *   Paging token used on large result sets. [Currently unused]
   * @param string $method
   *   Required when using the HTTP POST method
   * @return mixed
   */
  public function getLead($values, $type = 'email', $fields = NULL, $batch = '300', $token = NULL, $method = 'GET');

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
   * @param string $batch_size
   *   Default and max = '300'
   * @return mixed
   */
  public function getLeadActivity($ids, $token, $lead_ids = NULL, $list_id = NULL, $batch_size = '300');

  /**
   * Create new leads, or if the lead already exists update it.
   *
   * @param array $input
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
  public function syncLead(array $input, $action = 'createOrUpdate', $field = NULL, $async = FALSE, $partition = NULL);
}

/**
 * Marketo REST Client
 */
class MarketoRestClient implements MarketoRestInterface {

  // Identity and endpoints.
  protected $client_id;
  protected $client_secret;
  protected $endpoint;
  protected $identity;

  // Temporary access token.
  private $access_token;
  private $access_token_expiry;

  // Temporary paging token.
  protected $paging_token;

  // Activity types.
  private $activity_types;

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
    $this->client_id = $client_id;
    $this->client_secret = $client_secret;
    $this->endpoint = $endpoint;
    $this->identity = $identity;

    return $this;
  }

  /**
   * Get new REST access token.
   *
   * @param array|null $token
   *   Full access token JSON.
   * @return bool
   */
  public function getAccessToken(array $token = NULL) {

    // Load the access token from the (JSON) full token.
    if ($token) {
      $this->loadStoredAccessToken($token);
    }

    // Check if we have an access token or if token is valid.
    if (!$this->access_token || $this->isAccessTokenExpired()) {
      $this->getNewAccessToken();
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
  public function getLead($values, $type = 'email', $fields = NULL, $batch = '300', $token = NULL, $method = 'GET') {

    $operation = 'GET';
    // Set the operation to the method param.
    if ($method != $operation) {
      $operation = $method;
    }

    $body = (object) [
      'filterType' => $type,
      'filterValues' => is_array($values) ? implode(',', $values) : $values,
      'batchSize' => $batch,
      '_method' => $method
    ];

    if (!empty($fields)) {
      $body->fields = $fields;
    }

    if (!empty($token)) {
      $body->nextPageToken = $token; // @todo: add assoc functionality.
    }

    try {
      $result = $this->request(MARKETO_REST_LEADS_API, $body, $operation);
      $leads = $this->prepareResults($result);
    } catch (Exception $e) {
      throw new Exception($e->getMessage());
    }

    return $leads['result'];
  }

  /**
   * Get all lead activities.
   *
   * @param $values
   * @param string $type
   * @param int $batch_size
   * @return array
   */
  public function getAllLeadActivities($values, $type = 'email', $batch_size = 10) {
    $activity_ids = $this->getActivityDataByField($this->getActivityTypes());

    // Check we have more than 1 batch.
    if (count($activity_ids) > $batch_size) {
      // Separate into batches of $batch_size.
      $batches = array_chunk($activity_ids, $batch_size);
    }
    else {
      // We have only 1 batch.
      $batches = array($activity_ids);
    }

    $lead_activity = array();
    $lead_ids = implode(',', $this->getLeadIds($values, $type));

    // Cycle through each activity batch and merge lead activities.
    foreach ($batches as $batch) {
      $batch_activity = $this->getLeadActivity(
        implode(',', $batch),
        $this->getPagingToken(),
        $lead_ids
      );
      if (!empty($batch_activity) && is_array($batch_activity)) {
        $lead_activity = array_merge($lead_activity, $batch_activity);
      }
    }

    return $lead_activity;
  }

  /**
   * {@inheritdoc}
   */
  public function getLeadActivity($ids, $token, $lead_ids = NULL, $list_id = NULL, $batch_size = 300) {

    $body = (object) [
      'activityTypeIds' => $ids,
      'nextPageToken' => $token,
      'batchSize' => $batch_size,
    ];

    if (!empty($list_id)) {
      $body->listId = $list_id;
    }

    if (!empty($lead_ids)) {
      $body->leadIds = $lead_ids;
    }

    try {
      $result = $this->request(MARKETO_REST_ACTIVITIES_API, $body);
      $activities = $this->prepareResults($result);
      // Set paging token from the response.
      // @todo: should this be set only if $activities['moreResult'] == true ?
      $this->setPagingToken($activities['nextPageToken']);
      // If we have no items return false.
      if (empty($activities['result'])) {
        return FALSE;
      }
    } catch (Exception $e) {
      throw new Exception($e->getMessage());
    }

    return $activities['result'];
  }

  /**
   * {@inheritdoc}
   */
  public function getFields() {
    try {
      $result = $this->request(MARKETO_REST_LEADS_DESCRIBE_API);
      $fields = $this->prepareResults($result);
    } catch (Exception $e) {
      $fields = array();
    }

    return $fields;
  }

  /**
   * {@inheritdoc}
   */
  public function syncLead(array $input, $action = 'createOrUpdate', $field = NULL, $async = 'false', $partition = NULL) {
    // Generate an array of leads.
    $params = (object) [
      'input' => $input,
      'action' => $action,
      'asyncProcessing' => $async
    ];

    if ($field) {
      $params->lookupField = $field;
    }

    if ($partition) {
      $params->partitionName = $partition;
    }

    try {
      $response = $this->request(MARKETO_REST_LEADS_API, $params, 'POST');
      $result = $this->prepareResults($response);
      $ret['success'] = $result['success'];
    } catch (Exception $e) {
      $ret['result'] = $e->getMessage();
      $ret['success'] = FALSE;
    }
    return $ret;
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
   * @param stdClass $request
   * @return mixed
   */
  public function setLastRequest(stdClass $request) {
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
   * Get the options to send in access token request.
   *
   * @return stdClass
   */
  public function getIdentityTokenOptions() {
    // Query Marketo for token.
    return (object) [
      'client_id' => $this->client_id,
      'client_secret' => $this->client_secret,
      'grant_type' => 'client_credentials',
    ];
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
   * Generate a build query.
   *
   * @param $params
   * @return string
   */
  public function buildQuery($params) {
    return drupal_http_build_query((array) $params);
  }

  /**
   * Get identity endpoint as per Marketo docs.
   *
   * @return string
   */
  public function getIdentityEndpoint() {
    return $this->identity;
  }

  /**
   * Access token setter.
   *
   * @param $access_token
   */
  public function setAccessToken($access_token) {
    $this->access_token = $access_token;
  }

  /**
   * Access token expiry setter.
   *
   * @param $calculateTokenExpiry
   */
  public function setAccessTokenExpiry($calculateTokenExpiry) {
    $this->access_token_expiry = $calculateTokenExpiry;
  }

  /**
   * Get paging token from memory or request new token.
   *
   * @return mixed
   */
  public function getPagingToken() {
    if (!$this->paging_token) {
      $this->paging_token = $this->getNewPagingToken();
    }

    return $this->paging_token;
  }

  /**
   * Set paging token.
   *
   * Usually set from response.
   *
   * @param $token
   */
  public function setPagingToken($token) {
    $this->paging_token = $token;
  }

  /**
   * Get meta data about activity types.
   *
   * @return mixed
   * @throws \Exception
   */
  public function getActivityTypes() {
    try {
      if (empty($this->activity_types)) {
        $result = $this->request(MARKETO_REST_ACTIVITIES_TYPES_API);
        $types = $this->prepareResults($result);
        $this->activity_types = $types['result'];
      }
    } catch (Exception $e) {
      throw new Exception($e->getMessage());
    }

    return $this->activity_types;
  }

  /**
   * Get activity fata by field name.
   *
   * @param array $types
   * @param string $field_name
   * @return array
   */
  protected function getActivityDataByField(array $types, $field_name = 'id') {
    $activity_data = array();
    foreach ($types as $type) {
      $activity_data[$type['id']] = $type[$field_name];
    }
    return $activity_data;
  }

  /**
   * Get the stored activity types.
   *
   * @return mixed
   */
  protected function getStoredActivityTypes() {
    return $this->activity_types;
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
   * Extract the token data from the response array and load into properties.
   *
   * @param $data
   */
  protected function extractTokenDataFromResponse($data) {
    // Get the JSON access token from response string.
    $token = drupal_json_decode($data);
    if (!empty($token['access_token'])) {
      // Set new access token.
      $this->setAccessToken($token['access_token']);
      $this->setAccessTokenExpiry($this->calculateTokenExpiry($token['expires_in']));
    }
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
   * Set the activity types.
   *
   * @param $types
   * @return bool
   */
  protected function setActivityTypes($types) {
    if (!empty($types)) {
      $this->activity_types = $types;
      return TRUE;
    }
    return FALSE;
  }

  /**
   * Get lead ids.
   *
   * @param $values
   * @param $type
   * @return array|mixed
   */
  protected function getLeadIds($values, $type) {
    $lead_ids = array();
    // Cycle through the leads and create an array of lead ids.
    foreach ($this->getLead($values, $type) as $lead) {
      $lead_ids[$lead['id']] = $lead['id'];
    }
    return $lead_ids;
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
   * Request a new access token.
   */
  private function getNewAccessToken() {
    // Send a GET request for the access token.
    $response = $this->request(
      MARKETO_REST_IDENTITY_API,
      $this->getIdentityTokenOptions(),
      'GET',
      $this->getIdentityEndpoint()
    );

    // Extract response data into properties and return raw data json string.
    $this->extractTokenDataFromResponse($response);
  }

  /**
   * Constructs the request to create the REST call to Marketo.
   *
   * @param $api
   *   The API endpoint
   * @param stdClass $body
   *   Parameters to be sent with the request
   * @param string $operation
   *   The operation to execute
   * @param null|string $url
   * @return object
   *   Response object
   */
  private function request($api, stdClass $body = NULL, $operation = 'GET', $url = NULL) {

    // Set our REST endpoint if empty.
    if (!$url) {
      $url = $this->getFullRestEndpoint();
    }

    // Add the api endpoint to the url.
    $url = $url . '/' . $api;

    // Depending on operation we construct the REST call differently.
    switch ($operation) {
      case 'POST':
        // Add the access token
        $url = $url . '?access_token=' . $this->access_token;
        $body = drupal_json_encode($body);
        break;
      case 'GET':
      default:
        // Add the access token unless we are querying for access token.
        if ($api != MARKETO_REST_IDENTITY_API && $url != $this->identity) {
          if (empty($body)) {
            $body = new stdClass();
          }
          $body->access_token = $this->access_token;
        }
        // GET requires query string rather than params in the body.
        $url = $url . '?' . $this->buildQuery($body);
        $body = NULL;
        break;
    }
    return $this->curlCall($url, $body, $operation);
  }

  /**
   * Make a cURL request.
   *
   * @param $url
   * @param $body
   * @param $operation
   * @return mixed
   */
  private function curlCall($url, $body = NULL, $operation = 'GET') {
    $ch = curl_init($url);
    curl_setopt($ch, CURLOPT_RETURNTRANSFER, 1);
    if ($operation == 'POST') {
      curl_setopt($ch, CURLOPT_HTTPHEADER, array(
        'accept: application/json',
        'Content-Type: application/json'
      ));
      curl_setopt($ch, CURLOPT_POST, 1);
      if ($body) {
        curl_setopt($ch, CURLOPT_POSTFIELDS, $body);
      }
    }
    curl_getinfo($ch);
    // Record the latest request.
    $this->setLastRequest(
      (object) [
        'url' => $url,
        'body' => $body,
        'operation' => $operation,
      ]
    );

    $response = curl_exec($ch);
    // Record the latest response.
    $this->setLastResponse($response);

    return $response;
  }

  /**
   * Get a new paging token.
   *
   * @return mixed
   * @throws \Exception
   */
  private function getNewPagingToken() {
    $body = (object) ['sinceDatetime' => date(MARKETO_REST_DATE_FORMAT)];

    try {
      $result = $this->request(MARKETO_REST_PAGING_TOKEN_API, $body);
      $token = $this->prepareResults($result);
      $this->setPagingToken($token['nextPageToken']);
    } catch (Exception $e) {
      throw new Exception($e->getMessage());
    }

    return $token['nextPageToken'];
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