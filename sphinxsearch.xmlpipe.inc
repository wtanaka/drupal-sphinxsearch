<?php
// $Id: sphinxsearch.xmlpipe.inc,v 1.4.2.3 2008/10/31 13:15:45 markuspetrux Exp $

/**
 * @file
 * XMLPipe generator invoked by Sphinx indexer.
 */

/**
 * Entry point for XMLPipe generator.
 *
 * This function is invoked from sphinxsearch_scripts/sphinxsearch_xmlpipe.php
 *
 * Note this process may eat a lot of resources, and it will be executed by
 * anonymous user, so access control is based on IP.
 */
function sphinxsearch_xmlpipe($caller_version = 0) {
  // Check caller version.
  $sphinxsearch_xmlpipe_generator_version = 2;
  if ((int)$caller_version != $sphinxsearch_xmlpipe_generator_version) {
    $message = t('Oops! It looks like version of your sphinxsearch_xmlpipe.php script does not match the version expected by the sphinxsearch module. Maybe someone forgot to upgrade the contents of the sphinxsearch_scripts subdirectory when updating the module.');
    watchdog('sphinxsearch', $message, NULL, WATCHDOG_ERROR);
    print $message ."\n";
    exit;
  }

  // Check access to XMLPipe process by IP.
  $access_xmlpipe = FALSE;
  $sphinxsearch_indexer_ips = array_map('trim', explode(',', variable_get('sphinxsearch_indexer_ips', '')));
  if (!empty($sphinxsearch_indexer_ips)) {
    foreach ($sphinxsearch_indexer_ips as $cidr) {
      if (sphinxsearch_ip_check_cidr(ip_address(), $cidr)) {
        $access_xmlpipe = TRUE;
        break;
      }
    }
  }
  if (!$access_xmlpipe) {
    $message = t('Not authorized.');
    watchdog('sphinxsearch', $message, NULL, WATCHDOG_ERROR);
    print $message ."\n";
    exit;
  }

  // Obtain current mode of operation.
  $mode = (isset($_GET['mode']) ? trim($_GET['mode']) : '');

  // Prepare global data for XMLPipe generation.
  $GLOBALS['sphinxsearch_start_execution_time'] = time();
  $GLOBALS['sphinxsearch_max_execution_time'] = (int)ini_get('max_execution_time');
  $GLOBALS['sphinxsearch_memory_limit'] = sphinxsearch_get_memory_limit();
  $GLOBALS['sphinxsearch_initial_memory_usage'] = memory_get_usage();
  $GLOBALS['sphinxsearch_taxonomy_module_exists'] = module_exists('taxonomy');

  if ($GLOBALS['sphinxsearch_taxonomy_module_exists']) {
    require_once(drupal_get_path('module', 'sphinxsearch') .'/sphinxsearch.taxonomy.inc');
  }

  // Check PHP settings for resource consumption control.
  if ($GLOBALS['sphinxsearch_memory_limit'] < 0) {
    $message = t('Unable to obtain PHP memory limit. Please, make sure your PHP installation is compiled with --enable-memory-limit option.');
    watchdog('sphinxsearch', $message, NULL, WATCHDOG_ERROR);
    print $message ."\n";
    exit;
  }
  if ($GLOBALS['sphinxsearch_max_execution_time'] < 30) {
    $message = t('Maximum PHP execution time is set to @max_execution_time seconds, but this window is too short for this process.', array(
      '@max_execution_time' => $GLOBALS['sphinxsearch_max_execution_time']
    ));
    watchdog('sphinxsearch', $message, NULL, WATCHDOG_ERROR);
    print $message ."\n";
    exit;
  }

  // Make sure no output buffering is being used.
  if (ob_get_level()) {
    ob_end_clean();
  }

  if ($mode == 'main') {
    // Obtain the main index identifier.
    $main_index_id = (isset($_GET['id']) ? max(0, (int)$_GET['id']) : 0);
    // Obtain range of nodes to be included in this main index.
    $first_nid = (isset($_GET['first_nid']) ? max(0, (int)$_GET['first_nid']) : 0);
    $last_nid = (isset($_GET['last_nid']) ? max(0, (int)$_GET['last_nid']) : -1);
    if ($last_nid < 0) {
      $sql_node_types_condition = sphinxsearch_get_enabled_node_types_condition();
      if (!empty($sql_node_types_condition)) {
        $sql_node_types_condition = ' AND '. $sql_node_types_condition;
      }
      $last_nid = (int)db_result(db_query_range('SELECT nid FROM {node} WHERE status = 1'. $sql_node_types_condition .' ORDER BY nid DESC', 0, 1));
      if ($last_nid <= 0) {
        $message = t('Could not obtain last nid.');
        watchdog('sphinxsearch', $message, NULL, WATCHDOG_ERROR);
        print $message ."\n";
        exit;
      }
    }
    sphinxsearch_generate_xmlpipe_main($main_index_id, $first_nid, $last_nid);
  }
  else if ($mode == 'delta') {
    sphinxsearch_generate_xmlpipe_delta();
  }
  else {
    $message = t('Invalid argument.');
    watchdog('sphinxsearch', $message, NULL, WATCHDOG_ERROR);
    print $message ."\n";
    exit;
  }
}

