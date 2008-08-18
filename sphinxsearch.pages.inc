<?php
// $Id: sphinxsearch.pages.inc,v 1.1 2008/08/18 13:51:28 markuspetrux Exp $

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
      if (user_access('administer sphinxsearch')) {
        drupal_set_message($sphinxsearch->GetLastError(), 'error');
      }
      else {
        drupal_set_message(t('Search service is disabled temporarily. Please, try later.'), 'error');
        watchdog('sphinxsearch', t('Search failed: !message', array('!message' => $sphinxsearch->GetLastError())), WATCHDOG_WARNING);
      }
    }
    else {
      $results['query'] = $keys;
      $warning = $sphinxsearch->GetLastWarning();
      if (!empty($warning)) {
        $warning = t('Warning: !warning', array('!warning' => $warning));
        if (user_access('administer sphinxsearch')) {
          drupal_set_message($warning, 'error');
        }
        else {
          watchdog('sphinxsearch', $warning, WATCHDOG_WARNING);
        }
      }
      if (!empty($results['matches'])) {
        $output .= theme('sphinxsearch_search_results', $results);
      }
    }
  }
  return $output;
}

/**
 * Format the result page of a search query.
 *
 * @param array $results
 *   All search result as returned by Sphinx query.
 *
 * @ingroup themeable
 */
function theme_sphinxsearch_search_results($results) {
  $output = '<p>';
  $output .= format_plural((int)$results['total_found'], '1 document found in', '@count documents found in') .' '. t('@seconds seconds.', array('@seconds' => round($results['time'], 3)));
  if (!empty($results['words'])) {
    $words = array();
    foreach ($results['words'] as $word => $word_data) {
      $words[] = '<em>'. check_plain($word) .'</em> ('. t('documents: @docs, hits: @hits', array('@docs' => (int)$word_data['docs'], '@hits' => (int)$word_data['hits'])) .')';
    }
    $output .= ' '. t('Terms found:') .' '. implode('; ', $words) .'.';
  }
  $output .= '</p>' ."\n";

  $matched_nodes = array();
  $titles = $excerpts = array();
  foreach ($results['matches'] as $sphinx_docid => $sphinx_match) {
    if (isset($sphinx_match['attrs']['nid']) && ($node = node_load($sphinx_match['attrs']['nid']))) {
      $matched_nodes[] = $node;
      $titles[] = check_plain($node->title);
      $excerpts[] = sphinxsearch_get_node_text($node);
    }
  }

  // Use Sphinx to build excerpts with highlighted keywords
  $sphinxsearch = sphinxsearch_get_client();
  $sphinxsearch_excerpts_index = variable_get('sphinxsearch_excerpts_index', '');
  $excerpts = $sphinxsearch->BuildExcerpts($excerpts, $sphinxsearch_excerpts_index, $results['query'], array(
    'before_match' => '<span class="search-keyword-match">',
    'after_match' => '</span>',
    'chunk_separator' => '<span class="search-chuck-separator"> ... </span>',
    'limit' => 256,
    'around' => 3,
    'exact_phrase' => FALSE,
    'single_passage' => FALSE,
  ));
  if (!$excerpts) {
    drupal_set_message($sphinxsearch->GetLastError(), 'error');
  }
  $titles = $sphinxsearch->BuildExcerpts($titles, $sphinxsearch_excerpts_index, $results['query'], array(
    'before_match' => '<span class="search-keyword-match">',
    'after_match' => '</span>',
    'chunk_separator' => '',
    'limit' => 256,
    'around' => 100,
    'exact_phrase' => FALSE,
    'single_passage' => FALSE,
  ));
  if (!$titles) {
    drupal_set_message($sphinxsearch->GetLastError(), 'error');
  }

/*
// Snippet that could be used to group results by term,
// maybe to build tag cloud or something similar.
$sphinxsearch_query_index = variable_get('sphinxsearch_query_index', '');
$sphinxsearch->SetArrayResult(TRUE);
$sphinxsearch->SetGroupBy('tid', SPH_GROUPBY_ATTR, '@count DESC');
$rc = $sphinxsearch->Query($results['query'], $sphinxsearch_query_index);
  foreach ($rc['matches'] as $sphinx_docid => $sphinx_match) {
    print 'tid:'. $sphinx_match['attrs']['@groupby'] .', count:'. $sphinx_match['attrs']['@count'];
  }
//*/

  $output .= '<dl class="search-results">';
  foreach ($matched_nodes as $item_id => $node) {
    $output .= ' <dt class="title"><a href="'. url('node/'. $node->nid) .'">'. (isset($titles[$item_id]) ? $titles[$item_id] : check_plain($node->title)) .'</a></dt>';
    $info = array(
      theme('username', $node),
      format_date($node->created, 'small'),
    );
    $extra = node_invoke_nodeapi($node, 'search result');
    if (!empty($extra) && is_array($extra)) {
      $info = array_merge($info, $extra);
    }
    $output .= ' <dd>'. (isset($excerpts[$item_id]) ? '<p class="search-excerpt">'. $excerpts[$item_id] .'</p>' : '') .'<p class="search-info">'. implode(' - ', $info) .'</p></dd>';
  }
  $output .= '</dl>';
  $output .= sphinxsearch_pager($results);
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
 * @param array $results
 *   All search result as returned by Sphinx query.
 *
 * @return string
 */
function sphinxsearch_pager($results) {
  $results_per_page = (int)variable_get('sphinxsearch_results_per_page', 10);
  $results_total = (int)$results['total_found'];
  $pager_element = 0;
  $GLOBALS['pager_page_array'] = explode(',', $_GET['page']);
  $GLOBALS['pager_total_items'][$pager_element] = $results_total;
  $GLOBALS['pager_total'][$pager_element] = ceil($results_total / $results_per_page);
  $GLOBALS['pager_page_array'][$pager_element] = max(0, min((int)$GLOBALS['pager_page_array'][$pager_element], ((int)$GLOBALS['pager_total'][$pager_element]) - 1));
  return theme('pager', array(), $results_per_page, $pager_element);
}
