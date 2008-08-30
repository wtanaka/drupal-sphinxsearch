<?php
// $Id: sphinxsearch.xmlpipe.inc,v 1.3 2008/08/30 06:53:31 markuspetrux Exp $

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
    watchdog('sphinxsearch', $message, WATCHDOG_ERROR);
    print $message ."\n";
    exit;
  }

  // Check access to XMLPipe process by IP.
  $access_xmlpipe = FALSE;
  $sphinxsearch_indexer_ips = array_map('trim', explode(',', variable_get('sphinxsearch_indexer_ips', '')));
  if (!empty($sphinxsearch_indexer_ips)) {
    foreach ($sphinxsearch_indexer_ips as $cidr) {
      if (sphinxsearch_ip_check_cidr($_SERVER['REMOTE_ADDR'], $cidr)) {
        $access_xmlpipe = TRUE;
        break;
      }
    }
  }
  if (!$access_xmlpipe) {
    $message = t('Not authorized.');
    watchdog('sphinxsearch', $message, WATCHDOG_ERROR);
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
    watchdog('sphinxsearch', $message, WATCHDOG_ERROR);
    print $message ."\n";
    exit;
  }
  if ($GLOBALS['sphinxsearch_max_execution_time'] < 30) {
    $message = t('Maximum PHP execution time is set to @max_execution_time seconds, but this window is too short for this process.', array(
      '@max_execution_time' => $GLOBALS['sphinxsearch_max_execution_time']
    ));
    watchdog('sphinxsearch', $message, WATCHDOG_ERROR);
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
      $last_nid = (int)db_result(db_query('SELECT nid FROM {node} WHERE status = 1'. $sql_node_types_condition .' ORDER BY nid DESC LIMIT 1'));
      if ($last_nid <= 0) {
        $message = t('Could not obtain last nid.');
        watchdog('sphinxsearch', $message, WATCHDOG_ERROR);
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
    watchdog('sphinxsearch', $message, WATCHDOG_ERROR);
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

  sphinxsearch_watchdog('sphinxsearch', t('XMLPipe processing for main index @main_index_id has started.<ul>
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
  )), WATCHDOG_NOTICE);

  // Generate XMLPipe header.
  print sphinxsearch_xmlpipe_header();

  $sql_common_conditions = sphinxsearch_get_enabled_node_types_condition();
  if (!empty($sql_common_conditions)) {
    $sql_common_conditions = 'status = 1 AND '. $sql_common_conditions;
  }
  else {
    $sql_common_conditions = 'status = 1';
  }

  while ( TRUE )
  {
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
      sphinxsearch_watchdog('sphinxsearch', $message, WATCHDOG_ERROR);
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
      sphinxsearch_watchdog('sphinxsearch', $message, WATCHDOG_ERROR);
      print strip_tags($message) ."\n";
      exit;
    }

    // Load the nids we are about to process within current loop.
    $range_end = min($range_start + $range_step, $last_nid);
    $nids = array();
    $result = db_query('SELECT nid FROM {node}
      WHERE '. $sql_common_conditions .' AND nid >= %d AND nid <= %d
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

    // Need to restart DB connection?
    if ($chunks_before_restart > 0) {
      $chunks_counter++;
      if ($chunks_counter >= $chunks_before_restart) {
        sphinxsearch_db_reconnect();
        $current_memory_bytes = memory_get_usage();
        $current_execution_time = time() - $GLOBALS['sphinxsearch_start_execution_time'];
        sphinxsearch_watchdog('sphinxsearch', t('Database server connection has been restarted.<ul>
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
        )), WATCHDOG_ERROR);
        $chunks_counter = 0;
      }
    }
  }

  // Generate XMLPipe footer.
  print sphinxsearch_xmlpipe_footer();

  // Store process statistics to watchdog.
  $current_memory_bytes = memory_get_usage();
  $total_execution_time = time() - $GLOBALS['sphinxsearch_start_execution_time'];
  sphinxsearch_watchdog('sphinxsearch', t('XMLPipe processing for main index @main_index_id has finished successfully.<ul>
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
  )), WATCHDOG_NOTICE);
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
    watchdog('sphinxsearch', $message, WATCHDOG_ERROR);
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
    watchdog('sphinxsearch', $message, WATCHDOG_ERROR);
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
        watchdog('sphinxsearch', $message, WATCHDOG_ERROR);
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

  $sql_common_conditions = sphinxsearch_get_enabled_node_types_condition('n');
  if (!empty($sql_common_conditions)) {
    $sql_common_conditions = 'n.status = 1 AND '. $sql_common_conditions;
  }
  else {
    $sql_common_conditions = 'n.status = 1';
  }

  foreach ($main_indexes_info as $main_index_id => $main_index_info) {
    // Load the nids we are about to process for current main index interval.
    $nids = array();
    $query_sql = 'SELECT n.nid FROM {node} n LEFT JOIN {node_comment_statistics} c ON c.nid = n.nid
      WHERE '. $sql_common_conditions .'
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
  $watchdog_report = t('XMLPipe processing for delta index has finished successfully.<ul>
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
  $watchdog_report .= t('Statistics related to main indexes processed for this delta:') .'<ul>'. implode('', $watchdog_report_items) .'</ul>';
  sphinxsearch_watchdog('sphinxsearch', $watchdog_report, WATCHDOG_NOTICE);
  exit;
}

/**
 * Generate the XMLPipe header.
 */
function sphinxsearch_xmlpipe_header() {
  // Start XMLPipe stream.
  $output = '<'.'?xml version="1.0" encoding="utf-8"?'.'>' ."\n";
  $output .= '<sphinx:docset>' ."\n";

  // Build document schema.
  $output .= '<sphinx:schema>' ."\n";

  // Text fields.
  $output .= '<sphinx:field name="subject"/>' ."\n";
  $output .= '<sphinx:field name="content"/>' ."\n";

  // Node related attributes.
  $output .= '<sphinx:attr name="nid" type="int" bits="32" default="0"/>' ."\n";
  $output .= '<sphinx:attr name="uid" type="int" bits="32" default="0"/>' ."\n";
  $output .= '<sphinx:attr name="nodetype" type="int" bits="32" default="0"/>' ."\n";
  $output .= '<sphinx:attr name="created" type="timestamp"/>' ."\n";
  $output .= '<sphinx:attr name="last_updated" type="timestamp"/>' ."\n";

  // Taxonomy related attributes.
  if ($GLOBALS['sphinxsearch_taxonomy_module_exists']) {
    foreach (sphinxsearch_get_enabled_vocabularies() as $vid => $void) {
      $output .= '<sphinx:attr name="terms'. $vid .'" type="multi"/>' ."\n";
    }
    $output .= '<sphinx:attr name="vid" type="multi"/>' ."\n";
  }

  // Internal attributes.
  $output .= '<sphinx:attr name="is_deleted" type="bool" default="0"/>' ."\n";
  $output .= '<sphinx:attr name="main_index_id" type="int" bits="32" default="0"/>' ."\n";
  $output .= '</sphinx:schema>' ."\n";
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
  static $vocabularies;
  if (!isset($vocabularies)) {
    $vocabularies = sphinxsearch_get_enabled_vocabularies();
  }

  $node = node_load($nid, NULL, TRUE);
  $output = '';
  if (!$node) {
    return FALSE;
  }

  // Obtain text representation of the node.
  $text = sphinxsearch_get_node_text($node);

  // Generate the XML for this document.
  $output .= '<sphinx:document id="'. $nid .'">' ."\n";

  // Text fields.
  $output .= '<subject><![CDATA[['. check_plain($node->title) .']]></subject>' ."\n";
  $output .= '<content><![CDATA[['. check_plain($text) .']]></content>' ."\n";

  // Node related attributes.
  $output .= '<nid>'. $nid .'</nid>' ."\n";
  $output .= '<uid>'. $node->uid .'</uid>' ."\n";
  $output .= '<nodetype>'. sphinxsearch_xmlpipe_nodetype('id', $node->type) .'</nodetype>' ."\n";
  $output .= '<created>'. $node->created .'</created>' ."\n";

  // Taxonomy related attributes.
  if ($GLOBALS['sphinxsearch_taxonomy_module_exists']) {
    $vids = array();
    foreach ($node->taxonomy as $tid => $term) {
      if (empty($vocabularies) || isset($vocabularies[$term->vid])) {
        if (!isset($vids[$term->vid])) {
          $vids[$term->vid] = array();
        }
        $vids[$term->vid][] = $tid;
      }
    }
    if (!empty($vids)) {
      foreach ($vids as $vid => $tids) {
        $output .= '<terms'. $vid .'>'. implode(' ', $tids) .'</terms'. $vid .'>' ."\n";
      }
      $output .= '<vid>'. implode(' ', array_keys($vids)) .'</vid>' ."\n";
    }
  }

  // Internal attributes.
  if (!empty($node->last_comment_timestamp) && $node->last_comment_timestamp > $node->changed) {
    $output .= '<last_updated>'. $node->last_comment_timestamp .'</last_updated>' ."\n";
  }
  else {
    $output .= '<last_updated>'. $node->changed .'</last_updated>' ."\n";
  }
  $output .= '<main_index_id>'. $main_index_id .'</main_index_id>' ."\n";

  $output .= '</sphinx:document>' ."\n";
  return $output;
}

/**
 * Restart DB server connection.
 */
function sphinxsearch_db_reconnect() {
  global $db_url, $db_type, $active_db;
  static $connect_url;
  if (!isset($connect_url)) {
    if ($db_type != 'mysql') {
      return;
    }
    if (is_array($db_url)) {
      $connect_url = $db_url['default'];
    }
    else {
      $connect_url = $db_url;
    }
  }
  mysql_close($active_db);
  $active_db = db_connect($connect_url);
}

/**
 * Log a system message.
 *
 * Note: Drupal watchdog() is broken by our sphinxsearch_db_reconnect().
 *
 * @param $type
 *   The category to which this message belongs.
 * @param $message
 *   The message to store in the log.
 * @param $severity
 *   The severity of the message. One of the following values:
 *   - WATCHDOG_NOTICE
 *   - WATCHDOG_WARNING
 *   - WATCHDOG_ERROR
 * @param $link
 *   A link to associate with the message.
 */
function sphinxsearch_watchdog($type, $message, $severity = WATCHDOG_NOTICE, $link = NULL) {
  global $user, $base_root;

  // Note: log the exact, entire absolute URL.
  $request_uri = $base_root . request_uri();

  db_query("INSERT INTO {watchdog} (uid, type, message, severity, link, location, referer, hostname, timestamp) VALUES (%d, '%s', '%s', %d, '%s', '%s', '%s', '%s', %d)", $user->uid, $type, $message, $severity, $link, $request_uri, referer_uri(), $_SERVER['REMOTE_ADDR'], time());
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