/**
 * Generate a XMLPipe stream to build a main index for specified range.
 *
 * Main index processing will terminate when one of the following conditions is met:
 * a) All nodes specified by range have been processed.
 * b) There are less than 30 seconds to reach maximum PHP execution time (max_execution_time).
 * c) Memory used is more than 90% of available PHP memory (memory_limit).
 *
 * @param int $main_index_id
 *   Main index identifier. Main indexes should be numbered from 0 to n.
 * @param int $first_nid
 *   First node identifier to be included in this main index (inclusive).
 * @param int $last_nid
 *   Last node identifier to be included in this main index (inclusive).
 */
function sphinxsearch_generate_xmlpipe_main($main_index_id, $first_nid, $last_nid) {
  $range_start = $first_nid;
  $range_step = (int)variable_get('sphinxsearch_nodes_per_chunk', 0);
  $chunks_before_restart = variable_get('sphinxsearch_chunks_before_restart', 0);
  $chunks_counter = 0;
  $nodes_counter = 0;

  if ($range_step <= 0) {
    $range_step = ($last_nid - $first_nid) + 1;
    $chunks_before_restart = 0;
  }

  watchdog('sphinxsearch', t('XMLPipe processing for main index @main_index_id has started.<ul>
      <li>Nodes range: @first_nid-@last_nid.</li>
      <li>PHP max_execution_time: @max_execution_time seconds.</li>
      <li>PHP memory_limit: @memory_limit_bytes bytes (@memory_limit_kb KB).</li>
      <li>Initial memory usage: @initial_memory_bytes bytes (@initial_memory_kb KB).</li>
    </ul>', array(
    '@main_index_id' => $main_index_id,
    '@first_nid' => $first_nid,
    '@last_nid' => $last_nid,
    '@max_execution_time' => $GLOBALS['sphinxsearch_max_execution_time'],
    '@memory_limit_bytes' => $GLOBALS['sphinxsearch_memory_limit'],
    '@memory_limit_kb' => round($GLOBALS['sphinxsearch_memory_limit'] / 1024, 2),
    '@initial_memory_bytes' => $GLOBALS['sphinxsearch_initial_memory_usage'],
    '@initial_memory_kb' => round($GLOBALS['sphinxsearch_initial_memory_usage'] / 1024, 2),
  )), NULL, WATCHDOG_INFO);

  // Generate XMLPipe header.
  print sphinxsearch_xmlpipe_header();

  while (TRUE) {
    // Abort processing if current memory usage is more than 90%.
    $current_memory_bytes = memory_get_usage();
    if (intval($current_memory_bytes * 100 / $GLOBALS['sphinxsearch_memory_limit']) > 90) {
      $message = t('Short on resources. Current memory usage is higher than 90% of PHP memory_limit.<ul>
          <li>PHP memory_limit: @memory_limit_bytes bytes (@memory_limit_kb KB).</li>
          <li>Initial memory usage: @initial_memory_bytes bytes (@initial_memory_kb KB).</li>
          <li>Current memory usage: @current_memory_bytes bytes (@current_memory_kb KB).</li>
        </ul>', array(
        '@memory_limit_bytes' => $GLOBALS['sphinxsearch_memory_limit'],
        '@memory_limit_kb' => round($GLOBALS['sphinxsearch_memory_limit'] / 1024, 2),
        '@initial_memory_bytes' => $GLOBALS['sphinxsearch_initial_memory_usage'],
        '@initial_memory_kb' => round($GLOBALS['sphinxsearch_initial_memory_usage'] / 1024, 2),
        '@current_memory_bytes' => $current_memory_bytes,
        '@current_memory_kb' => round($current_memory_bytes / 1024, 2),
      ));
      _sphinxsearch_wrapper_watchdog('sphinxsearch', $message, NULL, WATCHDOG_ERROR);
      print strip_tags($message) ."\n";
      exit;
    }
    // Let's supose we need less than 30 seconds to process one single chunk of nodes.
    // Abort processing if current execution time is about to be higher than max.
    $current_execution_time = time() - $GLOBALS['sphinxsearch_start_execution_time'];
    if (($GLOBALS['sphinxsearch_max_execution_time'] - $current_execution_time) < 30) {
      $message = t('Short on resources. Current execution time is about to exceed PHP max_execution_time.<ul>
          <li>PHP max_execution_time: @max_execution_time seconds.</li>
          <li>Currrent execution time: @current_execution_time seconds.</li>
        </ul>', array(
        '@max_execution_time' => $GLOBALS['sphinxsearch_max_execution_time'],
        '@current_execution_time' => $current_execution_time,
      ));
      _sphinxsearch_wrapper_watchdog('sphinxsearch', $message, NULL, WATCHDOG_ERROR);
      print strip_tags($message) ."\n";
      exit;
    }

    // Load the nids we are about to process within current loop.
    $range_end = min($range_start + $range_step, $last_nid);
    $nids = array();
    $result = db_query('SELECT nid FROM {node}
      WHERE '. implode(' AND ', array_filter(array('status = 1', sphinxsearch_get_enabled_node_types_condition()))) .'
        AND nid >= %d AND nid <= %d
      ORDER BY nid ASC', array($range_start, $range_end));
    while ($row = db_fetch_object($result)) {
      $nids[] = $row->nid;
    }

    // Process nodes for this loop.
    foreach ($nids as $nid) {
      if ($nid > $last_nid) {
        break;
      }
      $nodes_counter++;
      $xmlpipe_document = sphinxsearch_xmlpipe_document($main_index_id, $nid);
      if ($xmlpipe_document) {
        print $xmlpipe_document;
      }
    }
    unset($nids);

    // Are we done?
    $range_start = $range_end + 1;
    if ($range_start > $last_nid) {
      break;
    }

    // Need to restart database connection?
    if ($chunks_before_restart > 0) {
      $chunks_counter++;
      if ($chunks_counter >= $chunks_before_restart) {
        _sphinxsearch_db_reconnect();
        $current_memory_bytes = memory_get_usage();
        $current_execution_time = time() - $GLOBALS['sphinxsearch_start_execution_time'];
        $message = t('Database server connection has been restarted.<ul>
            <li>Initial memory usage: @initial_memory_bytes bytes (@initial_memory_kb KB).</li>
            <li>Current memory usage: @current_memory_bytes bytes (@current_memory_kb KB).</li>
            <li>Current execution time: @current_execution_time seconds.</li>
            <li>Nodes processed: @nodes_counter (@nodes_per_second nodes/sec).</li>
          </ul>', array(
          '@initial_memory_bytes' => $GLOBALS['sphinxsearch_initial_memory_usage'],
          '@initial_memory_kb' => round($GLOBALS['sphinxsearch_initial_memory_usage'] / 1024, 2),
          '@current_memory_bytes' => $current_memory_bytes,
          '@current_memory_kb' => round($current_memory_bytes / 1024, 2),
          '@current_execution_time' => $current_execution_time,
          '@nodes_counter' => $nodes_counter,
          '@nodes_per_second' => ($current_execution_time > 0 ? round($nodes_counter / $current_execution_time, 2) : $nodes_counter),
        ));
        _sphinxsearch_wrapper_watchdog('sphinxsearch', $message, NULL, WATCHDOG_NOTICE);
        $chunks_counter = 0;
      }
    }
  }

  // Generate XMLPipe footer.
  print sphinxsearch_xmlpipe_footer();

  // Store process statistics to watchdog.
  $current_memory_bytes = memory_get_usage();
  $total_execution_time = time() - $GLOBALS['sphinxsearch_start_execution_time'];
  $message = t('XMLPipe processing for main index @main_index_id has finished successfully.<ul>
      <li>Initial memory usage: @initial_memory_bytes bytes (@initial_memory_kb KB).</li>
      <li>Current memory usage: @current_memory_bytes bytes (@current_memory_kb KB).</li>
      <li>Total execution time: @total_execution_time seconds.</li>
      <li>Nodes processed: @nodes_counter (@nodes_per_second nodes/sec).</li>
    </ul>', array(
    '@main_index_id' => $main_index_id,
    '@initial_memory_bytes' => $GLOBALS['sphinxsearch_initial_memory_usage'],
    '@initial_memory_kb' => round($GLOBALS['sphinxsearch_initial_memory_usage'] / 1024, 2),
    '@current_memory_bytes' => $current_memory_bytes,
    '@current_memory_kb' => round($current_memory_bytes / 1024, 2),
    '@total_execution_time' => $total_execution_time,
    '@nodes_counter' => $nodes_counter,
    '@nodes_per_second' => ($total_execution_time > 0 ? round($nodes_counter / $total_execution_time, 2) : $nodes_counter),
  ));
  _sphinxsearch_wrapper_watchdog('sphinxsearch', $message, NULL, WATCHDOG_INFO);
  exit;
}

