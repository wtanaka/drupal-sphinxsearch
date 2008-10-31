<?php
// $Id: sphinxsearch.admin.inc,v 1.4.2.3 2008/10/31 13:15:45 markuspetrux Exp $

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
  $form['server']['searchd'] = array(
    '#type' => 'fieldset',
    '#collapsible' => FALSE,
  );
  $form['server']['searchd']['sphinxsearch_searchd_host'] = array(
    '#type' => 'textfield',
    '#title' => t('Sphinx searchd host name'),
    '#default_value' => variable_get('sphinxsearch_searchd_host', 'localhost'),
    '#description' => t('Host name where your <strong>Sphinx searchd daemon</strong> is running, i.e. <code>localhost</code>, <code>sphinx.example.com</code> or <code>192.168.0.1</code>.'),
    '#required' => TRUE,
  );
  $form['server']['searchd']['sphinxsearch_searchd_port'] = array(
    '#type' => 'textfield',
    '#title' => t('Sphinx searchd port'),
    '#default_value' => variable_get('sphinxsearch_searchd_port', '3312'),
    '#description' => t('Port on which your <strong>Sphinx searchd daemon</strong> listens. Default Sphinx port is 3312.'),
    '#required' => TRUE,
  );
  $form['server']['searchd']['sphinxsearch_searchd_timeout'] = array(
    '#type' => 'select',
    '#title' => t('Sphinx searchd connection timeout'),
    '#default_value' => variable_get('sphinxsearch_searchd_timeout', 0),
    '#options' => drupal_map_assoc(array(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20)),
    '#description' => t('Timeout in seconds used for connections to your <strong>Sphinx searchd daemon</strong>. Leave 0 for PHP defaults for <a href="@fsockopen" target="_blank">fsockopen()</a>.', array('@fsockopen' => 'http://www.php.net/fsockopen')),
  );
  $form['server']['searchd']['sphinxsearch_indexer_ips'] = array(
    '#type' => 'textfield',
    '#title' => t('Sphinx indexer IP addresses'),
    '#default_value' => variable_get('sphinxsearch_indexer_ips', $_SERVER['SERVER_ADDR']),
    '#description' => t('Enter a comma separated list of IP addresses used by your <strong>Sphinx indexer</strong>. This is necessary to protect access to the XMLPipe generator. Only connections from these IP addresses will be allowed to run this process. You can also specify IP ranges using <a href="http://en.wikipedia.org/wiki/Classless_Inter-Domain_Routing" target="_blank">CIDR notation</a>. IPv6 addresses are not supported.'),
    '#required' => TRUE,
  );
  $form['server']['query'] = array(
    '#type' => 'fieldset',
    '#title' => t('Query connection settings'),
    '#collapsible' => TRUE, '#collapsed' => TRUE,
  );
  $form['server']['query']['limits'] = array(
    '#type' => 'fieldset',
    '#collapsible' => FALSE,
  );
  $form['server']['query']['limits']['sphinxsearch_searchd_maxquerytime'] = array(
    '#type' => 'select',
    '#title' => t('Maximum query time'),
    '#default_value' => variable_get('sphinxsearch_searchd_maxquerytime', 0),
    '#options' => drupal_map_assoc(array(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20)),
    '#description' => t('Sets the maximum search query time in seconds. Use 0 for no limits. Note that if you\'re using several indexes behind a distributed index, then this limit applies to each local index separately.'),
  );
  $form['server']['query']['retries'] = array(
    '#type' => 'fieldset',
    '#title' => t('Distributed retry count and delay'),
    '#collapsible' => FALSE,
    '#description' => t('These options set the retry count and delay in seconds used by <strong>Sphinx searchd daemon</strong> on temporary failures that may happen when sending queries to distributed indexes. Retries are disabled by default.'),
  );
  $form['server']['query']['retries']['inline'] = array('#prefix' => '<div class="container-inline">', '#suffix' => '</div>');
  $form['server']['query']['retries']['inline']['sphinxsearch_retries_count'] = array(
    '#type' => 'select',
    '#title' => t('Number of retries'),
    '#default_value' => variable_get('sphinxsearch_retries_count', 0),
    '#options' => drupal_map_assoc(array(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20)),
  );
  $form['server']['query']['retries']['inline']['separator'] = array('#value' => '&nbsp;&nbsp;&nbsp;&nbsp;');
  $form['server']['query']['retries']['inline']['sphinxsearch_retries_delay'] = array(
    '#type' => 'select',
    '#title' => t('Delay in seconds'),
    '#default_value' => variable_get('sphinxsearch_retries_delay', 0),
    '#options' => drupal_map_assoc(array(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20)),
  );

  $form['index'] = array(
    '#type' => 'fieldset',
    '#title' => t('Sphinx index settings'),
    '#collapsible' => FALSE,
    '#description' => t('Use this section to setup options related to your Sphinx indexes. Note that there a few options that will require you to rebuild your Sphinx indexes if you change them.'),
  );
  $form['index']['indexnames'] = array(
    '#type' => 'fieldset',
    '#collapsible' => FALSE,
  );
  $form['index']['indexnames']['sphinxsearch_query_index'] = array(
    '#type' => 'textfield',
    '#title' => t('Index name for queries'),
    '#default_value' => variable_get('sphinxsearch_query_index', ''),
    '#description' => t('Name of the Sphinx index used for queries. If you have more than one index (ie. main+delta), you may configure Sphinx with a distributed index that joins all of them (local and/or remote) to provide a single interface to resolve queries.'),
  );
  $form['index']['indexnames']['sphinxsearch_excerpts_index'] = array(
    '#type' => 'textfield',
    '#title' => t('Index name for building excerpts'),
    '#default_value' => variable_get('sphinxsearch_excerpts_index', ''),
    '#description' => t('Regardless of the index used for queries, Sphinx needs an index with tokenizing settings defined to build excerpts. Sphinx may be unable to build excertps from distributed indexes. It is recommended to use the name of your first main index for this purpose.'),
  );
  $form['index']['indexer'] = array(
    '#type' => 'fieldset',
    '#title' => t('Indexer settings'),
    '#collapsible' => TRUE, '#collapsed' => TRUE,
  );
  $form['index']['indexer']['xmlpipe'] = array(
    '#type' => 'fieldset',
    '#title' => t('XMLPipe generation settings'),
    '#collapsible' => FALSE,
    '#description' => t('If your indexes take a long time to generate, or your site manages a huge number of nodes, you may need to adjust PHP settings provided in the <strong>.htaccess</strong> file located in the <strong>sphinxsearch_scripts</strong> subdirectory of this module. Additionally, you can use the following options to tell XMLPipe generator to process nodes in chunks to avoid table locks and/or to prevent DB server connection timeouts.'),
  );
  $nodes_per_chunk_options = array(0 => t('All')) + drupal_map_assoc(array(
    250, 500, 700, 750, 1000, 1250, 1500, 1750, 2000, 2250, 2500, 2750, 3000, 3250, 3500, 3750, 4000, 4250, 4500, 4750, 5000,
    6000, 6500, 7000, 7500, 8000, 8500, 9000, 9500, 10000,
    11000, 12000, 13000, 14000, 15000, 16000, 17000, 18000, 19000, 20000,
    25000, 30000, 35000, 40000, 45000, 50000, 60000, 70000, 80000, 90000, 100000,
  ));
  $form['index']['indexer']['xmlpipe']['sphinxsearch_nodes_per_chunk'] = array(
    '#type' => 'select',
    '#title' => t('Nodes per chunk'),
    '#default_value' => variable_get('sphinxsearch_nodes_per_chunk', 0),
    '#options' => $nodes_per_chunk_options,
    '#description' => t('Use this option to tell XMLPipe generator to process node in chunks. A new and different SELECT statement against the nodes table will be executed for each chunk. Use 0 to process all nodes at once.'),
  );
  $form['index']['indexer']['xmlpipe']['sphinxsearch_chunks_before_restart'] = array(
    '#type' => 'select',
    '#title' => t('Chunks before DB restart'),
    '#default_value' => variable_get('sphinxsearch_chunks_before_restart', 0),
    '#options' => drupal_map_assoc(array(0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 60, 70, 80, 90, 100)),
    '#description' => t('Use this option to tell XMLPipe generator to restart DB server conecction after this specified number of chunks. Use 0 to process all nodes in one single connection.'),
  );
  $form['index']['nodetypes'] = array(
    '#type' => 'fieldset',
    '#title' => t('Content type settings'),
    '#collapsible' => TRUE, '#collapsed' => (bool)count(sphinxsearch_get_enabled_node_types()),
    '#description' => t('Use these options to select content types that you wish to be included in your Sphinx indexes. By default, if no content type is selected here, nodes of all content types will be indexed. <strong>Note that you will have to rebuild your indexes if you change these options.</strong>'),
  );
  foreach (node_get_types() as $node_type) {
    $form['index']['nodetypes']['sphinxsearch_include_node_type_'. $node_type->type] = array(
      '#type' => 'checkbox',
      '#title' => $node_type->name,
      '#return_value' => 1,
      '#default_value' => variable_get('sphinxsearch_include_node_type_'. $node_type->type, 0),
    );
  }
  $form['index']['taxonomy'] = array(
    '#type' => 'fieldset',
    '#title' => t('Taxonomy settings'),
    '#collapsible' => TRUE, '#collapsed' => (bool)count(sphinxsearch_get_enabled_vocabularies()),
    '#description' => t('Use these options to select vocabularies that you wish to index. By default, if no vocabulary is selected here, all vocabularies in use for enabled content types will be indexed. <strong>Note that you will have to rebuild your indexes if you change these options and/or if you create new vocabularies.</strong>'),
  );
  if (module_exists('taxonomy')) {
    foreach (taxonomy_get_vocabularies() as $vocabulary) {
      $form['index']['taxonomy']['sphinxsearch_include_vocabulary_'. $vocabulary->vid] = array(
        '#type' => 'checkbox',
        '#title' => $vocabulary->name,
        '#return_value' => 1,
        '#default_value' => variable_get('sphinxsearch_include_vocabulary_'. $vocabulary->vid, 0),
      );
    }
  }
  else {
    $form['index']['taxonomy']['not_available'] = array(
      '#value' => '<div class="error">'. t('This feature is not available. Taxonomy module is not installed. Note that if you ever enable the taxonomy module and wish to provide additional filtering options for your vocabularies you will have to review these options and rebuild your Sphinx indexes.') .'</div>',
    );
  }

  $form['search'] = array(
    '#type' => 'fieldset',
    '#title' => t('Search page settings'),
    '#collapsible' => FALSE,
    '#description' => t('Use these options to customize the behaviour of your search page.'),
  );
  $form['search']['results'] = array(
    '#type' => 'fieldset',
    '#collapsible' => FALSE,
  );
  $search_path_description = t('Use this option to setup the path for the main search page.');
  if (module_exists('search')) {
    $search_path_description .= ' '. t("Note that Drupal search module is installed. You cannot take over the 'search' path.");
  }
  else {
    $search_path_description .= ' '. t("Note that Drupal search module is not installed. You can take over the 'search' path.");
  }
  $form['search']['results']['sphinxsearch_search_path'] = array(
    '#type' => 'textfield',
    '#title' => t('Search path'),
    '#default_value' => variable_get('sphinxsearch_search_path', 'search-content'),
    '#description' => $search_path_description,
    '#required' => TRUE,
  );
  $form['search']['results']['sphinxsearch_results_per_page'] = array(
    '#type' => 'select',
    '#title' => t('Results per page'),
    '#default_value' => variable_get('sphinxsearch_results_per_page', 10),
    '#options' => drupal_map_assoc(array(5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 60, 70, 80, 90, 100)),
    '#description' => t('How many items will be shown on search results page.'),
  );
  $form['search']['results']['sphinxsearch_log_search_keys'] = array(
    '#type' => 'radios',
    '#title' => t('Log search keys'),
    '#default_value' => variable_get('sphinxsearch_log_search_keys', 'always'),
    '#options' => array(
      'always' => t('Always'),
      'keys-only' => t('Only when keywords have been specified'),
      'never' => t('Never'),
    ),
    '#description' => t('Information about search requests can be logged to watchdog for later review. Use this option to disable this feature or to only enable logging for requests where search keywords have been specified. Note that you can also setup Sphinx to log all search queries (see parameter <code>query_log</code> in your sphinx.conf), in which case you may wish to disable this feature here.'),
  );
  $form['search']['flood'] = array(
    '#type' => 'fieldset',
    '#title' => t('Flood control settings'),
    '#collapsible' => TRUE, '#collapsed' => TRUE,
    '#description' => t('Use these options to set the maximum number of search queries a user can perform per hour. Users with %admin permission are not affected by these restrictions.', array('%admin' => t('administer sphinxsearch'))),
  );
  $user_roles = array(DRUPAL_ANONYMOUS_RID => t('anonymous user'), DRUPAL_AUTHENTICATED_RID => t('authenticated user'));
  foreach ($user_roles as $rid => $role) {
    $form['search']['flood']['sphinxsearch_search_flood_per_role_'. $rid] = array(
      '#type' => 'select',
      '#title' => t('Hourly threshold per %role', array('%role' => $role)),
      '#options' => (
        array(0 => t('Unlimited')) +
        drupal_map_assoc(array(10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170, 180, 190, 200, 210, 220, 230, 240, 250, 260, 270, 280, 290, 300, 350, 400, 450, 500))
      ),
      '#default_value' => variable_get('sphinxsearch_search_flood_per_role_'. $rid, 0),
    );
  }
  $form['search']['excerpts'] = array(
    '#type' => 'fieldset',
    '#title' => t('Excerpts builder settings'),
    '#collapsible' => TRUE, '#collapsed' => TRUE,
    '#description' => t('Use these options to customize how to build excerpts for search results.'),
  );
  $form['search']['excerpts']['sphinxsearch_excerpts_limit'] = array(
    '#type' => 'select',
    '#title' => t('Excerpts size limit'),
    '#default_value' => variable_get('sphinxsearch_excerpts_limit', 256),
    '#options' => drupal_map_assoc(array(128, 192, 256, 320, 384, 448, 512, 640, 768, 896, 1024)),
    '#description' => t('Maximum snippet size (in Sphinx symbols) used to build content excerpts for search results.'),
  );
  $form['search']['excerpts']['sphinxsearch_excerpts_around'] = array(
    '#type' => 'select',
    '#title' => t('Words around matches'),
    '#default_value' => variable_get('sphinxsearch_excerpts_around', 5),
    '#options' => drupal_map_assoc(array(1, 2, 3, 4, 5, 6, 7, 8, 9, 10)),
    '#description' => t('How much words to pick around each matching keywords block when building excerpts. Note that the actual number of words around matches also depends on Sphinx index configuration options such as <code>min_word_len</code>, <code>charset_table</code>, etc.'),
  );
  $form['search']['excerpts']['sphinxsearch_excerpts_single_passage'] = array(
    '#type' => 'radios',
    '#title' => t('Single passage'),
    '#default_value' => variable_get('sphinxsearch_excerpts_single_passage', 0),
    '#options' => array(1 => t('Enabled'), 0 => t('Disabled')),
    '#description' => t('Whether to extract single best passage only. When this option is enabled, <em>Words around matches</em> setting is ignored and excerpts are build with a single passage up to <em>Excerpts size limit</em> long.'),
  );
  if (module_exists('taxonomy') && !module_exists('tagadelic')) {
    $form['search']['tagadelic'] = array(
      '#type' => 'fieldset',
      '#title' => t('Tagadelic settings'),
      '#collapsible' => TRUE, '#collapsed' => TRUE,
      '#description' => t('Use these options to customize the behaviour of <a href="@tagadelic">tagadelic pages</a>.', array('@tagadelic' => url('tagadelic'))),
    );
    $form['search']['tagadelic']['sphinxsearch_page_tagadelic_sortmode'] = array(
      '#type' => 'radios',
      '#title' => t('Tagadelic sort order'),
      '#options' => array(
        'weight,asc' => t('by weight, ascending'),
        'weight,desc' => t('by weight, descending'),
        'title,asc' => t('by title, ascending'),
        'title,desc' => t('by title, descending'),
        'random,none' => t('random')
      ),
      '#default_value' => variable_get('sphinxsearch_page_tagadelic_sortmode', 'title,asc'),
      '#description' => t('Determines the sort order of the tags in the cloud.'),
    );
    $form['search']['tagadelic']['sphinxsearch_page_tagadelic_tags'] = array(
      '#type' => 'select',
      '#title' => t('Tags to show'),
      '#options' => drupal_map_assoc(array(40, 50, 60, 70, 80, 90, 100, 110, 120, 130, 140, 150, 160, 170, 180, 190, 200, 210, 220, 230, 240, 250, 260, 270, 280, 290, 300)),
      '#default_value' => variable_get('sphinxsearch_page_tagadelic_tags', 100),
      '#description' => t('The number of tags to show in this block.'),
    );
    $form['search']['tagadelic']['sphinxsearch_page_tagadelic_levels'] = array(
      '#type' => 'select',
      '#title' => t('Number of levels'),
      '#options' => drupal_map_assoc(array(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16)),
      '#default_value' => variable_get('sphinxsearch_page_tagadelic_levels', 10),
      '#description' => t('The number of levels between the least popular tags and the most popular ones. Different levels will be assigned a different class to be themed (see <code>sphinxsearch.css</code>).'),
    );
    $form['search']['tagadelic']['sphinxsearch_page_tagadelic_linkto'] = array(
      '#type' => 'radios',
      '#title' => t('Terms in the cloud link to'),
      '#options' => array(
        'taxonomy' => t('Taxonomy managed links.'),
        'search' => t('Start search with predefined filters.'),
      ),
      '#default_value' => variable_get('sphinxsearch_page_tagadelic_linkto', 'taxonomy'),
      '#description' => t('Determines where the links of terms in the tag cloud should point to.'),
    );
  }

  // Let default settings form do its own job, and then
  // inject our own submit handler.
  $form = system_settings_form($form);
  $form['#submit'][] = 'sphinxsearch_settings_submit';
  return $form;
}

