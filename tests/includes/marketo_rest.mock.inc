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
    if (!$this->getStoredAccessToken() || $this->isAccessTokenExpired()) {
      $this->getNewAccessToken($options);
    }

    return $this->getStoredAccessToken();
  }

  /**
   * Mock a new access token.
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
      $params += array('access_token' => $this->getStoredAccessToken());
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
    return $this->restCall($url, $options, $api);
  }

  /**
   * Make a mock REST request to the server.
   *
   * @param $url
   * @param $options
   * @param $api
   *   Add the api in for testing.
   * @return object
   */
  private function restCall($url, $options, $api = null) {
    // Record the latest request.
    $this->setLastRequest(drupal_json_encode(
      array(
        'url' => $url,
        'options' => $options
      )
    ));

    // For identity request get a mock token.
    if ($api == MARKETO_REST_IDENTITY_API) {
      $response = $this->getMockToken();
    }
    else {
      // Get mock data from rest endpoint for all other requests.
      $response = $this->getMockData($url, $options, $api);
    }
    // Record the latest response.
    $this->setLastResponse(drupal_json_encode($response));
    return $response;
  }

  /**
   * Return a Mock token.
   *
   * @return object
   */
  private function getMockToken() {
    // Return a response object including 'data' property.
    return (object) [
      'data' => '{
        "access_token":"88888888-4444-4444-4444-121212121212:ab",
        "token_type":"bearer",
        "expires_in":3599,
        "scope":"john@marketo_rest.com"
        }',
    ];
  }

  /**
   * Get mock data.
   *
   * @param $url
   * @param $options
   * @param $api
   * @return object
   */
  private function getMockData($url, $options, $api) {
    // Set up the response object.
    $response = (object) [
      'request' => $options['data'],
      'code' => 200,
      'protocol' => 'HTTP/1.1',
      'status_message' => '',
      'redirect_code' => '',
      'redirect_url' => '',
      //'error' => '', // Only set if error received.
      'headers' => '',
    ];
    switch($api) {
      case MARKETO_REST_LEADS_API :
        $response->data = '';
        break;
      case MARKETO_REST_ACTIVITIES_API :
        $response->data = '';
        break;
      default:
        $response->data = '';
        break;
    }

    return $response;
  }

}
