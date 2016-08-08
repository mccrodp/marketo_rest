<?php

/**
 * @file
 * Rules integration for Marketo REST
 */

/**
 * Implements hook_rules_action_info().
 */
function marketo_rest_rules_action_info() {
  $actions = array(
    'marketo_rest_rules_action_add_lead' => array(
      'label' => t('Send lead data to Marketo'),
      'group' => t('User'),
      'parameter' => array(
        'account' => array(
          'type' => 'user',
          'label' => t('User account to add as Marketo lead'),
          'save' => FALSE,
        ),
        'data' => array(
          'type' => 'list',
          'label' => t('Marketo data should be a list of pipe delimited strings provided by a variable.'),
        ),
      ),
    ),
    'marketo_rest_rules_action_get_lead' => array(
      'label' => t('Get lead data from Marketo'),
      'group' => t('User'),
      'parameter' => array(
        'account' => array(
          'type' => 'user',
          'label' => t('User account to retrieve'),
          'save' => FALSE,
        ),
      ),
      'provides' => array(
        'marketo_rest_lead' => array(
          'type' => 'text',
          'label' => t('Marketo lead data'),
        ),
      ),
    ),
  );
  return $actions;
}

/**
 * Process data passed as a list from action firing and execute lead update.
 *
 * @param $account
 * @param $data
 */
function marketo_rest_rules_action_add_lead($account, $data) {
  if (!empty($account->mail)) {
    $output = array();
    foreach ($data as $row) {
      $r = explode("|", $row);
      $output[$r[0]] = $r[1];
    }
    $marketo_data = new MarketoRestData($output, variable_get('marketo_rest_tracking_method', MARKETO_REST_TRACKING_METHOD_DEFAULT));
    marketo_rest_add_lead($account->mail, $marketo_data);
  }
}

/**
 * Retrieves a lead from Marketo.
 *
 * @param $account
 * @return array
 */
function marketo_rest_rules_action_get_lead($account) {
  $lead = NULL;
  if (!empty($account->mail)) {
    $lead = marketo_rest_get_lead($account->mail);
  }
  return array(
    'marketo_rest_lead' => json_encode($lead),
  );
}