/**
 * Validate module settings form.
 */
function sphinxsearch_settings_validate($form, &$form_state) {
  if (!is_numeric($form_state['values']['sphinxsearch_searchd_port']) || (int)$form_state['values']['sphinxsearch_searchd_port'] < 0) {
    form_set_error('sphinxsearch_searchd_port', t('Sphinx port must be a numeric positive value.'));
  }
  if (strpos($form_state['values']['sphinxsearch_search_path'], ' ') !== FALSE) {
    form_set_error('sphinxsearch_search_path', t('Search path cannot contain whitespaces.'));
  }
  else if ($form_state['values']['sphinxsearch_search_path'] == 'search' && module_exists('search')) {
    form_set_error('sphinxsearch_search_path', t("You should first uninstall the Drupal search module to be able to take over the <code>'search'</code> path."));
  }
}

/**
 * Submit handler for module settings form.
 */
function sphinxsearch_settings_submit($form, &$form_state) {
  // Rebuild menus when search page path has been changed.
  if ($form_state['values']['sphinxsearch_search_path'] != $form['search']['results']['sphinxsearch_search_path']['#default_value']) {
    menu_rebuild();
  }
}

/**
 * Menu callback; Diagnose Sphinx searchd daemon connection.
 */
function sphinxsearch_check_connection_page() {
  $steps = array();

  $sphinxsearch = &sphinxsearch_get_client();
  $sphinxsearch_query_index = variable_get('sphinxsearch_query_index', '');
  $sphinxsearch_excerpts_index = variable_get('sphinxsearch_excerpts_index', '');

  // Check connection.
  $steps['connection'] = array(
    'title' => t('Testing Sphinx searchd daemon connection.'),
    'messages' => array(),
  );
  if (sphinxsearch_check_connection()) {
    $steps['connection']['messages'][] = t('OK') .' - '. t('Your Sphinx searchd daemon is up and running.');
  }
  else {
    $steps['connection']['messages'][] = t('ERROR') .' - '. t('Unable to connect to your Sphinx searchd daemon.') . theme('item_list', array(
      t('searchd host: %host', array('%host' => variable_get('sphinxsearch_searchd_host', 'localhost'))),
      t('searchd port: %port', array('%port' => (int)variable_get('sphinxsearch_searchd_port', '3312'))),
      t('Sphinx message: %message', array('%message' => $sphinxsearch->GetLastError())),
    ));
  }

  // Check distributed index.
  $steps['query'] = array(
    'title' => t('Testing search query using index %index.', array('%index' => $sphinxsearch_query_index)),
    'messages' => array(
      t('Query asks for a list of all indexed documents to retrieve the last indexed node.'),
      t('MySQL equivalent would look something like:<br /><code>SELECT nid FROM {node} ORDER BY nid DESC LIMIT 1;</code>'),
    ),
  );
  if (empty($sphinxsearch_query_index)) {
    $steps['query']['messages'][] = t('ERROR') .' - '. t('Sphinx query index not specified. Please, check module settings to match your Sphinx server configuration.');
  }
  else {
    $sphinxsearch->SetLimits(0, 1);
    $sphinxsearch->SetSortMode(SPH_SORT_EXTENDED, 'nid DESC');
    $sphinxsearch->SetFilter('is_deleted', array(0));
    $sphinx_results = $sphinxsearch->Query('', $sphinxsearch_query_index);
    if (!$sphinx_results) {
      $steps['query']['messages'][] = t('ERROR') .' - '. t('Search query failed.') . theme('item_list', array(
        t('Sphinx message: %message', array('%message' => $sphinxsearch->GetLastError())),
      ));
    }
    else {
      if (!empty($sphinx_results['matches'])) {
        $sphinx_match = array_shift($sphinx_results['matches']);
        $nid = $sphinx_match['attrs']['nid'];
        $updated = '('. format_date($sphinx_match['attrs']['last_updated'], 'custom', 'Y-m-d H:i:s') .')';
      }
      else {
        $nid = t('N/A - Oops! It apears that your Sphinx indexes are empty.');
        $updated = '';
      }
      $steps['query']['messages'][] = t('OK') .' - '. t('Test passed successfully. Sphinx results summary follow:') . theme('item_list', array(
        t('Query execution time: @time seconds', array('@time' => $sphinx_results['time'])),
        t('Total results found: @total', array('@total' => $sphinx_results['total_found'])),
        t('Total results available: @total', array('@total' => $sphinx_results['total'])),
        t('Last indexed node: @nid @updated', array('@nid' => $nid, '@updated' => $updated)),
      ));
    }
    $warning = $sphinxsearch->GetLastWarning();
    if (!empty($warning)) {
      $steps['query']['messages'][] = t('WARNING') .' - '. t('There was a problem while trying to resolve the query.') . theme('item_list', array(
        t('Sphinx message: %message', array('%message' => $warning)),
      ));
    }
  }

  // Check excerpts index.
  $steps['excerpts'] = array(
    'title' => t('Testing excerpts builder using index %index.', array('%index' => $sphinxsearch_excerpts_index)),
    'messages' => array(),
  );
  if (empty($sphinxsearch_excerpts_index)) {
    $steps['excerpts']['messages'][] = t('ERROR') .' - '. t('Sphinx excerpts index not specified. Please, check module settings to match your Sphinx server configuration.');
  }
  else {
    $excerpt_samples = array(t('The quick brown fox jumps over a lazy dog.'));
    $excerpt_keywords = t('brown lazy');
    $excerpt_results = $sphinxsearch->BuildExcerpts($excerpt_samples, $sphinxsearch_excerpts_index, $excerpt_keywords);
    if (!$excerpt_results) {
      $steps['excerpts']['messages'][] = t('ERROR') .' - '. t('Unable to build excerpts.') . theme('item_list', array(
        t('Sphinx message: %message', array('%message' => $sphinxsearch->GetLastError())),
      ));
    }
    else {
      $steps['excerpts']['messages'][] = t('OK') .' - '. t('Test passed successfully. Sphinx results summary follow:') . theme('item_list', array(
        t('Sample text: <code>@sample</code>', array('@sample' => $excerpt_samples[0])),
        t('Keywords: <code>@keywords</code>', array('@keywords' => $excerpt_keywords)),
        t('Result: <code>@result</code>', array('@result' => $excerpt_results[0])),
      ));
    }
  }

  // Format HTML report.
  $items = array();
  foreach ($steps as $step) {
    $items[] = theme('item_list', $step['messages'], $step['title']);
  }

  return theme('item_list', $items);
}