/**
 * Generate a XMLPipe stream to build a delta index.
 *
 * Delta index is built based on data stored on current main indexes.
 *
 * This process works as follows:
 * - First, Sphinx is queried using distributed index specified in module
 *   settings to obtain
 *   a) The list of main index identifiers behind this distributed index.
 *   b) For each main index, we get nid ranges and last updated time.
 * - Finally, for each main index found, one particular SQL query is built
 *   and executed to obtain the list of new or updated nodes within its
 *   own interval.
 *
 * This method reduces data dependencies between Drupal site database and
 * current number and contents of main indexes used for the site.
 */
function sphinxsearch_generate_xmlpipe_delta() {
  $sphinxsearch_query_index = variable_get('sphinxsearch_query_index', '');
  if (empty($sphinxsearch_query_index)) {
    $message = t('XMLPipe for delta index failed: Sphinx query index not specified. Please, check module settings.');
    watchdog('sphinxsearch', $message, NULL, WATCHDOG_ERROR);
    print $message ."\n";
    exit;
  }

  // Prepare process statistics.
  $watchdog_report_items = array();
  $nodes_counter = 0;

  // Setup Sphinx search client.
  $sphinxsearch = &sphinxsearch_get_client();
  $sphinxsearch->SetArrayResult(TRUE);

  // Obtain list of main index identifiers.
  // Note that index identifier used for documents stored on
  // delta index is ignored here.
  $sphinxsearch->SetLimits(0, 100);
  $sphinxsearch->SetFilter('main_index_id', array(SPHINXSEARCH_DELTA_INDEX_ID), TRUE);
  $sphinxsearch->SetGroupBy('main_index_id', SPH_GROUPBY_ATTR, 'main_index_id ASC');
  $query_result = $sphinxsearch->Query('', $sphinxsearch_query_index);
  $main_indexes_info = array();
  if ($query_result && is_array($query_result['matches'])) {
    foreach ($query_result['matches'] as $match) {
      if (isset($match['attrs']['main_index_id'])) {
        $main_index_id = (int)$match['attrs']['main_index_id'];
        if (!isset($main_indexes_info[$main_index_id])) {
          $main_indexes_info[$main_index_id] = array();
        }
      }
    }
  }
  if (empty($main_indexes_info)) {
    $message = t('XMLPipe for delta index failed: Could not obtain list of main indexes from Sphinx.');
    watchdog('sphinxsearch', $message, NULL, WATCHDOG_ERROR);
    print $message ."\n";
    exit;
  }

  // Obtain index boundaries currently stored on each main index.
  $query_keys = array(
    'last_updated' => 'last_updated',
    'first_nid' => 'nid',
    'last_nid' => 'nid',
  );
  foreach ($main_indexes_info as $main_index_id => $main_index_info) {
    // Reset query internals for current main index.
    $query_ids = array();
    $sphinxsearch->ResetFilters();
    $sphinxsearch->ResetGroupBy();
    $sphinxsearch->SetLimits(0, 1);
    $sphinxsearch->SetFilter('main_index_id', array($main_index_id));

    // Ask for last_updated document in current main index.
    $sphinxsearch->SetSortMode(SPH_SORT_EXTENDED, 'last_updated DESC');
    $query_ids['last_updated'] = $sphinxsearch->AddQuery('', $sphinxsearch_query_index);

    // Ask for first nid in current main index.
    $sphinxsearch->SetSortMode(SPH_SORT_EXTENDED, 'nid ASC');
    $query_ids['first_nid'] = $sphinxsearch->AddQuery('', $sphinxsearch_query_index);

    // Ask for last nid in current main index.
    $sphinxsearch->SetSortMode(SPH_SORT_EXTENDED, 'nid DESC');
    $query_ids['last_nid'] = $sphinxsearch->AddQuery('', $sphinxsearch_query_index);

    // Run queries and parse results.
    $run_results = $sphinxsearch->RunQueries();
    if (is_array($run_results)) {
      foreach ($query_ids as $query_key => $results_key) {
        if (is_array($run_results[$results_key])) {
          $results = $run_results[$results_key];
          if (isset($results['matches'])) {
            $tmpdoc = array_pop($results['matches']);
            if (is_array($tmpdoc['attrs'])) {
              $main_indexes_info[$main_index_id][$query_key] = $tmpdoc['attrs'];
            }
          }
        }
      }
    }
    foreach ($query_keys as $query_key => $field_key) {
      if (!is_array($main_indexes_info[$main_index_id][$query_key])) {
        $message = t('XMLPipe for delta index failed: Could not obtain @query_key data for main index @main_index_id.', array(
          '@query_key' => $query_key,
          '@main_index_id' => $main_index_id,
        ));
        watchdog('sphinxsearch', $message, NULL, WATCHDOG_ERROR);
        print $message ."\n";
        exit;
      }
      $main_indexes_info[$main_index_id][$query_key] = $main_indexes_info[$main_index_id][$query_key][$field_key];
    }
  }

  // Generate XMLPipe header.
  print sphinxsearch_xmlpipe_header();

  // Get new and/or updated documents for each main index.
  $main_indexes_count = count($main_indexes_info);
  $last_main_index_id = array_pop(array_keys($main_indexes_info));

  foreach ($main_indexes_info as $main_index_id => $main_index_info) {
    // Load the nids we are about to process for current main index interval.
    $nids = array();
    $query_sql = 'SELECT n.nid FROM {node} n LEFT JOIN {node_comment_statistics} c ON c.nid = n.nid
      WHERE '. implode(' AND ', array_filter(array('n.status = 1', sphinxsearch_get_enabled_node_types_condition('n')))) .'
        AND GREATEST(IF(c.last_comment_timestamp IS NULL, 0, c.last_comment_timestamp), n.changed) > %d
        AND n.nid >= %d';
    $query_args = array($main_index_info['last_updated'], $main_index_info['first_nid']);
    if ($main_index_id != $last_main_index_id) {
      $query_sql .= ' AND n.nid <= %d';
      $query_args[] = $main_index_info['last_nid'];
    }
    $query_sql .= ' ORDER BY n.nid ASC';
    $result = db_query($query_sql, $query_args);
    while ($row = db_fetch_object($result)) {
      $nids[] = $row->nid;
    }
    $nids_count = count($nids);
    $nodes_counter += $nids_count;

    // Process nodes for current main index.
    foreach ($nids as $nid) {
      $xmlpipe_document = sphinxsearch_xmlpipe_document(SPHINXSEARCH_DELTA_INDEX_ID, $nid);
      if ($xmlpipe_document) {
        print $xmlpipe_document;
      }
    }
    unset($nids);

    // Build statistics for this main index.
    $watchdog_report_items[] = '<li>'. t('Main index @main_index_id:<ul>
        <li>First nid: @first_nid.</li>
        <li>Last nid: @last_nid.</li>
        <li>Last updated time: @last_updated.</li>
        <li>Nodes processed: @nids_count.</li>
      </ul>', array(
      '@main_index_id' => $main_index_id,
      '@first_nid' => $main_index_info['first_nid'],
      '@last_nid' => $main_index_info['last_nid'],
      '@last_updated' => format_date($main_index_info['last_updated'], 'custom', 'Y-m-d H:s:i'),
      '@nids_count' => $nids_count,
    )) .'</li>';
  }

  // Generate XMLPipe footer.
  print sphinxsearch_xmlpipe_footer();

  // Store process statistics to watchdog.
  $current_memory_bytes = memory_get_usage();
  $total_execution_time = time() - $GLOBALS['sphinxsearch_start_execution_time'];
  $message = t('XMLPipe processing for delta index has finished successfully.<ul>
      <li>Initial memory usage: @initial_memory_bytes bytes (@initial_memory_kb KB).</li>
      <li>Current memory usage: @current_memory_bytes bytes (@current_memory_kb KB).</li>
      <li>Total execution time: @total_execution_time seconds.</li>
      <li>Nodes processed: @nodes_counter (@nodes_per_second nodes/sec).</li>
    </ul>', array(
    '@initial_memory_bytes' => $GLOBALS['sphinxsearch_initial_memory_usage'],
    '@initial_memory_kb' => round($GLOBALS['sphinxsearch_initial_memory_usage'] / 1024, 2),
    '@current_memory_bytes' => $current_memory_bytes,
    '@current_memory_kb' => round($current_memory_bytes / 1024, 2),
    '@total_execution_time' => $total_execution_time,
    '@nodes_counter' => $nodes_counter,
    '@nodes_per_second' => ($total_execution_time > 0 ? round($nodes_counter / $total_execution_time, 2) : $nodes_counter),
  ));
  $message .= t('Statistics related to main indexes processed for this delta:') .'<ul>'. implode('', $watchdog_report_items) .'</ul>';
  watchdog('sphinxsearch', $message, NULL, WATCHDOG_INFO);
  exit;
}

