<?php
// $Id: sphinxsearch.pages.inc,v 1.4.2.1 2008/09/12 03:36:10 markuspetrux Exp $

/**
 * @file
 * Implementation of pages used to search using Sphinx search module.
 */

/**
 * Menu callback; presents the search form and/or search results.
 *
 * Search form submits with POST but redirects to GET. This way we can keep
 * the search query URL clean as a whistle:
 * http://www.example.com/search-path?keys=... (including keys and options)
 * @see sphinxsearch_get_search_path()
 * @see sphinxsearch_get_query_string()
 */
function sphinxsearch_search_page() {
  global $user;

  if (isset($_POST['form_id'])) {
    // Let POST method redirect to GET.
    return drupal_get_form('sphinxsearch_search_form');
  }

  // Parse request and build search options structure.
  $search_options = sphinxsearch_parse_request($_GET);

  // If any error was found in request, display them and render expanded search form.
  if (!empty($search_options['errors'])) {
    foreach ($search_options['errors'] as $field_name => $message) {
      form_set_error($field_name, $message);
    }
    return drupal_get_form('sphinxsearch_search_form', $search_options, FALSE);
  }

  // If no filters have specified, just render a clean collapsed search form.
  // If form was submitted, user has already been warned from submit handler.
  if (empty($search_options['filters'])) {
    return drupal_get_form('sphinxsearch_search_form', $search_options);
  }

  // Flood control.
  $admin_access = user_access('administer sphinxsearch');
  if (!$admin_access) {
    $rid = ($user->uid ? DRUPAL_AUTHENTICATED_RID : DRUPAL_ANONYMOUS_RID);
    $threshold = (int)variable_get('sphinxsearch_search_flood_per_role_'. $rid, 0);
    if ($threshold > 0 && !flood_is_allowed('sphinxsearch:search', $threshold)) {
      flood_register_event('sphinxsearch:search');
      // This header is specially aimed to stop bad bots.
      header('HTTP/1.0 403 Forbidden');
      // Enable flood limit flag to not generate tagadelic/faceted search blocks.
      sphinxsearch_flood_limit_exceeded(TRUE);
      return t('You cannot perform more than @number search queries per hour. Please try again later.', array('@number' => $threshold));
    }
  }

  // Execute search query and collect the results.
  $search_results = sphinxsearch_execute_query($search_options);

  // If any, display warnings if user is administrator,
  // or store them to watchdog for later review.
  if (!empty($search_results['warnings'])) {
    sphinxsearch_watchdog_warning(implode('<br />', $search_results['warnings']));
  }

  // Deal with search results.
  if ($search_results['total_available'] <= 0) {
    if (!empty($search_results['error_message'])) {
      drupal_set_message($search_results['error_message'], 'error');
    }
    // Display no results warning and render expanded search form.
    $output = theme('box', t('Your search yielded no results'), sphinxsearch_help('sphinxsearch#noresults'));
    $output .= drupal_get_form('sphinxsearch_search_form', $search_options, FALSE);
  }
  else {
    // Render collapsed search form and search results.
    $output = drupal_get_form('sphinxsearch_search_form', $search_options);
    $output .= theme('box', t('Search results'), theme('sphinxsearch_search_results', $search_options, $search_results));

    // Register flood event for non-admins.
    if (!$admin_access) {
      flood_register_event('sphinxsearch:search');
    }

    // Log the search request?
    $log_search_keys = variable_get('sphinxsearch_log_search_keys', 'always');
    if ($log_search_keys == 'always' || ($log_search_keys == 'keys-only' && !empty($search_options['filters']['keys']))) {
      $query_string = sphinxsearch_get_query_string($search_options);
      watchdog('search', t('%keys %query', array(
          '%keys' => (!empty($search_options['filters']['keys']) ? $search_options['filters']['keys'] : t('(no keywords)')),
          '%query' => (!empty($query_string) ? '?'. urldecode($query_string) : ''),
        )), NULL,
        WATCHDOG_NOTICE,
        l(t('results'), sphinxsearch_get_search_path(), array('query' => $query_string))
      );
    }
  }

  return $output;
}

/**
 * Record a warning to watchdog.
 *
 * If current user has 'administer sphinxsearch' privilege,
 * then the warning is just sent to the user interface.
 *
 * @param string $message
 */
function sphinxsearch_watchdog_warning($message) {
  if (user_access('administer sphinxsearch')) {
    drupal_set_message($message, 'error');
  }
  else {
    watchdog('sphinxsearch', $message, NULL, WATCHDOG_WARNING);
  }
}

/**
 * Render a search form.
 *
 * @param array $search_options
 *   The search string and options entered by the user.
 * @param boolean $advanced_options_collapsed
 *   TRUE to collapse Advanced search options fieldset.
 * @return array
 *   The search form.
 */
