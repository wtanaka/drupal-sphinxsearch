<?php
// $Id: sphinxsearch.common.inc,v 1.2 2008/08/30 06:53:31 markuspetrux Exp $

/**
 * @file
 * Common functions for the Sphinx search module.
 *
 * WARNING: Implementation is not finished yet, and it is subject to change in
 * future versions with no backwards compatibility. I should probably create a
 * sample module before it gets into something really reusable... and useful.
 */

/**
 * Delta index identifier; used for XMLPipe processing (internal).
 */
define('SPHINXSEARCH_DELTA_INDEX_ID', -1);

/**
 * List of supported search matching modes.
 */
define('SPHINXSEARCH_MATCH_ALL', 0);
define('SPHINXSEARCH_MATCH_ANY', 1);
define('SPHINXSEARCH_MATCH_PHRASE', 2);

/**
 * Obtain list of supported search matching modes.
 *
 * @return array
 */
function sphinxsearch_get_matching_modes() {
  return array(
    SPHINXSEARCH_MATCH_ALL => t('All words'),
    SPHINXSEARCH_MATCH_ANY => t('Any word'),
    SPHINXSEARCH_MATCH_PHRASE => t('Exact phrase'),
  );
}

/**
 * Obtain list of sortable fields.
 *
 * @return array
 */
function sphinxsearch_get_sortable_fields() {
  return array(
    '@weight' => t('Relevance'),
    'created' => t('Creation time'),
    'last_updated' => t('Last updated time'),
  );
}

/**
 * Obtain the path to Sphinx search page.
 *
 * @return string
 */
function sphinxsearch_get_search_path() {
  return variable_get('sphinxsearch_search_path', 'search-content');
}

/**
 * Check if specified path is the Sphinx search page.
 *
 * @param string $path
 *   Path to check. Defaults to $_GET['q'].
 * @return boolean
 */
function sphinxsearch_is_search_path($path = NULL) {
  if (!isset($path)) {
    $path = $_GET['q'];
  }
  return (strpos($path, sphinxsearch_get_search_path()) === 0);
}

/**
 * Redirect to search page with specified query options.
 *
 * @param mixed $query
 *   This argument accepts a query string array or an already escaped query string.
 */
function sphinxsearch_goto_search($query = NULL) {
  if (isset($query) && is_array($query)) {
    $query = (!empty($query) ? drupal_query_string_encode($query) : NULL);
  }
  drupal_goto(sphinxsearch_get_search_path(), $query);
}

/**
 * Instatiate a Sphinx search client object.
 *
 * Usage:
 *   $sphinxsearch = &sphinxsearch_get_client();
 *
 * @return object
 *   Sphinx client instance.
 */
function &sphinxsearch_get_client() {
  static $sphinxsearch;
  if (!isset($sphinxsearch)) {
    require_once(drupal_get_path('module', 'sphinxsearch') .'/lib/sphinxapi.php');

    $sphinxsearch = new SphinxClient();
    $sphinxsearch->SetServer(variable_get('sphinxsearch_searchd_host', 'localhost'), (int)variable_get('sphinxsearch_searchd_port', '3312'));

    // Setup connection timeout?
    if (($sphinxsearch_searchd_timeout = (int)variable_get('sphinxsearch_searchd_timeout', 0)) > 0) {
      $sphinxsearch->SetConnectTimeout($sphinxsearch_searchd_timeout);
    }

    // Setup max query time?
    if (($sphinxsearch_searchd_maxquerytime = (int)variable_get('sphinxsearch_searchd_maxquerytime', 0)) > 0) {
      $sphinxsearch->SetMaxQueryTime($sphinxsearch_searchd_maxquerytime * 1000);
    }

    // Setup distributed retries?
    if (($sphinxsearch_retries_count = (int)variable_get('sphinxsearch_retries_count', 0)) > 0) {
      $sphinxsearch_retries_delay = (int)variable_get('sphinxsearch_retries_delay', 0);
      $sphinxsearch->SetRetries($sphinxsearch_retries_count, $sphinxsearch_retries_delay * 1000);
    }
  }
  return $sphinxsearch;
}

/**
 * Check connection with Sphinx searchd daemon.
 *
 * @return boolean
 */
