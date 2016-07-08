<?php

/**
 * @file
 * Class and methods for interacting with the Marketo REST API.
 */

/**
 * Marketo Interface
 */
interface MarketoInterface {

  /**
   * Retrieves list of defined fields.
   */
  public function getFields();

  /**
   * Retrieves lead information.
   * 
   * @param string $key
   *   Lead Key, typically email address
   * @param string $type
   *   Lead key type, auto-detection attempted if not supplied
   */
  public function getLead($key, $type);

  /**
   * Retrieves lead activity information.
   * 
   * @param string $key
   *   Lead Key, typically email address
   * @param string $type
   *   Lead key type, auto-detection attempted if not supplied
   */
  public function getLeadActivity($key, $type);

  /**
   * Inserts or updates a lead.
   * 
   * @param array $lead
   *   Key value pairs to be associated with a lead
   * @param string $key
   *   Lead Key, typically email address
   * @param object $cookie
   *   Marketo cookie information
   * @param array $options
   *   Array of additional options to configure lead syncing
   */
  public function syncLead($lead, $key, $cookie, $options);
}

/**
 * Marketo Client
 */
class MarketoClient implements MarketoInterface {

  protected $clientId;
  protected $clientSecret;
  protected $endpoint;

  // Temporary access token.
  private $access_token;

  /**
   * Constructor for MarketoClient class.
   * 
   * @param string $client_id
   *   A Client ID defined in Marketo admin
   * @param string $client_secret
   *   A secret-key defined in Marketo admin
   * @param string $endpoint
   *   REST endpoint URL
   * @param array $options
   *   Optional array of SoapClient options 
   */
  public function __construct($client_id, $client_secret, $endpoint, $options = array()) {
    $this->clientId = $client_id;
    $this->clientSecret = $client_secret;
    $this->endpoint = $endpoint;

    $this->access_token = $this->getAccessToken($options);
  }

  /**
   * Builds Marketo authentication header.
   * 
   * @return \SoapHeader
   *   Marketo authentication header
   */
  protected function authenticationHeader() {
    $timestamp = date("c");
    $encrypt_string = $timestamp . $this->clientId;
    $signature = hash_hmac('sha1', $encrypt_string, $this->clientSecret);

    $data = new stdClass();
    $data->mktowsUserId = $this->clientId;
    $data->requestSignature = $signature;
    $data->requestTimestamp = $timestamp;

    $header = new SoapHeader('http://www.marketo.com/mktows/', 'AuthenticationHeader', $data);

    return $header;
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
   * Sends request to Marketo.
   * 
   * @param string $operation
   *   The operation to execute
   * @param array $params
   *   Parameters to be sent with the request
   * 
   * @return object
   *   Response object
   */
  protected function request($operation, $params) {
    return $this->client->__soapCall(
        $operation, array($params), array(), $this->authenticationHeader()
    );
  }

  /**
   * Get new REST access token.
   *
   * @param array $options
   * @return bool
   */
  protected function getAccessToken(array $options) {
    // Query Marketo for token.
    $default_options = array(
      'client_id' => $this->clientId,
      'client_secret' => $this->clientSecret,
      'grant_type' => 'client_credentials',
    );

    $rest_options = array_merge($default_options, $options);
    $full_url = url($this->endpoint, array('query' => $rest_options));
    $response = drupal_http_request($full_url);

    if ($response->data) {
      $res = json_decode($response->data);
      if (!empty($res->access_token)) {
        return $res->access_token;
      }
    }

    return FALSE;
  }

  /**
   * Returns the XML sent in the last SOAP request.
   * 
   * @return string
   */
  public function __getLastRequest() {
    return $this->client->__getLastRequest();
  }

  /**
   * Returns the XML received in the last REST response.
   *
   * @return string
   */
  public function __getLastResponse() {
    return $this->client->__getLastResponse();
  }

  /**
   * {@inheritdoc}
   */
  public function getLead($key, $type = NULL) {
    $lead = new stdClass();
    $lead->leadKey = new stdClass();
    $lead->leadKey->keyType = (is_null($type)) ? $this->keyType($key) : strtoupper($type);
    $lead->leadKey->keyValue = $key;

    try {
      $result = $this->request('getLead', $lead);
      $leads = $this->prepareLeadResults($result);
    }
    catch (Exception $e) {

      if (isset($e->detail->serviceException->code) && $e->detail->serviceException->code == MarketoError::ERR_LEAD_NOT_FOUND) {
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
  public function getLeadActivity($key, $type = NULL) {
    $lead = new stdClass();
    $lead->leadKey = new stdClass();
    $lead->leadKey->keyType = (is_null($type)) ? $this->keyType($key) : strtoupper($type);
    $lead->leadKey->keyValue = $key;
    $lead->activityFilter = new stdClass();
    $lead->startPosition = new stdClass();
    $lead->batchSize = 100;

    try {
      $result = $this->request('getLeadActivity', $lead);
      $activity = $this->prepareLeadActivityResults($result);
    }
    catch (Exception $e) {

      if (isset($e->detail->serviceException->code) && $e->detail->serviceException->code == MarketoError::ERR_LEAD_NOT_FOUND) {
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
    $params = array(
      'objectName' => 'LeadRecord',
    );
    try {
      $result = $this->request('describeMObject', $params);
      $fields = $this->prepareFieldResults($result);
    }
    catch (Exception $e) {
      $fields = array();
    }

    return $fields;
  }

  /**
   * {@inheritdoc}
   */
  public function syncLead($lead, $key = NULL, $cookie = NULL, $options = array()) {
    // Merge default options.
    $options += array(
      'returnLead' => FALSE,
      'extendedResponse' => FALSE,
    );

    $params = new stdClass();
    $params->marketoCookie = $cookie;
    $params->returnLead = $options['returnLead'];
    $params->leadRecord = $this->leadRecord($lead, $key);
    $ret = array(
      'success' => FALSE,
      'result' => NULL,
    );

    try {
      $ret['result'] = $this->request('syncLead', $params);
      $ret['success'] = TRUE;
    }
    catch (Exception $e) {
      $ret['result'] = $e->getMessage();
      $ret['success'] = FALSE;
    }
    return ($options['extendedResponse']) ? $ret : $ret['result'];
  }

  /**
   * Converts response into a more useful structure.
   * 
   * @param object $data
   *   LeadRecord object definition
   * 
   * @return array
   *   Key value pairs of fields
   */
  protected function prepareFieldResults($data) {
    $fields = array();

    foreach ($data->result->metadata->fieldList->field as $field) {
      $fields[$field->name] = (array) $field;
    }

    return $fields;
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

}

class MarketoError {

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