function sphinxsearch_search_form(&$form_state, $search_options = array(), $advanced_options_collapsed = TRUE) {
  $form = array(
    '#action' => url(sphinxsearch_get_search_path()),
    '#attributes' => array('class' => 'search-form'),
  );
  $form['basic'] = array('#type' => 'item', '#title' => t('Enter your keywords'));
  $form['basic']['inline'] = array('#prefix' => '<div class="container-inline">', '#suffix' => '</div>');
  $form['basic']['inline']['keys'] = array(
    '#type' => 'textfield',
    '#title' => '',
    '#default_value' => $search_options['filters']['keys'],
    '#size' => 50,
    '#maxlength' => 255,
  );
  $form['basic']['inline']['submit'] = array('#type' => 'submit', '#value' => t('Search'));

  $form['advanced'] = array(
    '#type' => 'fieldset',
    '#title' => t('Advanced search'),
    '#collapsible' => TRUE, '#collapsed' => $advanced_options_collapsed,
    '#attributes' => array('class' => 'search-advanced'),
  );

  // Matching modes.
  $form['advanced']['matchmode'] = array(
    '#type' => 'radios',
    '#title' => t('Matching mode'),
    '#prefix' => '<div class="criterion">',
    '#suffix' => '</div>',
    '#options' => sphinxsearch_get_matching_modes(),
    '#default_value' => (isset($search_options['matchmode']) ? $search_options['matchmode'] : SPHINXSEARCH_MATCH_ALL),
  );

  // Filter by content author.
  $form['advanced']['author'] = array(
    '#type' => 'textfield',
    '#title' => t('Author'),
    '#prefix' => '<div class="criterion">',
    '#suffix' => '</div>',
    '#maxlength' => 60,
    '#autocomplete_path' => 'user/autocomplete',
    '#default_value' => (isset($search_options['filters']['author']['name']) ? $search_options['filters']['author']['name'] : ''),
    '#description' => t('Use this option to filter search results by content author.'),
  );

  // Filter by content type (only if more than one content type exists).
  $system_node_types = array_map('check_plain', node_get_types('names'));
  $search_node_types = array();
  foreach (sphinxsearch_get_enabled_node_types() as $type) {
    $search_node_types[$type] = $system_node_types[$type];
  }
  if (count($search_node_types) > 1) {
    $form['advanced']['types'] = array(
      '#type' => 'checkboxes',
      '#title' => t('Only of the type(s)'),
      '#prefix' => '<div class="criterion">',
      '#suffix' => '</div>',
      '#options' => $search_node_types,
      '#default_value' => (isset($search_options['filters']['types']) ? $search_options['filters']['types'] : array()),
    );
  }

  // Filter by taxonomy (only if taxonomy module is enabled).
  if (module_exists('taxonomy')) {
    $vocabularies = sphinxsearch_get_enabled_vocabularies();
    // Build filtering options for enabled vocabularies.
    if (!empty($vocabularies)) {
      $form['advanced']['taxonomy'] = array();
      foreach ($vocabularies as $vid => $vocabulary) {
        $terms_key = 'terms'. $vid;
        $search_terms = (!empty($search_options['filters']['taxonomy'][$vid]) ? $search_options['filters']['taxonomy'][$vid] : array());
        if ($vocabulary->tags) {
          // Note: for some reason, titles for freetagging vocabularies are not
          // check_plain'd in taxonomy module. Let's be consistent with core.
          $form['advanced']['taxonomy']['tags'][$terms_key] = array(
            '#type' => 'textfield',
            '#title' => $vocabulary->name,
            '#prefix' => '<div class="criterion">',
            '#suffix' => '</div>',
            '#description' => t('If you wish to filter by more than one term, you can separate them with commas. Example: funny, bungee jumping, "Company, Inc.".'),
            '#default_value' => sphinxsearch_taxonomy_encode_typed_terms($search_terms),
            '#autocomplete_path' => 'taxonomy/autocomplete/'. $vid,
            '#weight' => $vocabulary->weight,
            '#maxlength' => 255,
          );
        }
        else {
          $tree = taxonomy_get_tree($vid);
          if ($tree) {
            $options = array();
            foreach ($tree as $term) {
              $choice = new stdClass();
              $choice->option = array($term->tid => str_repeat('-', $term->depth) . $term->name);
              $options[] = $choice;
            }
            if (!empty($options)) {
              $form['advanced']['taxonomy'][$terms_key] = array(
                '#type' => 'select',
                '#title' => check_plain($vocabulary->name),
                '#prefix' => '<div class="criterion">',
                '#suffix' => '</div>',
                '#description' => t('Select one or more items to filter search results by terms of this category.'),
                '#default_value' => array_keys($search_terms),
                '#options' => $options,
                '#multiple' => TRUE,
                '#size' => min(10, count($options)),
                '#weight' => $vocabulary->weight,
              );
            }
          }
        }
      }
      // Add fieldset only if form has more than 1 vocabulary element.
      if (count($form['advanced']['taxonomy']) > 1) {
        $form['advanced']['taxonomy'] += array(
          '#type' => 'fieldset',
          '#title' => t('Categories'),
          '#prefix' => '<div class="clear-block"></div>',
          '#suffix' => '<div class="clear-block"></div>',
          '#collapsible' => TRUE, '#collapsed' => empty($search_options['filters']['taxonomy']),
        );
      }
    }
  }

  // Sort options.
  $form['advanced']['sortfield'] = array(
    '#type' => 'radios',
    '#title' => t('Order by'),
    '#prefix' => '<div class="criterion">',
    '#suffix' => '</div>',
    '#options' => sphinxsearch_get_sortable_fields(),
    '#default_value' => (isset($search_options['sortfield']) ? $search_options['sortfield'] : '@weight'),
  );
  $form['advanced']['sortdir'] = array(
    '#type' => 'radios',
    '#title' => t('Sort direction'),
    '#prefix' => '<div class="criterion">',
    '#suffix' => '</div>',
    '#options' => array('ASC' => t('Ascending'), 'DESC' => t('Descending')),
    '#default_value' => (isset($search_options['sortdir']) ? $search_options['sortdir'] : 'DESC'),
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
 * Validate a search form submission.
 */
function sphinxsearch_search_form_validate($form, &$form_state) {
  $search_options = sphinxsearch_parse_request($form_state['values']);
  if (!empty($search_options['errors'])) {
    foreach ($search_options['errors'] as $field_name => $message) {
      form_set_error($field_name, $message);
    }
  }
}

/**
 * Process a search form submission.
 */
function sphinxsearch_search_form_submit($form, &$form_state) {
  $search_options = sphinxsearch_parse_request($form_state['values']);
  $query_string = sphinxsearch_get_query_string($search_options);
  if (empty($query_string)) {
    form_set_error('keys', t('Please enter some keywords and/or other search options.'));
  }
  // Transform POST into a GET request.
  sphinxsearch_goto_search($query_string);
}

/**
 * Format the results page for the given query results array.
 *
 * @param array $search_options
 *   Search options structure.
 * @param array $search_results
 *   Search results structure.
 *
 * @ingroup themeable
 */
function theme_sphinxsearch_search_results($search_options, $search_results) {
  // Display information about query execution.
  $output = '<p>';
  $output .= format_plural($search_results['total_found'], '1 document found', '@count documents found') .' '. t('in @seconds seconds.', array('@seconds' => round($search_results['time'], 3)));
  if (!empty($search_results['words'])) {
    $words = array();
    foreach ($search_results['words'] as $word => $word_data) {
      $words[] = '<em>'. check_plain($word) .'</em> ('. t('documents: @docs, hits: @hits', array('@docs' => (int)$word_data['docs'], '@hits' => (int)$word_data['hits'])) .')';
    }
    $output .= ' '. t('Terms found:') .' '. implode('; ', $words) .'.';
  }
  $output .= '</p>' ."\n";
  if ($search_results['total_found'] != $search_results['total_available']) {
    drupal_set_message(t('Warning: only the first @count matches are displayed. You may wish to use additional filters to reduce the number of results.', array('@count' => $search_results['total_available'])), 'error');
  }

  // Display list of formatted search results.
  $output .= '<dl class="search-results">' ."\n";
  foreach ($search_results['nodes'] as $item_id => $node) {
    $title = (isset($search_results['titles'][$item_id]) ? $search_results['titles'][$item_id] : check_plain($node->title));
    $output .= '<dt class="title">'. l($title, 'node/'. $node->nid, array('html' => TRUE)) .'</dt>' ."\n";
    $info = array(
      theme('username', $node),
      format_date($node->created, 'small'),
    );
    $extra = node_invoke_nodeapi($node, 'search result');
    if (!empty($extra) && is_array($extra)) {
      $info = array_merge($info, $extra);
    }
    $excerpt = (isset($search_results['excerpts'][$item_id]) ? '<p class="search-excerpt">'. $search_results['excerpts'][$item_id] .'</p>' ."\n" : '');
    if (module_exists('taxonomy') && is_array($node->taxonomy)) {
      $links = array();
      foreach ($node->taxonomy as $term) {
        $links[] = l($term->name, taxonomy_term_path($term));
      }
      if (!empty($links)) {
        $excerpt .= '<p class="search-links">'. t('Tags') .': '. implode(' - ', $links) .'</p>' ."\n";
      }
    }
    $output .= '<dd>'. $excerpt .'<p class="search-info">'. implode(' - ', $info) .'</p></dd>' ."\n";
  }
  $output .= '</dl>' ."\n";

  // Display pager.
  $output .= sphinxsearch_pager($search_results['total_available'], $search_options['results_per_page']);
  return $output;
}
