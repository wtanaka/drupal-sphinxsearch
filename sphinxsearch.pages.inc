<?php
// $Id: sphinxsearch.pages.inc,v 1.2 2008/08/19 01:37:49 markuspetrux Exp $

/**
 * @file
 * Implementation of pages used to search using Sphinx search module.
 */

/**
 * Menu callback; presents the search form and/or search results.
 */
function sphinxsearch_search_page() {
  // Search form submits with POST but redirects to GET. This way we can keep
  // the search query URL clean as a whistle:
  // SPHINXSEARCH_PATH_PREFIX/keyword+keyword
  if (!isset($_POST['form_id'])) {
    $keys = trim(arg(1));
    $search_options = sphinxsearch_parse_search_options();

    // Construct the search form.
    $output = drupal_get_form('sphinxsearch_search_form', $keys, $search_options);

    // Only perform search if there is non-whitespace search term:
    if (!empty($keys)) {
      // Collect the search results.
      $results = sphinxsearch_search_data($keys, $search_options);

      if ($results) {
        $output .= theme('box', t('Search results'), $results);

        // Log the search keys.
        watchdog('search', t('%keys', array('%keys' => $keys)), WATCHDOG_NOTICE, l(t('results'), SPHINXSEARCH_PATH_PREFIX .'/'. $keys));
      }
      else {
        $output .= theme('box', t('Your search yielded no results'), sphinxsearch_help('search#noresults'));
      }
    }
    return $output;
  }
  return drupal_get_form('sphinxsearch_search_form');
}

/**
 * Parse search options.
 */
function sphinxsearch_parse_search_options() {
  $search_options = array();
  $types = (isset($_GET['types']) ? array_filter(array_map('trim', explode(',', $_GET['types']))) : array());
  $sphinxsearch_enabled_node_types = sphinxsearch_get_enabled_node_types();
  foreach ($types as $type) {
    if (in_array($type, $sphinxsearch_enabled_node_types)) {
      if (!isset($search_options['types'])) {
        $search_options['types'] = array();
      }
      $search_options['types'][] = $type;
    }
  }
  if (isset($_GET['author'])) {
    $search_options['author'] = $_GET['author'];
  }
  return $search_options;
}

/**
 * Render a search form.
 *
 * @param $keys
 *   The search string entered by the user, containing keywords for the search.
 * @return
 *   An HTML string containing the search form.
 */
function sphinxsearch_search_form($keys = '', $search_options = array()) {
  drupal_add_css(drupal_get_path('module', 'sphinxsearch') .'/sphinxsearch.css', 'module', 'all', FALSE);

  $form = array(
    '#action' => url(SPHINXSEARCH_PATH_PREFIX),
    '#attributes' => array('class' => 'search-form'),
  );
  $form['basic'] = array('#type' => 'item', '#title' => t('Enter your keywords'));
  $form['basic']['inline'] = array('#prefix' => '<div class="container-inline">', '#suffix' => '</div>');
  $form['basic']['inline']['keys'] = array(
    '#type' => 'textfield',
    '#title' => '',
    '#default_value' => $keys,
    '#size' => 50,
    '#maxlength' => 255,
  );
  $form['basic']['inline']['submit'] = array('#type' => 'submit', '#value' => t('Search'));

  $form['advanced'] = array(
    '#type' => 'fieldset',
    '#title' => t('Advanced search'),
    '#collapsible' => TRUE,
    '#collapsed' => empty($search_options),
    '#attributes' => array('class' => 'search-advanced'),
  );
  $form['advanced']['author'] = array(
    '#type' => 'textfield',
    '#title' => t('Author'),
    '#prefix' => '<div class="criterion">',
    '#suffix' => '</div>',
    '#maxlength' => 60,
    '#autocomplete_path' => 'user/autocomplete',
    '#default_value' => (isset($search_options['author']) ? $search_options['author'] : ''),
  );
  $system_node_types = array_map('check_plain', node_get_types('names'));
  $search_node_types = array();
  foreach (sphinxsearch_get_enabled_node_types() as $type) {
    $search_node_types[$type] = $system_node_types[$type];
  }
  $form['advanced']['types'] = array(
    '#type' => 'checkboxes',
    '#title' => t('Only of the type(s)'),
    '#prefix' => '<div class="criterion">',
    '#suffix' => '</div>',
    '#options' => $search_node_types,
    '#default_value' => (isset($search_options['types']) ? $search_options['types'] : array()),
  );
  $form['advanced']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Advanced search'),
    '#prefix' => '<div class="action">',
    '#suffix' => '</div>',
  );

  return $form;
}

/**
 * Process a search form submission.
 */