/**
 * Generate the XMLPipe header.
 */
function sphinxsearch_xmlpipe_header() {
  // Collect information about Sphinx document fields and attributes.
  $GLOBALS['sphinxsearch_xmlpipe_schema'] = array_merge(sphinxsearch_invoke_api('sphinx_schema'), array(
    // Flag to take care of deleted documents until main indexes are rebuilt.
    'is_deleted' => array('sphinx_type' => 'bool', 'sphinx_default' => 0),
    // Main Index ID is used for delta index processing.
    'main_index_id' => array('sphinx_type' => 'int'),
  ));

  // Build Sphinx documents schema.
  $output = '<'.'?xml version="1.0" encoding="utf-8"?'.'>'."\n";
  $output .= '<sphinx:docset>'."\n";
  $output .= '<sphinx:schema>'."\n";
  foreach ($GLOBALS['sphinxsearch_xmlpipe_schema'] as $name => $attributes) {
    if ($attributes['sphinx_type'] == 'multi') {
      $output .= '<sphinx:attr name="'. $name .'" type="multi"/>'."\n";
    }
    else if ($attributes['sphinx_type'] == 'int') {
      $default = (isset($attributes['sphinx_default']) ? (int)$attributes['sphinx_default'] : 0);
      $output .= '<sphinx:attr name="'. $name .'" type="int" bits="32" default="'. $default .'"/>'."\n";
    }
    else if ($attributes['sphinx_type'] == 'bool') {
      $default = (isset($attributes['sphinx_default']) ? (int)(bool)$attributes['sphinx_default'] : 0);
      $output .= '<sphinx:attr name="'. $name .'" type="bool" default="'. $default .'"/>'."\n";
    }
    else if ($attributes['sphinx_type'] == 'timestamp') {
      $output .= '<sphinx:attr name="'. $name .'" type="timestamp"/>'."\n";
    }
    else if ($attributes['sphinx_type'] == 'text') {
      $output .= '<sphinx:field name="'. $name .'"/>'."\n";
    }
    else if ($attributes['sphinx_type'] == 'float') {
      $default = (isset($attributes['sphinx_default']) ? (float)$attributes['sphinx_default'] : 0);
      $output .= '<sphinx:attr name="'. $name .'" type="float" default="'. $default .'"/>'."\n";
    }
  }
  $output .= '</sphinx:schema>'."\n";
  return $output;
}

