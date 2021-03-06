<?php

/**
 * @file
 * Administrative page callbacks for the Marketo REST module.
 */

/**
 * Builds and returns the Marketo settings form.
 */
function marketo_rest_admin_settings_form($form, &$form_state) {
  $form['marketo_rest_basic'] = array(
    '#title' => t('Basic Settings'),
    '#type' => 'fieldset',
  );

  $form['marketo_rest_basic']['marketo_rest_munchkin_account_id'] = array(
    '#type' => 'textfield',
    '#title' => t('Account ID'),
    '#default_value' => variable_get('marketo_rest_munchkin_account_id', ''),
    '#required' => TRUE,
  );
  $form['marketo_rest_basic']['marketo_rest_munchkin_javascript_library'] = array(
    '#type' => 'textfield',
    '#title' => t('Munchkin Javascript Library'),
    '#default_value' => variable_get('marketo_rest_munchkin_javascript_library', '//munchkin.marketo.net/munchkin.js'),
    '#required' => TRUE,
    '#description' => t('Typically this does not need to be changed and should use the default value //munchkin.marketo.net/munchkin.js'),
  );
  $form['marketo_rest_basic']['marketo_rest_instance_host'] = array(
    '#type' => 'textfield',
    '#title' => t('Marketo Instance Host'),
    '#default_value' => variable_get('marketo_rest_instance_host'),
    '#required' => FALSE,
    '#description' => t('Host for your Marketo instance. Example: app-sjqe.marketo.com'),
  );
  $form['marketo_rest_basic']['marketo_rest_munchkin_lead_source'] = array(
    '#type' => 'textfield',
    '#title' => t('Default Lead Source'),
    '#default_value' => variable_get('marketo_rest_munchkin_lead_source'),
    '#description' => t('If set, leadSource will be set to this value unless specifically overridden during data collection.'),
  );
  $form['marketo_rest_basic']['marketo_rest_logging'] = array(
    '#type' => 'checkbox',
    '#title' => t('Verbose Logging'),
    '#default_value' => variable_get('marketo_rest_logging', FALSE),
    '#description' => t('If checked, additional data will be added to watchdog.'),
  );

  $form['marketo_rest_tabs'] = array(
    '#type' => 'vertical_tabs',
  );

  $form['marketo_rest_tabs']['marketo_rest_api'] = array(
    '#title' => t('API Configuration'),
    '#type' => 'fieldset',
  );
  $form['marketo_rest_tabs']['marketo_rest_api']['marketo_rest_tracking_method'] = array(
    '#title' => t('Tracking Method'),
    '#type' => 'radios',
    '#options' => array(
      'munchkin' => t('Munchkin Javascript API'),
      'rest' => t('REST API'),
    ),
    '#default_value' => variable_get('marketo_rest_tracking_method', MARKETO_REST_TRACKING_METHOD_DEFAULT),
    '#required' => TRUE,
    '#descritpion' => t('Asynchronous will queue requests and send data when cron runs.'),
  );
  $form['marketo_rest_tabs']['marketo_rest_api']['marketo_rest_munchkin'] = array(
    '#title' => t('Munchkin Javascript API'),
    '#type' => 'fieldset',
  );
  $form['marketo_rest_tabs']['marketo_rest_api']['marketo_rest_munchkin']['marketo_rest_munchkin_api_private_key'] = array(
    '#type' => 'textfield',
    '#title' => t('API Private Key'),
    '#default_value' => variable_get('marketo_rest_munchkin_api_private_key', ''),
    '#required' => TRUE,
    '#description' => t('Value can be found on the Munchkin Admin page at Admin > Integration > Munchkin'),
  );
  $form['marketo_rest_tabs']['marketo_rest_api']['marketo_rest_munchkin']['marketo_rest_munchkin_partition'] = array(
    '#type' => 'textfield',
    '#title' => t('Workspace (Partition)'),
    '#default_value' => variable_get('marketo_rest_munchkin_partition', ''),
    '#required' => FALSE,
    '#description' => t('Value can be found on the Munchkin Admin page at Admin > Integration > Munchkin'),
  );

  $form['marketo_rest_tabs']['marketo_rest_api']['marketo_rest_rest'] = array(
    '#title' => t('REST API'),
    '#type' => 'fieldset',
    '#description' => t('Values can be found on the REST API Admin page at Admin > Integration > REST API'),
  );
  $form['marketo_rest_tabs']['marketo_rest_api']['marketo_rest_rest']['marketo_rest_endpoint'] = array(
    '#type' => 'textfield',
    '#title' => t('Endpoint'),
    '#default_value' => variable_get('marketo_rest_endpoint', ''),
    '#required' => FALSE,
  );
  $form['marketo_rest_tabs']['marketo_rest_api']['marketo_rest_rest']['marketo_rest_identity'] = array(
    '#type' => 'textfield',
    '#title' => t('Identity'),
    '#default_value' => variable_get('marketo_rest_identity', ''),
    '#required' => FALSE,
  );
  $form['marketo_rest_tabs']['marketo_rest_api']['marketo_rest_rest']['marketo_rest_client_id'] = array(
    '#type' => 'textfield',
    '#title' => t('Client ID'),
    '#default_value' => variable_get('marketo_rest_client_id', ''),
    '#required' => FALSE,
  );
  $form['marketo_rest_tabs']['marketo_rest_api']['marketo_rest_rest']['marketo_rest_client_secret'] = array(
    '#type' => 'textfield',
    '#title' => t('Client Secret'),
    '#default_value' => variable_get('marketo_rest_client_secret', ''),
    '#required' => FALSE,
  );
  $form['marketo_rest_tabs']['marketo_rest_api']['marketo_rest_rest']['marketo_rest_rest_proxy'] = array(
    '#title' => t('RestClient Proxy Settings'),
    '#type' => 'fieldset',
    '#description' => t('For making an HTTP connection through a proxy server, the options proxy_host, proxy_port, proxy_login and proxy_password are also available.'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
  );
  $form['marketo_rest_tabs']['marketo_rest_api']['marketo_rest_rest']['marketo_rest_rest_proxy']['marketo_rest_rest_proxy_host'] = array(
    '#type' => 'textfield',
    '#title' => t('Proxy Host'),
    '#default_value' => variable_get('marketo_rest_rest_proxy_host', ''),
    '#required' => FALSE,
  );
  $form['marketo_rest_tabs']['marketo_rest_api']['marketo_rest_rest']['marketo_rest_rest_proxy']['marketo_rest_rest_proxy_port'] = array(
    '#type' => 'textfield',
    '#title' => t('Proxy Port'),
    '#default_value' => variable_get('marketo_rest_rest_proxy_port', ''),
    '#required' => FALSE,
  );
  $form['marketo_rest_tabs']['marketo_rest_api']['marketo_rest_rest']['marketo_rest_rest_proxy']['marketo_rest_rest_proxy_login'] = array(
    '#type' => 'textfield',
    '#title' => t('Proxy Login'),
    '#default_value' => variable_get('marketo_rest_rest_proxy_login', ''),
    '#required' => FALSE,
  );
  $form['marketo_rest_tabs']['marketo_rest_api']['marketo_rest_rest']['marketo_rest_rest_proxy']['marketo_rest_rest_proxy_password'] = array(
    '#type' => 'textfield',
    '#title' => t('Proxy Password'),
    '#default_value' => variable_get('marketo_rest_rest_proxy_password', ''),
    '#required' => FALSE,
  );

  $form['marketo_rest_tabs']['marketo_rest_page_vis'] = array(
    '#title' => t('Page Visibility'),
    '#type' => 'fieldset',
  );
  $form['marketo_rest_tabs']['marketo_rest_page_vis']['marketo_rest_visibility_pages'] = array(
    '#title' => t('Add tracking to specific pages'),
    '#type' => 'radios',
    '#options' => array(
      t('All pages except those listed below'),
      t('Only the pages listed below'),
    ),
    '#default_value' => variable_get('marketo_rest_visibility_pages', 0),
    '#required' => TRUE,
  );
  $form['marketo_rest_tabs']['marketo_rest_page_vis']['marketo_rest_pages'] = array(
    '#type' => 'textarea',
    '#title' => t('Pages'),
    '#wysiwyg' => FALSE,
    '#title_display' => 'invisible',
    '#default_value' => variable_get('marketo_rest_pages', MARKETO_REST_PAGES),
    '#description' => t("Specify pages by using their paths. Enter one path per line. The '*' character is a wildcard. Example paths are %blog for the blog page and %blog-wildcard for every personal blog. %front is the front page.", array(
      '%blog' => 'blog',
      '%blog-wildcard' => 'blog/*',
      '%front' => '<front>')
    ),
    '#rows' => 10,
  );

  $form['marketo_rest_tabs']['marketo_rest_role_vis'] = array(
    '#title' => t('Role Visibility'),
    '#type' => 'fieldset',
  );
  $form['marketo_rest_tabs']['marketo_rest_role_vis']['marketo_rest_visibility_roles'] = array(
    '#title' => t('Add tracking to specific roles'),
    '#type' => 'radios',
    '#options' => array(
      1 => t('All roles except those selected below'),
      0 => t('Only the roles selected below'),
    ),
    '#default_value' => variable_get('marketo_rest_visibility_roles', 1),
    '#required' => TRUE,
  );
  $role_options = array_map('check_plain', user_roles());
  $form['marketo_rest_tabs']['marketo_rest_role_vis']['marketo_rest_roles'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Roles'),
    '#default_value' => variable_get('marketo_rest_roles', array()),
    '#options' => $role_options,
  );

  return system_settings_form($form);
}

/**
 * Implements hook_form_validate().
 */
function marketo_rest_admin_settings_form_validate($form, &$form_state) {
  // If using rest, attempt validation.
  if ($form_state['values']['marketo_rest_tracking_method'] == 'rest') {
    $form_state['values']['marketo_rest_endpoint'] = trim($form_state['values']['marketo_rest_endpoint']);
    $form_state['values']['marketo_rest_identity'] = trim($form_state['values']['marketo_rest_identity']);
    $form_state['values']['marketo_rest_client_id'] = trim($form_state['values']['marketo_rest_client_id']);
    $form_state['values']['marketo_rest_client_secret'] = trim($form_state['values']['marketo_rest_client_secret']);

    $proxy = array();
    $proxy['proxy_host'] = trim($form_state['values']['marketo_rest_rest_proxy_host']);
    $proxy['proxy_port'] = trim($form_state['values']['marketo_rest_rest_proxy_port']);
    $proxy['proxy_login'] = trim($form_state['values']['marketo_rest_rest_proxy_login']);
    $proxy['proxy_password'] = trim($form_state['values']['marketo_rest_rest_proxy_password']);

    if (empty($proxy['proxy_host'])) {
      unset($proxy['proxy_host']);
    }
    if (empty($proxy['proxy_port'])) {
      unset($proxy['proxy_port']);
    }
    if (empty($proxy['proxy_login'])) {
      unset($proxy['proxy_login']);
    }
    if (empty($proxy['proxy_password'])) {
      unset($proxy['proxy_password']);
    }

    $rest_is_valid = _marketo_rest_validate_rest_configuration(
      $form_state['values']['marketo_rest_client_id'], $form_state['values']['marketo_rest_client_secret'], $form_state['values']['marketo_rest_endpoint'], $form_state['values']['marketo_rest_identity'], $proxy
    );
    if (!$rest_is_valid) {
      form_set_error('marketo_rest_endpoint', 'Unable to validate REST API settings.');
      form_set_error('marketo_rest_identity');
      form_set_error('marketo_rest_client_id');
      form_set_error('marketo_rest_client_secret');
    }
  }
}

/**
 * Builds and returns the Marketo field definition settings form.
 *
 * @param $form
 * @param $form_state
 * @return mixed
 */
function marketo_rest_field_definition_form($form, &$form_state) {

  $form['marketo_rest_fields'] = array(
    '#title' => t('Field Definition'),
    '#type' => 'fieldset',
    '#description' => t('Enable / disable the fields defined here for mapping to Webform and User Profile fields.'),
  );

  $field_definitions = _marketo_rest_get_field_definitions();
  $field_options = _marketo_rest_get_field_tableselect_options($field_definitions);

  if (isset($form_state['triggering_element']['#array_parents']) && in_array('marketo_rest_lead_fields_rest', $form_state['triggering_element']['#array_parents'])) {
    $marketo_fields = _marketo_rest_get_fields();
    // Make field data accessible to callback.
    $form['#marketo-fields'] = $marketo_fields;
    $field_options += $marketo_fields; // Only merge new array keys.
  }

  $field_values = array();

  // When we have fields, get enabled / disabled values.
  if (!empty($field_options)) {
    $field_values = _marketo_rest_get_field_tableselect_values($field_definitions);
  }

  $form['marketo_rest_fields']['marketo_rest_lead_fields'] = array(
    '#type' => 'tableselect',
    '#header' => array(
      MARKETO_REST_LEAD_FIELD_ID => t('Marketo ID'),
      'name' => t('Display name'),
      MARKETO_REST_LEAD_FIELD_REST_KEY => t('REST key'),
      MARKETO_REST_LEAD_FIELD_MUNCHKIN_KEY => t('Munchkin key'),
    ),
    '#options' => $field_options,
    '#default_value' => $field_values,
    '#prefix' => '<div id="marketo-lead-fields-wrapper">',
    '#suffix' => '</div>',
  );

  $form['marketo_rest_fields']['marketo_rest_lead_fields_rest'] = array(
    '#type' => 'button',
    '#value' => t('Retrieve from Marketo'),
    '#ajax' => array(
      'callback' => '_marketo_rest_webform_fields_rest_callback',
      'wrapper' => 'marketo-lead-fields-wrapper',
    ),
  );

  // If REST settings have not been configured, the button should be disabled.
  if (!_marketo_rest_rest_is_configured()) {
    $form['marketo_rest_fields']['marketo_rest_lead_fields_rest']['#disabled'] = TRUE;
  }

  // Add a submit handler.
  $form['#submit'][] = 'marketo_rest_field_definition_form_submit';

  return system_settings_form($form);
}

/**
 * Form definition submit callback.
 *
 * @param $form
 * @param $form_state
 */
function marketo_rest_field_definition_form_submit($form, &$form_state) {
  // When we have retrieved marketo fields, we may need to map new fields.
  if (!empty($form['#marketo-fields'])) {
    _marketo_rest_sync_field_definitions($form_state['values']['marketo_rest_lead_fields'], $form['#marketo-fields']);
  }
  _marketo_rest_sync_field_definitions($form_state['values']['marketo_rest_lead_fields']);
}


/**
 * Performs validation for Marketo REST API settings.
 * 
 * @param string $client_id
 *   Client Id
 * @param string $client_secret
 *   Client Secret
 * @param string $endpoint
 *   Endpoint
 * @param $identity
 *   Identity endpoint
 * @param $proxy
 * 
 * @return bool
 *   Returns TRUE if REST settings are validated successfully
 */
function _marketo_rest_validate_rest_configuration($client_id, $client_secret, $endpoint, $identity, $proxy) {
  module_load_include('inc', 'marketo_rest', 'includes/marketo_rest.rest');
  $rest_token = variable_get('marketo_rest_token');
  try {
    $client = new MarketoRestClient($client_id, $client_secret, $endpoint, $identity);
    _marketo_rest_persist_access_token($client->getAccessToken($rest_token), $client->getAccessTokenExpiry());
    $result = $client->getFields();
    if (!$result) {
      $result = FALSE;
    }
  }
  catch (Exception $e) {
    $result = FALSE;
  }
  return $result;
}

/**
 * Callback for updating webform fields.
 */
function _marketo_rest_webform_fields_rest_callback($form, &$form_state) {
  return $form['marketo_rest_fields']['marketo_rest_lead_fields'];
}