function sphinxsearch_search_form_submit($form_id, $form_values) {
  // Remove redundant whitespaces.
  $keys = preg_replace('#\s+#', ' ', trim($form_values['keys']));
  if (empty($keys)) {
    form_set_error('keys', t('Please enter some keywords.'));
    // Fall through to the drupal_goto() call.
  }
  $query = array();
  if (!empty($form_values['types'])) {
    $types = array();
    foreach ($form_values['types'] as $type => $value) {
      if (!empty($value)) {
        $types[] = $type;
      }
    }
    if (!empty($types)) {
      $query[] = 'types='. implode(',', $types);
    }
  }
  if (!empty($form_values['author'])) {
    $query[] = 'author='. $form_values['author'];
  }
  if (!empty($query)) {
    drupal_goto(SPHINXSEARCH_PATH_PREFIX .'/'. $keys, implode('&', $query));
  }
  drupal_goto(SPHINXSEARCH_PATH_PREFIX .'/'. $keys);
}

/**
 * Perform a standard search on the given keys, and return the formatted results.
 */
function sphinxsearch_search_data($keys = NULL, $search_options = array()) {
  $sphinxsearch_query_index = variable_get('sphinxsearch_query_index', '');
  if (empty($sphinxsearch_query_index)) {
    drupal_set_message(t('Sphinx query index not specified.'), 'error');
    return '';
  }
  $results_per_page = (int)variable_get('sphinxsearch_results_per_page', 10);
  $output = '';
  if (!empty($keys)) {
    $current_page = sphinxsearch_search_get_current_page();
    $sphinxsearch = sphinxsearch_get_client();
    $sphinxsearch->SetLimits($current_page * $results_per_page, $results_per_page);
    $sphinxsearch->SetFieldWeights(array('subject' => 2, 'content' => 1));
    $sphinxsearch->SetMatchMode(SPH_MATCH_EXTENDED);
    $sphinxsearch->SetSortMode(SPH_SORT_EXTENDED, '@weight DESC, changed DESC');
    if (isset($search_options['types'])) {
      $filter_values = array();
      foreach ($search_options['types'] as $type) {
        $filter_values[] = sphinxsearch_get_node_type_id($type);
      }
      $sphinxsearch->SetFilter('nodetype', $filter_values);
    }
    if (!empty($search_options['author'])) {
      $uid = (int)db_result(db_query('SELECT uid FROM {users} WHERE name = \'%s\'', $search_options['author']));
      if ($uid > 0) {
        $sphinxsearch->SetFilter('uid', array($uid));
      }
    }
    $sphinxsearch->SetFilter('is_deleted', array(0));
    $results = $sphinxsearch->Query($keys, $sphinxsearch_query_index);
    if (!$results) {
      sphinxsearch_ui_warning(t('Search failed: !message', array('!message' => $sphinxsearch->GetLastError())));
      if (!sphinxsearch_check_connection()) {
        drupal_set_message(t('Search service is disabled temporarily. Please, try again later.'), 'error');
      }
    }
    else {
      $results['query'] = $keys;
      $message = $sphinxsearch->GetLastWarning();
      if (!empty($message)) {
        sphinxsearch_ui_warning(t('Search query warning: !message', array('!message' => $message)));
      }
      if (!empty($results['matches'])) {
        $query_results = sphinxsearch_parse_query_results($results);
        unset($results); // Free a little memory.
        $output .= theme('sphinxsearch_search_results', $query_results);
      }
    }
  }
  return $output;
}

/**
 * Generate a UI warning.
 *
 * Displays the message if user has 'administer sphinxsearch' privilege.
 * Otherwise, the warning is logged to watchdog.
 *
 * @param string $message
 */
function sphinxsearch_ui_warning($message) {
  if (user_access('administer sphinxsearch')) {
    drupal_set_message($message, 'error');
  }
  else {
    watchdog('sphinxsearch', $message, WATCHDOG_WARNING);
  }
}

/**
 * Parse Sphinx query results into a format that can be easilly themed.
 *
 * @param array $results
 *   All search results as returned by Sphinx query.
 */