function sphinxsearch_check_connection() {
  $sphinxsearch = &sphinxsearch_get_client();
  return $sphinxsearch->_Connect() ? TRUE : FALSE;
}

/**
 * Build search URL data based on the given search options structure.
 *
 * @param array $search_options
 *   Search options structure.
 * @return string
 *   Encoded query string. NULL indicates no search filter has been specified.
 */
function sphinxsearch_get_query_string($search_options) {
  $query = array();

  // Search keywords.
  if (!empty($search_options['filters']['keys'])) {
    $query['keys'] = $search_options['filters']['keys'];
  }

  // Matching modes.
  if (!empty($search_options['matchmode'])) {
    $matchmodes = sphinxsearch_get_matching_modes();
    $matchmode = (int)$search_options['matchmode'];
    if (isset($matchmodes[$matchmode]) && $matchmode != SPHINXSEARCH_MATCH_ALL) {
      $query['matchmode'] = $matchmode;
    }
  }

  // Filter by content author.
  if (!empty($search_options['filters']['author']['name'])) {
    $author = trim($search_options['filters']['author']['name']);
    if (!empty($author)) {
      $query['author'] = $author;
    }
  }

  // Filter by content type.
  if (!empty($search_options['filters']['types'])) {
    $query['types'] = implode(',', $search_options['filters']['types']);
  }

  // Filter by taxonomy.
  if (module_exists('taxonomy')) {
    foreach (sphinxsearch_get_enabled_vocabularies() as $vid => $vocabulary) {
      if (!empty($search_options['filters']['taxonomy'][$vid])) {
        $terms_key = 'terms'. $vid;
        $query[$terms_key] = sphinxsearch_taxonomy_encode_typed_terms($search_options['filters']['taxonomy'][$vid], ',');
      }
    }
  }

  // Sort options.
  if (!empty($search_options['sortfield'])) {
    $sortable_fields = sphinxsearch_get_sortable_fields();
    if (isset($sortable_fields[$search_options['sortfield']]) && $search_options['sortfield'] != '@weight') {
      $query['sortfield'] = $search_options['sortfield'];
    }
  }
  if (!empty($search_options['sortdir']) && $search_options['sortdir'] == 'ASC') {
    $query['sortdir'] = 'ASC';
  }

  return (!empty($query) ? str_replace('%2C', ',', drupal_query_string_encode($query)) : NULL);
}

/**
 * Parse search request and build search options structure.
 *
 * @param array $request_options
 *   Requested search options.
 * @return array
 *   Search options structure.
 */
