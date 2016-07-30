<?php

/**
 * @file
 * Class and methods to mock interacting with the Marketo REST API.
 */

/**
 * Marketo Mock Client
 */
class MarketoMockClient extends MarketoRestClient implements MarketoRestInterface {

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
   * @return MarketoMockClient
   */
  public function __construct($client_id, $client_secret, $endpoint, $identity) {
    // When any of our required params are empty, throw exception.
    if (empty($client_id) || empty($client_secret) || empty($endpoint) || empty($identity)) {
      throw new Exception(t('One of the fields were empty: Client Id, Client Secret, Endpoint, Identity'));
    }

    parent::__construct($client_id, $client_secret, $endpoint, $identity);

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
    if (!$this->getStoredAccessToken() || $this->isAccessTokenExpired()) {
      $this->getNewAccessToken();
    }

    return $this->getStoredAccessToken();
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
  public function syncLead(array $input, $action = 'createOrUpdate', $field = null, $async = 'false', $partition = null) {
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
    }
    catch (Exception $e) {
      $ret['result'] = $e->getMessage();
      $ret['success'] = FALSE;
    }
    return $ret;
  }

  /**
   * {@inheritdoc}
   */
  public function getLead($values, $type = 'email', $fields = null, $batch = '300', $token = null, $method = 'GET') {

    $operation = 'GET';
    // Set the operation to the method param.
    if ($method != $operation) {
      $operation = $method;
    }

    $params = (object) [
      'filterType' => $type,
      'filterValues' => is_array($values) ? implode(',', $values) : $values,
      'batchSize' => $batch,
      '_method' => $method
    ];

    if (!empty($fields)) {
      $params['fields'] = $fields;
    }

    if (!empty($token)) {
      $params['nextPageToken'] = $token; // @todo: add assoc functionality.
    }

    try {
      $result = $this->request(MARKETO_REST_LEADS_API, $params, $operation);
      $leads = $this->prepareResults($result);
    }
    catch (Exception $e) {
      throw new Exception($e->getMessage());
    }

    return $leads['result'];
  }

  /**
   * Mock a new access token.
   *
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
   * Get meta data about activity types.
   *
   * @param bool $stored
   * @return mixed
   * @throws \Exception
   */
  public function getActivityTypes($stored = TRUE) {
    try {
      if (!$stored || empty($this->getStoredActivityTypes())) {
        $result = $this->request(MARKETO_REST_ACTIVITIES_TYPES_API);
        $types = $this->prepareResults($result);
        $this->setActivityTypes($types['result']);
      }
    }
    catch (Exception $e) {
      throw new Exception($e->getMessage());
    }

    return $this->getStoredActivityTypes();
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
  private function request($api, stdClass $body = null, $operation = 'GET', $url = NULL) {

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
        $url = $url . '?access_token=' . $this->getStoredAccessToken();
        break;
      case 'GET':
      default:
        // Add the access token unless we are querying for access token.
        if ($api != MARKETO_REST_IDENTITY_API && $url != $this->identity) {
          if (empty($body)) {
            $body = new stdClass();
          }
          $body->access_token = $this->getStoredAccessToken();
        }

        // GET requires query string rather than params in the body.
        $url = $url . '?' . $this->buildQuery($body);
        $body = null;
        break;
    }
    return $this->curlCall($url, $body, $operation, $api);
  }

  /**
   * Make a mock cURL request to the server.
   *
   * @param $url
   * @param $body
   * @param $operation
   * @param $api
   *   Add the api in for testing.
   * @return object
   */
  private function curlCall($url, $body = null, $operation = 'GET', $api = NULL) {
    // Record the latest request.
    $this->setLastRequest(
      (object) [
        'url' => $url,
        'body' => $body,
        'operation' => $operation,
      ]
    );
    // For identity request get a mock token.
    if ($api == MARKETO_REST_IDENTITY_API) {
      // @todo: add validation on request.
      $response = $this->getMockToken();
    }
    else {
      // @todo: add validation on request.
      // Get mock data from rest endpoint for all other requests.
      $response = $this->getMockData($url, $body, $api);
    }
    // Record the latest response.
    $this->setLastResponse($response);
    return $response;
  }