function sphinxsearch_parse_query_results($results) {
  $query_results = array(
    'query' => $results['query'],
    'total_found' => (int)$results['total_found'],
    'total' => (int)$results['total'],
    'time' => $results['time'],
    'words' => $results['words'],
    'nodes' => array(),
    'titles' => array(),
    'excerpts' => array(),
  );

  // Load nodes references by returned results.
  foreach ($results['matches'] as $sphinx_docid => $sphinx_match) {
    if (isset($sphinx_match['attrs']['nid']) && ($node = node_load($sphinx_match['attrs']['nid']))) {
      $query_results['nodes'][] = $node;
      $query_results['titles'][] = check_plain($node->title);
      $query_results['excerpts'][] = sphinxsearch_get_node_text($node);
    }
  }

  // Prepare Sphinx client API.
  $sphinxsearch = sphinxsearch_get_client();
  $sphinxsearch_excerpts_index = variable_get('sphinxsearch_excerpts_index', '');
  $sphinxsearch_excerpts_limit = (int)variable_get('sphinxsearch_excerpts_limit', 256);

  // Use Sphinx to build node titles with highlighted keywords.
  $query_results['titles'] = $sphinxsearch->BuildExcerpts($query_results['titles'], $sphinxsearch_excerpts_index, $query_results['query'], array(
    'before_match' => '<span class="search-keyword-match">',
    'after_match' => '</span>',
    'chunk_separator' => '',
    'limit' => $sphinxsearch_excerpts_limit,
    'around' => 100,
    'exact_phrase' => FALSE,
    'single_passage' => FALSE,
  ));
  if (!$query_results['titles']) {
    sphinxsearch_ui_warning(t('Unable to build excerpts for node titles. Sphinx error: !message', array('!message' => $sphinxsearch->GetLastError())));
  }

  // Use Sphinx to build node excerpts with highlighted keywords.
  $query_results['excerpts'] = $sphinxsearch->BuildExcerpts($query_results['excerpts'], $sphinxsearch_excerpts_index, $query_results['query'], array(
    'before_match' => '<span class="search-keyword-match">',
    'after_match' => '</span>',
    'chunk_separator' => '<span class="search-chunk-separator"> ... </span>',
    'limit' => $sphinxsearch_excerpts_limit,
    'around' => 5,
    'exact_phrase' => FALSE,
    'single_passage' => FALSE,
  ));
  if (!$query_results['excerpts']) {
    sphinxsearch_ui_warning(t('Unable to build excerpts for node snippets. Sphinx error: !message', array('!message' => $sphinxsearch->GetLastError())));
  }

  return $query_results;
}

/**
 * Format the results page for the given query results array.
 *
 * @param array $query_results
 *   All search results formatted for easilly theming.
 *   @see sphinxsearch_parse_query_results()
 *
 * @ingroup themeable
 */
function theme_sphinxsearch_search_results($query_results) {
  // Display information about query execution.
  $output = '<p>';
  $output .= format_plural($query_results['total_found'], '1 document found', '@count documents found') .' '. t('in @seconds seconds.', array('@seconds' => round($query_results['time'], 3)));
  if (!empty($query_results['words'])) {
    $words = array();
    foreach ($query_results['words'] as $word => $word_data) {
      $words[] = '<em>'. check_plain($word) .'</em> ('. t('documents: @docs, hits: @hits', array('@docs' => (int)$word_data['docs'], '@hits' => (int)$word_data['hits'])) .')';
    }
    $output .= ' '. t('Terms found:') .' '. implode('; ', $words) .'.';
  }
  $output .= '</p>' ."\n";
  if ($query_results['total_found'] != $query_results['total']) {
    drupal_set_message(t('Warning: only the first @count matches are displayed. You may wish to use additional filters to reduce the number of results.', array('@count' => $query_results['total'])), 'error');
  }

  // Display list of formatted search results.
  $output .= '<dl class="search-results">' ."\n";
  foreach ($query_results['nodes'] as $item_id => $node) {
    $title = (isset($query_results['titles'][$item_id]) ? $query_results['titles'][$item_id] : check_plain($node->title));
    $output .= ' <dt class="title"><a href="'. url('node/'. $node->nid) .'">'. $title .'</a></dt>' ."\n";
    $info = array(
      theme('username', $node),
      format_date($node->created, 'small'),
    );
    $extra = node_invoke_nodeapi($node, 'search result');
    if (!empty($extra) && is_array($extra)) {
      $info = array_merge($info, $extra);
    }
    $excerpt = (isset($query_results['excerpts'][$item_id]) ? '<p class="search-excerpt">'. $query_results['excerpts'][$item_id] .'</p>' ."\n" : '');
    $output .= '<dd>'. $excerpt .'<p class="search-info">'. implode(' - ', $info) .'</p></dd>' ."\n";
  }
  $output .= '</dl>' ."\n";

  // Display pager.
  $output .= sphinxsearch_pager($query_results['total']);
  return $output;
}

/**
 * Obtain current page from search results navigation.
 *
 * @return int
 */
function sphinxsearch_search_get_current_page() {
  $pager_page_array = explode(',', $_GET['page']);
  $pager_element = 0;
  return (isset($pager_page_array[$pager_element]) ? (int)$pager_page_array[$pager_element] : 0);
}

/**
 * Compute pager options and invoke theme pager.
 *
 * @param int $results_total
 *   Total number of returned search results.
 *
 * @return string
 */
function sphinxsearch_pager($results_total) {
  $results_per_page = (int)variable_get('sphinxsearch_results_per_page', 10);
  $pager_element = 0;
  $GLOBALS['pager_page_array'] = explode(',', $_GET['page']);
  $GLOBALS['pager_total_items'][$pager_element] = $results_total;
  $GLOBALS['pager_total'][$pager_element] = ceil($results_total / $results_per_page);
  $GLOBALS['pager_page_array'][$pager_element] = max(0, min((int)$GLOBALS['pager_page_array'][$pager_element], ((int)$GLOBALS['pager_total'][$pager_element]) - 1));
  return theme('pager', array(), $results_per_page, $pager_element);
}