/**
 * Generate the XMLPipe footer.
 */
function sphinxsearch_xmlpipe_footer() {
  $output = '</sphinx:docset>';
  return $output;
}

/**
 * Generate a single XMLPipe document.
 */
function sphinxsearch_xmlpipe_document($main_index_id, $nid) {
  // Load the node (reseting the node_load cache).
  if (!($node = node_load($nid, NULL, TRUE))) {
    return FALSE;
  }

  // Collect Sphinx document data.
  $document = array_merge(sphinxsearch_invoke_api('sphinx_document', $node), array(
    'main_index_id' => $main_index_id,
  ));

  // Build Sphinx document stream.
  $output = '<sphinx:document id="'. $nid .'">'."\n";
  foreach ($document as $name => $value) {
    if (isset($GLOBALS['sphinxsearch_xmlpipe_schema'][$name])) {
      if ($GLOBALS['sphinxsearch_xmlpipe_schema'][$name]['sphinx_type'] == 'text') {
        $value = '<![CDATA[['. check_plain($value) .']]>';
      }
      $output .= '<'. $name .'>'. $value .'</'. $name .'>'."\n";
    }
  }
  $output .= '</sphinx:document>'."\n";
  return $output;
}

/**
 * Restart database server connection.
 *
 * This function is necessary because Drupal does not have a method to restart
 * a database server connection.
 * Note the main problem is that db_set_active() uses static variables to store
 * the list of opened connections ($db_conns array). We can still access the
 * currently active connection resource ($active_db), but this method breaks
 * core capability to switch database server connections via db_set_active().
 * This is normally no problem while XMLPipe processing takes place. However,
 * when we invoke external hooks, some of them may rely on db_set_active() to
 * perform their job, and in that case, restarting database server connection
 * from here will break those hooks. sphinxsearch module users in that
 * situation won't be able to use this feature, which is fortunately something
 * totally optional that can be set from the module settings panel, and it is
 * disabled by default.
 * Ideally, db_set_active() would have to be patched to offer the possibility
 * to restart database server connections.
 *
 * @see db_set_active()
 */