  /**
   * Return a Mock token.
   *
   * @return string
   */
  private function getMockToken() {
    // Return a response object including 'data' property.
    return '{
        "access_token":"88888888-4444-4444-4444-121212121212:ab",
        "token_type":"bearer",
        "expires_in":3599,
        "scope":"john@marketo-rest.com"
        }';
  }

  /**
   * Get mock data.
   *
   * @param $url
   * @param $body
   * @param $api
   * @return string
   */
  private function getMockData($url, $body, $api) {
    // Return data depending on api endpoint.
    switch ($api) {
      case MARKETO_REST_LEADS_API :
        // Return lead data for GET request with empty body.
        if (empty($body)) {
          $response = '{"requestId":"e463#1562256e577","result":[{"id":3368789,"updatedAt":"2016-07-25T09:02:35Z","lastName":null,"email":"test@marketo-rest.com","createdAt":"2016-07-24T12:52:03Z","firstName":"Jason"}],"success":true}';
        }
        else {
          // If we are sending a post request there will be a body.
          $response = '{"requestId":"4cb2#1561d3f0bf7","result":[{"id":3368789,"status":"created"}],"success":true}';
        }
        break;
      case MARKETO_REST_ACTIVITIES_API :
        $response = '{"requestId":"def3#1562cdef43e","success":true,"nextPageToken":"3PVDQWQ2XTJEGCWQB3THD6XYGS23BHHYNML2L43G564525NX5OVQ====","moreResult":false}';
        break;
      case MARKETO_REST_LEADS_DESCRIBE_API :
        $response = '{
          "result": [{
            "id": 2,
            "displayName": "Company Name",
            "dataType": "string",
            "length": 255,
            "rest": {
              "name": "company",
              "readOnly": false
            },
            "soap": {
              "name": "company",
              "readOnly": false
            }
          }, {
            "id": 3,
            "displayName": "Site",
            "dataType": "string",
            "length": 255,
            "rest": {
              "name": "site",
              "readOnly": false
            },
            "soap": {
              "name": "site",
              "readOnly": false
            }
          }, {
            "id": 4,
            "displayName": "Billing Address",
            "dataType": "text",
            "rest": {
              "name": "billingStreet",
              "readOnly": false
            },
            "soap": {
              "name": "billingStreet",
              "readOnly": false
            }
          }],
          "success": true
        }';
        break;
      case MARKETO_REST_ACTIVITIES_TYPES_API :
        $response = '{ "requestId": "eef6#1562be4d44c", "result": [{ "id": 1, "name": "Visit Webpage", "description": "User visits a web page", "primaryAttribute": { "name": "Webpage ID", "dataType": "integer" }, "attributes": [{ "name": "Client IP Address", "dataType": "string" }, { "name": "Query Parameters", "dataType": "string" }, { "name": "Referrer URL", "dataType": "string" }, { "name": "Search Engine", "dataType": "string" }, { "name": "Search Query", "dataType": "string" }, { "name": "User Agent", "dataType": "string" }, { "name": "Webpage URL", "dataType": "string" }] }, { "id": 2, "name": "Fill Out Form", "description": "User fills out and submits a form on web page", "primaryAttribute": { "name": "Webform ID", "dataType": "integer" }, "attributes": [{ "name": "Client IP Address", "dataType": "string" }, { "name": "Form Fields", "dataType": "text" }, { "name": "Query Parameters", "dataType": "string" }, { "name": "Referrer URL", "dataType": "string" }, { "name": "User Agent", "dataType": "string" }, { "name": "Webpage ID", "dataType": "integer" }] }], "success": true }';
        break;
      case MARKETO_REST_PAGING_TOKEN_API :
        $response = '{"requestId":"5123#15627c1c3fd","success":true,"nextPageToken":"K5T3IN4MRMAIU74RKJPXW3VICTV22SGRR7EPHF4QQLFTVSQS3CTQ===="}';
        break;
      default:
        $response = 'MOCK_DATA_DEFAULT';
        break;
    }

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
    }
    catch (Exception $e) {
      throw new Exception($e->getMessage());
    }

    return $token['nextPageToken'];
  }

}