function sphinxsearch_parse_request($request_options = array()) {
  $search_options = array(
    'matchmode' => SPHINXSEARCH_MATCH_ALL,
    'results_per_page' => (int)variable_get('sphinxsearch_results_per_page', 10),
    'excerpts_limit' => (int)variable_get('sphinxsearch_excerpts_limit', 256),
    'excerpts_around' => (int)variable_get('sphinxsearch_excerpts_around', 5),
    'excerpts_single_passage' => (int)variable_get('sphinxsearch_excerpts_single_passage', 0),
    'filters' => array(),
    'group_by' => '',
    'errors' => array(),
  );

  // Search keywords.
  if (isset($request_options['keys'])) {
    $search_options['filters']['keys'] = preg_replace('#\s+#', ' ', trim($request_options['keys']));
  }

  // Matching modes.
  if (isset($request_options['matchmode'])) {
    $matchmodes = sphinxsearch_get_matching_modes();
    $matchmode = (int)$request_options['matchmode'];
    if (isset($matchmodes[$matchmode])) {
      $search_options['matchmode'] = $matchmode;
    }
  }

  // Filter by content author.
  if (isset($request_options['author'])) {
    $name = trim($request_options['author']);
    if (!empty($name)) {
      $uid = (int)db_result(db_query("SELECT uid FROM {users} WHERE name = '%s'", $name));
      if ($uid <= 0) {
        $search_options['errors']['author'] = t('Specified author %name not found.', array('%name' => $name));
        $search_options['filters']['author'] = array('uid' => -1, 'name' => $name);
      }
      else {
        $search_options['filters']['author'] = array('uid' => $uid, 'name' => $name);
      }
    }
  }

  // Filter by content type.
  if (!empty($request_options['types'])) {
    $enabled_node_types = sphinxsearch_get_enabled_node_types();
    if (count($enabled_node_types) > 1) {
      if (is_array($request_options['types'])) {
        $types = array_values(array_filter($request_options['types']));
      }
      else {
        $types = array_filter(array_map('trim', explode(',', $request_options['types'])));
      }
      $unknown_types = array();
      foreach ($types as $type) {
        if (in_array($type, $enabled_node_types)) {
          if (!isset($search_options['filters']['types'])) {
            $search_options['filters']['types'] = array();
          }
          $search_options['filters']['types'][] = $type;
        }
        else {
          $unknown_types[] = $type;
        }
      }
      if (!empty($unknown_types)) {
        $search_options['errors']['types'] = t('The following content types are invalid: %types.', array(
          '%types' => implode(', ', $unknown_types),
        ));
      }
    }
  }

  // Filter by taxonomy.
  if (module_exists('taxonomy')) {
    foreach (sphinxsearch_get_enabled_vocabularies() as $vid => $vocabulary) {
      $terms_key = 'terms'. $vid;
      if (!empty($request_options[$terms_key])) {
        // Attempt to extract list of tids.
        $tids = array_filter(array_map('intval', array_map('trim', explode(',', $request_options[$terms_key]))));
        if ($request_options[$terms_key] != implode(',', $tids)) {
          // Request came with a comma separated list of terms.
          $terms = sphinxsearch_taxonomy_decode_typed_terms($vid, $request_options[$terms_key]);
        }
        else {
          // Request came with a comma separated list of tids.
          $terms = sphinxsearch_taxonomy_get_terms($vid, $tids);
        }
        // Check if we got not found terms.
        if (isset($terms[-1])) {
          $search_options['errors'][$terms_key] = t('The following terms have not been found in category %category: %terms.', array(
            '%category' => $vocabulary->name,
            '%terms' => $terms[-1],
          ));
          unset($terms[-1]);
        }
        if (!empty($terms)) {
          if (!isset($search_options['filters']['taxonomy'])) {
            $search_options['filters']['taxonomy'] = array();
          }
          $search_options['filters']['taxonomy'][$vid] = $terms;
        }
      }
    }
  }

  // Sort options.
  if (!empty($request_options['sortfield'])) {
    $sortable_fields = sphinxsearch_get_sortable_fields();
    if (isset($sortable_fields[$request_options['sortfield']])) {
      $search_options['sortfield'] = $request_options['sortfield'];
    }
  }
  if (!empty($request_options['sortdir'])) {
    $search_options['sortdir'] = $request_options['sortdir'];
  }

  return $search_options;
}

/**
 * Execute a search query on the given options.
 *
 * @param array $search_options
 *   Search options structure.
 * @return array
 *   Search results structure.
 */