function _sphinxsearch_db_reconnect() {
  global $db_url, $db_type, $active_db;
  static $connect_url;
  if (!isset($connect_url)) {
    if (is_array($db_url)) {
      $connect_url = $db_url['default'];
    }
    else {
      $connect_url = $db_url;
    }
  }
  switch ($db_type) {
    case 'mysql':
      mysql_close($active_db);
      break;
    case 'mysqli':
      mysqli_close($active_db);
      break;
    case 'pgsql':
      pg_close($active_db);
      break;
    default:
      return;
  }
  $active_db = db_connect($connect_url);
}

/**
 * Log a system message.
 *
 * Actually, dblog_watchdog() implementation makes use of db_set_active() which
 * breaks our database reconnection logic.
 *
 * @param $type
 *   The category to which this message belongs.
 * @param $message
 *   The message to store in the log. See t() for documentation
 *   on how $message and $variables interact. Keep $message
 *   translatable by not concatenating dynamic values into it!
 * @param $variables
 *   Array of variables to replace in the message on display or
 *   NULL if message is already translated or not possible to
 *   translate.
 * @param $severity
 *   The severity of the message, as per RFC 3164
 * @param $link
 *   A link to associate with the message.
 *
 * @see sphinxsearch_generate_xmlpipe_main()
 * @see _sphinxsearch_db_reconnect()
 * @see watchdog()
 * @see dblog_watchdog()
 * @see db_set_active()
 */
