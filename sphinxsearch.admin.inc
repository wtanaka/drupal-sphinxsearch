<?php
// $Id: sphinxsearch.admin.inc,v 1.1 2008/08/18 13:51:28 markuspetrux Exp $

/**
 * @file
 * Administration interface for the sphinxsearch module.
 */

/**
 * Module settings form.
 */
function sphinxsearch_settings() {
  $form = array();

  if (sphinxsearch_get_memory_limit() < 0) {
    drupal_set_message(t('Function <em>memory_get_usage</em> not found. Please, make sure your PHP installation is compiled with --enable-memory-limit option.'), 'error');
  }

  $form['server'] = array(
    '#type' => 'fieldset',
    '#title' => t('Sphinx server settings'),
    '#collapsible' => FALSE,
    '#description' => t('Use this section to setup options related to your Sphinx server installation.'),
  );
  $form['server']['sphinxsearch_searchd_host'] = array(
    '#type' => 'textfield',
    '#title' => t('Sphinx searchd host name'),
    '#default_value' => variable_get('sphinxsearch_searchd_host', 'localhost'),
    '#description' => t('Host name where your <strong>Sphinx searchd daemon</strong> is running, i.e. <code>localhost</code>, <code>sphinx.example.com</code> or <code>192.168.0.1</code>.'),
  );
  $form['server']['sphinxsearch_searchd_port'] = array(
    '#type' => 'textfield',
    '#title' => t('Sphinx searchd port'),
    '#default_value' => variable_get('sphinxsearch_searchd_port', '3312'),
    '#description' => t('Port on which your <strong>Sphinx searchd daemon</strong> listens. Default Sphinx port is 3312.'),
  );
  $form['server']['sphinxsearch_searchd_timeout'] = array(
    '#type' => 'select',
    '#title' => t('Sphinx searchd connection timeout'),
    '#default_value' => variable_get('sphinxsearch_searchd_timeout', 0),
    '#options' => drupal_map_assoc(array(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20)),
    '#description' => t('Timeout in seconds used for connections to your <strong>Sphinx searchd daemon</strong>. Leave 0 for PHP defaults for <a href="@fsockopen" target="_blank">fsockopen()</a>.', array('@fsockopen' => 'http://www.php.net/fsockopen')),
  );
  $form['server']['sphinxsearch_indexer_ips'] = array(
    '#type' => 'textfield',
    '#title' => t('Sphinx indexer IP addresses'),
    '#default_value' => variable_get('sphinxsearch_indexer_ips', $_SERVER['SERVER_ADDR']),
    '#description' => t('Enter a comma separated list of IP addresses used by your <strong>Sphinx indexer</strong>. This is necessary to protect access to the XMLPipe generator. Only connections from these IP addresses will be allowed to run this process. You can also specify IP ranges using CIDR notation. Note: IPv6 addresses are not supported.'),
  );
  $form['server']['advanced'] = array(
    '#type' => 'fieldset',
    '#title' => t('Advanced settings'),
    '#collapsible' => TRUE, '#collapsed' => TRUE,
  );
  $form['server']['advanced']['sphinxsearch_searchd_maxquerytime'] = array(
    '#type' => 'select',
    '#title' => t('Maximum query time'),
    '#default_value' => variable_get('sphinxsearch_searchd_maxquerytime', 0),
    '#options' => drupal_map_assoc(array(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20)),
    '#description' => t('Sets the maximum search query time in seconds. Use 0 for no limits. Note that if you\'re using several indexes behind a distributed index, then this limit applies to each local index separately.'),
  );
  $form['server']['advanced']['retries'] = array(
    '#type' => 'fieldset',
    '#title' => t('Distributed retry count and delay'),
    '#collapsible' => FALSE,
    '#description' => t('These options set the retry count and delay in seconds used by <strong>Sphinx searchd daemon</strong> on temporary failures experienced when sending queries to distributed indexes. Retries are disabled by default.'),
  );
  $form['server']['advanced']['retries']['sphinxsearch_retries_count'] = array(
    '#type' => 'select',
    '#title' => t('Number of retries'),
    '#default_value' => variable_get('sphinxsearch_retries_count', 0),
    '#options' => drupal_map_assoc(array(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20)),
  );
  $form['server']['advanced']['retries']['sphinxsearch_retries_delay'] = array(
    '#type' => 'select',
    '#title' => t('Delay in seconds'),
    '#default_value' => variable_get('sphinxsearch_retries_delay', 0),
    '#options' => drupal_map_assoc(array(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20)),
  );

  $form['index'] = array(
    '#type' => 'fieldset',
    '#title' => t('Sphinx index settings'),
    '#collapsible' => FALSE,
    '#description' => t('Use this section to setup options related to your Sphinx indexes.'),
  );
  $form['index']['sphinxsearch_query_index'] = array(
    '#type' => 'textfield',
    '#title' => t('Index name for queries'),
    '#default_value' => variable_get('sphinxsearch_query_index', ''),
    '#description' => t('Name of the Sphinx index used for queries. If you have more than one index (ie. main+delta), you may configure Sphinx with a distributed index that joins all of them (local and/or remote) to provide a single interface to resolve queries.'),
  );
  $form['index']['sphinxsearch_excerpts_index'] = array(
    '#type' => 'textfield',
    '#title' => t('Index name for building excerpts'),
    '#default_value' => variable_get('sphinxsearch_excerpts_index', ''),
    '#description' => t('Regarless of the index used for queries, Sphinx needs an index with tokenizing settings to build excerpts. Sphinx may be unable to build excertps from distributed indexes.'),
  );
  $nodes_per_chunk_options = array(0 => t('All')) + drupal_map_assoc(array(
    250, 500, 700, 750, 1000, 1250, 1500, 1750, 2000, 2250, 2500, 2750, 3000, 3250, 3500, 3750, 4000, 4250, 4500, 4750, 5000,
    6000, 6500, 7000, 7500, 8000, 8500, 9000, 9500, 10000,
    11000, 12000, 13000, 14000, 15000, 16000, 17000, 18000, 19000, 20000,
    25000, 30000, 35000, 40000, 45000, 50000, 60000, 70000, 80000, 90000, 100000,
  ));
  $form['index']['sphinxsearch_nodes_per_chunk'] = array(
    '#type' => 'select',
    '#title' => t('Nodes per chunk'),
    '#default_value' => variable_get('sphinxsearch_nodes_per_chunk', 0),
    '#options' => $nodes_per_chunk_options,
    '#description' => t('During XMLPipe generation, after this number of nodes have been processed, DB server connection is restarted in order to avoid connection timeouts. The time required to generate the text representation for each may vary depending on the complexity of your installation, modules installed, etc. so you can adjust this value to suit your needs. You may first try to index all nodes at once. Then, if you get DB server timeouts, you can play with different amounts, from higher to lower, until no timeout happens.'),
  );
  $form['index']['sphinxsearch_docid_offset'] = array(
    '#type' => 'textfield',
    '#title' => t('Document ID Offset'),
    '#default_value' => variable_get('sphinxsearch_docid_offset', 0),
    '#description' => t('If your Sphinx server manages several indexes for different things, then you may need to ensure documents are indexed with unique identifiers. This option allows you to set a number that will be added to nids when building Sphinx document ids, so it allows you to reserve a range of unique document identifiers for nodes of this particular site. Note that you will have to rebuild your indexes if you change this value.'),
  );
  $sphinxsearch_enabled_node_types = sphinxsearch_get_enabled_node_types();
  $form['index']['nodetypes'] = array(
    '#type' => 'fieldset',
    '#title' => t('Content types'),
    '#collapsible' => TRUE, '#collapsed' => !empty($sphinxsearch_enabled_node_types),
    '#description' => t('Use this option to select content types that will be included in the Sphinx index. By default, if no content type is selected here, nodes of all content types will be indexed.'),
  );
  foreach (node_get_types() as $node_type) {
    $form['index']['nodetypes']['sphinxsearch_include_node_type_'. $node_type->type] = array(
      '#type' => 'checkbox',
      '#title' => $node_type->name,
      '#return_value' => 1,
      '#default_value' => variable_get('sphinxsearch_include_node_type_'. $node_type->type, 0),
    );
  }

  $form['search'] = array(
    '#type' => 'fieldset',
    '#title' => t('Search settings'),
    '#collapsible' => FALSE,
    '#description' => t('Use this section to setup options related to the user interface for searching.'),
  );
  $form['search']['sphinxsearch_results_per_page'] = array(
    '#type' => 'select',
    '#title' => t('Results per page'),
    '#default_value' => variable_get('sphinxsearch_results_per_page', 10),
    '#options' => drupal_map_assoc(array(5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 60, 70, 80, 90, 100)),
    '#description' => t('Specify here how many items will be shown on search results page.'),
  );
  return system_settings_form($form);
}

/**
 * Validate module settings form.
 */
function sphinxsearch_settings_validate($form_id, $form_values) {
  if (!is_numeric($form_values['sphinxsearch_searchd_port']) || (int)$form_values['sphinxsearch_searchd_port'] < 0) {
    form_set_error('sphinxsearch_searchd_port', t('Sphinx port must be a numeric positive value.'));
  }
  if (!is_numeric($form_values['sphinxsearch_docid_offset']) || (int)$form_values['sphinxsearch_docid_offset'] < 0) {
    form_set_error('sphinxsearch_docid_offset', t('Sphinx document ID must be a numeric positive value.'));
  }
}