function sphinxsearch_execute_query($search_options) {
  $search_results = array(
    'error_message' => '',
    'warnings' => array(),
    'total_found' => 0,
    'total_available' => 0,
    'time' => 0,
    'words' => array(),
    'nodes' => array(),
    'titles' => array(),
    'excerpts' => array(),
    'groups' => array(),
  );
  $sphinx_query_keywords = $search_options['filters']['keys'];

  // Obtain distributed index name, required to resolve search query.
  $sphinxsearch_query_index = variable_get('sphinxsearch_query_index', '');
  if (empty($sphinxsearch_query_index)) {
    $search_results['error_message'] = t('Sphinx query index not specified.');
    return $search_results;
  }

  // Obtain excerpts index name, required to build excerpts.
  $sphinxsearch_excerpts_index = variable_get('sphinxsearch_excerpts_index', '');
  if (empty($sphinxsearch_excerpts_index)) {
    $search_results['error_message'] = t('Sphinx excerpts index not specified.');
    return $search_results;
  }

  // Validate results per page option.
  if (!isset($search_options['results_per_page']) || $search_options['results_per_page'] <= 0) {
    $search_options['results_per_page'] = 10;
  }

  // Quit if no search filter has been specified.
  if (empty($search_options['filters']) && empty($search_options['group_by'])) {
    return $search_results;
  }

  // Prepare Sphinx client for search queries.
  $current_page = sphinxsearch_get_current_page();
  $sphinxsearch = &sphinxsearch_get_client();
  $sphinxsearch->ResetFilters();
  $sphinxsearch->ResetGroupBy();
  $sphinxsearch->SetLimits($current_page * $search_options['results_per_page'], $search_options['results_per_page']);
  $sphinxsearch->SetFieldWeights(array('subject' => 2, 'content' => 1));
  $sphinxsearch->SetFilter('is_deleted', array(0));

  // Matching modes.
  if ($search_options['matchmode'] == SPHINXSEARCH_MATCH_PHRASE) {
    $sphinx_query_keywords = '"'. trim($sphinxsearch->EscapeString($sphinx_query_keywords)) .'"';
  }
  else {
    $sphinx_query_keywords = implode(
      ($search_options['matchmode'] == SPHINXSEARCH_MATCH_ALL ? ' ' : ' | '),
      array_filter(array_map('trim', explode(' ', $sphinxsearch->EscapeString($sphinx_query_keywords))))
    );
  }
  $sphinxsearch->SetMatchMode(SPH_MATCH_EXTENDED2);

  // Filter by content author.
  if (!empty($search_options['filters']['author'])) {
    $sphinxsearch->SetFilter('uid', array($search_options['filters']['author']['uid']));
  }

  // Filter by content type.
  if (isset($search_options['filters']['types'])) {
    $filter_values = array();
    foreach ($search_options['filters']['types'] as $type) {
      $filter_values[] = sphinxsearch_xmlpipe_nodetype('id', $type);
    }
    if (!empty($filter_values)) {
      $sphinxsearch->SetFilter('nodetype', $filter_values);
    }
  }

  // Filter by taxonomy.
  if (isset($search_options['filters']['taxonomy'])) {
    foreach ($search_options['filters']['taxonomy'] as $vid => $terms) {
      foreach ($terms as $tid => $term) {
        $sphinxsearch->SetFilter('terms'. $vid, array((int)$tid));
      }
    }
  }

  // Sort options.
  $sortdir = (!empty($search_options['sortdir']) && $search_options['sortdir'] == 'ASC' ? 'ASC' : 'DESC');
  if (empty($search_options['sortfield']) || $search_options['sortfield'] == '@weight') {
    $sphinxsearch->SetSortMode(SPH_SORT_EXTENDED, '@weight '. $sortdir .', last_updated '. $sortdir);
  }
  else {
    $sphinxsearch->SetSortMode(SPH_SORT_EXTENDED, $search_options['sortfield'] .' '. $sortdir .', @weight '. $sortdir);
  }

  // Grouping options.
  if (!empty($search_options['group_by'])) {
    $sphinxsearch->SetArrayResult(TRUE);
    $sphinxsearch->SetGroupBy($search_options['group_by'], SPH_GROUPBY_ATTR, '@count DESC, @weight DESC');
  }

  // Send query to Sphinx.
  $sphinx_results = $sphinxsearch->Query($sphinx_query_keywords, $sphinxsearch_query_index);
  if (!$sphinx_results) {
    $message = $sphinxsearch->GetLastError();
    if (!sphinxsearch_check_connection()) {
      $search_results['error_message'] = t('Search service is disabled temporarily. Please, try again later.');
      return $search_results;
    }
    $search_results['error_message'] = t('Search failed using index %index. Sphinx error: %message', array(
      '%index' => $sphinxsearch_query_index,
      '%message' => $message,
    ));
    return $search_results;
  }
  $message = $sphinxsearch->GetLastWarning();
  if (!empty($message)) {
    $search_results['warnings'][] = t('Search query warning: %message', array('%message' => $message));
  }
  if (empty($sphinx_results['matches'])) {
    return $search_results;
  }

  // Save Sphinx query results.
  $search_results['total_found'] = (int)$sphinx_results['total_found'];
  $search_results['total_available'] = (int)$sphinx_results['total'];
  $search_results['time'] = $sphinx_results['time'];
  $search_results['words'] = (isset($sphinx_results['words']) && is_array($sphinx_results['words']) ? $sphinx_results['words'] : array());

  // Parse grouping results.
  if (!empty($search_options['group_by'])) {
    foreach ($sphinx_results['matches'] as $sphinx_match) {
      if (isset($sphinx_match['attrs']['@groupby']) && isset($sphinx_match['attrs']['@count'])) {
        $group_id = $sphinx_match['attrs']['@groupby'];
        if ($search_options['group_by'] == 'nodetype') {
          $group_id = sphinxsearch_xmlpipe_nodetype('name', $group_id);
        }
        $search_results['groups'][$group_id] = array(
          'count' => $sphinx_match['attrs']['@count'],
          'weight' => $sphinx_match['weight'],
        );
      }
    }
    return $search_results;
  }

  // Load nodes referenced by returned results.
  foreach ($sphinx_results['matches'] as $sphinx_docid => $sphinx_match) {
    if (isset($sphinx_match['attrs']['nid']) && ($node = node_load($sphinx_match['attrs']['nid']))) {
      $search_results['nodes'][] = $node;
      $search_results['titles'][] = check_plain($node->title);
      $search_results['excerpts'][] = sphinxsearch_get_node_text($node);
    }
  }

  // Use Sphinx to build excerpts.
  if (!empty($sphinxsearch_excerpts_index)) {
    // Build node titles with highlighted keywords.
    $search_results['titles'] = $sphinxsearch->BuildExcerpts($search_results['titles'], $sphinxsearch_excerpts_index, $sphinx_query_keywords, array(
      'before_match' => '<span class="search-keyword-match">',
      'after_match' => '</span>',
      'chunk_separator' => '',
      'limit' => 1024, // We want all text here, so using a high enough number.
      'around' => 200, // Ignored when single_passage is TRUE.
      'exact_phrase' => ($search_options['matchmode'] == SPHINXSEARCH_MATCH_PHRASE),
      'single_passage' => TRUE,
    ));
    if (!$search_results['titles']) {
      $search_results['titles'] = array();
      $search_results['warnings'][] = t('Unable to build excerpts for content titles. Sphinx error: %message', array('%message' => $sphinxsearch->GetLastError()));
    }

    // Build node excerpts with highlighted keywords.
    $search_results['excerpts'] = $sphinxsearch->BuildExcerpts($search_results['excerpts'], $sphinxsearch_excerpts_index, $sphinx_query_keywords, array(
      'before_match' => '<span class="search-keyword-match">',
      'after_match' => '</span>',
      'chunk_separator' => '<span class="search-chunk-separator"> ... </span>',
      'limit' => $search_options['excerpts_limit'],
      'around' => $search_options['excerpts_around'],
      'exact_phrase' => ($search_options['matchmode'] == SPHINXSEARCH_MATCH_PHRASE),
      'single_passage' => $search_options['excerpts_single_passage'],
    ));
    if (!$search_results['excerpts']) {
      $search_results['excerpts'] = array();
      $search_results['warnings'][] = t('Unable to build excerpts for content snippets. Sphinx error: %message', array('%message' => $sphinxsearch->GetLastError()));
    }
  }

  return $search_results;
}