function _sphinxsearch_wrapper_watchdog($type, $message, $variables = array(), $severity = WATCHDOG_NOTICE, $link = NULL) {
  global $user, $base_root;

  // Prepare the fields to be logged
  $log_message = array(
    'type'        => $type,
    'message'     => $message,
    'variables'   => $variables,
    'severity'    => $severity,
    'link'        => $link,
    'user'        => $user,
    'request_uri' => $base_root . request_uri(),
    'referer'     => referer_uri(),
    'ip'          => ip_address(),
    'timestamp'   => time(),
  );

  // Call the logging hooks to log/process the message
  foreach (module_implements('watchdog', TRUE) as $module) {
    if ($module == 'dblog') {
      // This is exact same code than dblog_watchdog(), except we do not make
      // use of db_set_active(), we can't and it is not really necessary here.
      db_query("INSERT INTO {watchdog}
        (uid, type, message, variables, severity, link, location, referer, hostname, timestamp)
        VALUES
        (%d, '%s', '%s', '%s', %d, '%s', '%s', '%s', '%s', %d)",
        $log_message['user']->uid,
        $log_message['type'],
        $log_message['message'],
        serialize($log_message['variables']),
        $log_message['severity'],
        $log_message['link'],
        $log_message['request_uri'],
        $log_message['referer'],
        $log_message['ip'],
        $log_message['timestamp']
      );
    }
    else {
      module_invoke($module, 'watchdog', $log_message);
    }
  }
}

/**
 * Check if IP address belongs to specified CIDR range.
 * Note: IPv6 addresses are not supported.
 *
 * @param string $ip
 *   IPv4 address. ie. 192.168.0.1
 * @param string $cidr
 *   CIDR mask. ie. 192.168.0.0/24
 * @return boolean
 *   TRUE if $ip matches specified CIDR mask, FALSE otherwise.
 */
function sphinxsearch_ip_check_cidr($ip, $cidr) {
  list($net, $mask) = explode('/', $cidr);
  $ip_net = ip2long($net);
  $ip_mask = ~((1 << (32 - $mask)) - 1);
  $ip_ip = ip2long($ip);
  $ip_ip_net = $ip_ip & $ip_mask;
  return ($ip_ip_net == $ip_net);
}