/**
 * Obtain current page from search results navigation.
 *
 * @param int $pager_element
 *   An optional integer to distinguish between multiple pagers on one page.
 *
 * @return int
 */
function sphinxsearch_get_current_page($pager_element = 0) {
  $pager_page_array = (isset($_GET['page']) ? explode(',', $_GET['page']) : array());
  return (isset($pager_page_array[$pager_element]) ? (int)$pager_page_array[$pager_element] : 0);
}

/**
 * Compute pager options and invoke theme pager.
 *
 * @param int $total_results
 *   The total number of returned search results.
 * @param int $results_per_page
 *   The number of query results to display per page.
 * @param int $pager_element
 *   An optional integer to distinguish between multiple pagers on one page.
 *
 * @return string
 */
function sphinxsearch_pager($total_results, $results_per_page, $pager_element = 0) {
  $GLOBALS['pager_page_array'] = explode(',', $_GET['page']);
  $GLOBALS['pager_total_items'][$pager_element] = $total_results;
  $GLOBALS['pager_total'][$pager_element] = ceil($total_results / $results_per_page);
  $GLOBALS['pager_page_array'][$pager_element] = max(0, min((int)$GLOBALS['pager_page_array'][$pager_element], ((int)$GLOBALS['pager_total'][$pager_element]) - 1));
  return theme('pager', array(), $results_per_page, $pager_element);
}
