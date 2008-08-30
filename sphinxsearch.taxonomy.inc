<?php
// $Id: sphinxsearch.taxonomy.inc,v 1.2 2008/08/30 06:53:31 markuspetrux Exp $

/**
 * @file
 * Common taxonomy related functions for the Sphinx search module.
 */

/**
 * Obtain list of enabled vocabularies.
 *
 * Each vocabulary will be indexed on a separate multi-valued field.
 *
 * @return array
 *   list of enabled vocabularies.
 */
function sphinxsearch_get_enabled_vocabularies() {
  static $vocabularies;
  if (!isset($vocabularies)) {
    $vocabularies = array();
    if (module_exists('taxonomy')) {
      foreach(taxonomy_get_vocabularies() as $vocabulary) {
        if (variable_get('sphinxsearch_include_vocabulary_'. $vocabulary->vid, 0)) {
          $vocabularies[(int)$vocabulary->vid] = $vocabulary;
        }
      }
    }
  }
  return $vocabularies;
}

/**
 * Encode terms hash into a comma separated list.
 *
 * @param array $terms
 *   Hash of terms.
 * @param string $separator
 *   Terms separator. Defaults to ', '.
 * @return string
 *   Comma separated list of terms.
 */
function sphinxsearch_taxonomy_encode_typed_terms($terms, $separator = ', ') {
  $typed_terms = array();
  foreach ($terms as $tid => $term) {
    // Commas and quotes in terms are special cases, so encode 'em.
    if (strpos($term->name, ',') !== FALSE || strpos($term->name, '"') !== FALSE) {
      $term->name = '"'. str_replace('"', '""', $term->name) .'"';
    }
    $typed_terms[] = $term->name;
  }
  return implode($separator, $typed_terms);
}

/**
 * Decode a comma separated list of strings into hash of terms.
 * @see taxonomy_node_save()
 *
 * @param int $vid
 *   Vocabulary identifier related to terms.
 * @param string $typed_terms
 *   Comma separated list of terms.
 * @return array
 *   Hash of terms. Note that a string of not found typed terms is stored as tid -1.
 */
function sphinxsearch_taxonomy_decode_typed_terms($vid, $typed_terms) {
  $terms = array();
  $regexp = '%(?:^|,\ *)("(?>[^"]*)(?>""[^"]* )*"|(?: [^",]*))%x';
  preg_match_all($regexp, $typed_terms, $matches);
  $typed_terms = array_unique($matches[1]);
  foreach ($typed_terms as $typed_term) {
    $typed_term = trim(str_replace('""', '"', preg_replace('#^"(.*)"$#', '\1', $typed_term)));
    if (!empty($typed_term)) {
      $possibilities = taxonomy_get_term_by_name($typed_term);
      $term = NULL;
      foreach ($possibilities as $possibility) {
        if ($possibility->vid == $vid) {
          $term = $possibility;
        }
      }
      if ($term) {
        $terms[$term->tid] = $term;
      }
      else {
        // Store typed terms that we haven't found in a particular item with tid = -1,
        // so caller has the chance to notify the user about it.
        if (!isset($terms[-1])) {
          $terms[-1] = array();
        }
        $terms[-1][] = $typed_term;
      }
    }
  }
  if (isset($terms[-1])) {
    $terms[-1] = implode(', ', $terms[-1]);
  }
  return $terms;
}

/**
 * Get term data for the given tids.
 *
 * @param int $vid
 *   Vocabulary identifier related to terms.
 * @param array $tids
 *   List of tids.
 * @return array
 *   Hash of terms. Note that a string of not found tids is stored as tid -1.
 */
function sphinxsearch_taxonomy_get_terms($vid, $tids) {
  $terms = array();
  if (!empty($tids)) {
    $query_args = array_merge(array($vid), $tids);
    $query_sql = 'SELECT t.* FROM {term_data} t WHERE t.vid = %d AND t.tid ';
    if (count($tids) == 1) {
      $query_sql .= '= %d';
    }
    else {
      $placeholders = implode(',', array_fill(0, count($tids), '%d'));
      $query_sql .= 'IN ('. $placeholders .')';
    }
    $result = db_query(db_rewrite_sql($query_sql, 't', 'tid'), $query_args);
    while ($term = db_fetch_object($result)) {
      $terms[$term->tid] = $term;
    }
    $diff = array_diff($tids, array_keys($terms));
    if (!empty($diff)) {
      $terms[-1] = implode(', ', $diff);
    }
  }
  return $terms;
}

/**
 * Build tagadelic items for the given options.
 * Tagadelic block (weighted tags cloud).
 *
 * @param array $group_by_options
 *   array(
 *     'type' => Grouping type: 'author', 'taxonomy', 'types'
 *     'vid'  => Vocabulary ID when ('type' => 'taxonomy')
 *   )
 * @param int $count
 *   Number of tags to render.
 * @param int $levels
 *   Amount of tag-sizes.
 * @param string $sortmode
 *   Sort order options.
 * @param boolean $faceted_search
 *   Use TRUE to build tag links for faceted search (internal use only).
 *   @see sphinxsearch_faceted_block()
 * @param array $request_options
 *   Requested search options (optional).
 *
 * @return array
 *   Hash of tagadelic items.
 */
function sphinxsearch_tagadelic_build_data($group_by_options, $count = 20, $levels = 10, $sortmode = 'title,asc', $faceted_search = FALSE, $request_options = array()) {
  // Build search options structure.
  $search_options = sphinxsearch_parse_request($request_options);
  $search_filters = array();
  if ($group_by_options['type'] == 'author') {
    if (!empty($search_options['filters']['author'])) {
      $author = new stdClass();
      $author->uid = $search_options['filters']['author']['uid'];
      $author->name = $search_options['filters']['author']['name'];
      $search_filters[$author->uid] = $author;
    }
    $group_by_options['group_by_field'] = 'uid';
  }
  else if ($group_by_options['type'] == 'types') {
    if (!isset($search_options['filters']['types'])) {
      $search_options['filters']['types'] = array();
    }
    if (!empty($search_options['filters']['types'])) {
      foreach ($search_options['filters']['types'] as $type) {
        $node_type = new stdClass();
        $node_type->type = $type;
        $node_type->name = node_get_types('name', $type);
        $search_filters[$node_type->type] = $node_type;
      }
    }
    $group_by_options['group_by_field'] = 'nodetype';
  }
  else if ($group_by_options['type'] == 'taxonomy') {
    $vid = $group_by_options['vid'];
    if (!isset($search_options['filters']['taxonomy'])) {
      $search_options['filters']['taxonomy'] = array($vid => array());
    }
    else if (!isset($search_options['filters']['taxonomy'][$vid])) {
      $search_options['filters']['taxonomy'][$vid] = array();
    }
    $search_filters = $search_options['filters']['taxonomy'][$vid];
    $group_by_options['group_by_field'] = 'terms'. $vid;
  }
  else {
    return FALSE;
  }

  // Results count should include number of items specified in filters.
  $count += count($search_filters);

  // Execute a GroupBy query based on current search options.
  $search_results = sphinxsearch_execute_query(array_merge($search_options, array(
    'results_per_page' => $count,
    'group_by' => $group_by_options['group_by_field'],
  )));
  if ($search_results['total_available'] <= 0 && !$faceted_search) {
    return FALSE;
  }

  // Remove filtered items from results.
  $generic_groups = array();
  foreach ($search_results['groups'] as $tid => $group_info) {
    if (!isset($search_filters[$tid])) {
      $generic_groups[$tid] = $group_info;
    }
  }
  if (count($generic_groups) <= 1 && !$faceted_search) {
    return FALSE;
  }
  unset($search_results);

  // Compute filtered items list in user supplied order.
  $filtered_items = array();
  foreach ($search_filters as $tid => $item) {
    if (!isset($generic_groups[$tid])) {
      $filtered_items[$tid] = $item;
    }
  }

  // Transform search results into tagadelic items.
  $generic_terms = array();
  if (!empty($generic_groups)) {
    if ($group_by_options['type'] == 'author') {
      $uids = array_keys($generic_groups);
      $sql = 'SELECT uid, name FROM {users} WHERE uid ';
      if (count($uids) == 1) {
        $sql .= '= %d';
      }
      else {
        $placeholders = implode(',', array_fill(0, count($uids), '%d'));
        $sql .= 'IN ('. $placeholders .')';
      }
      $result = db_query($sql, $uids);
      while ($row = db_fetch_object($result)) {
        $generic_terms[$row->uid] = $row;
      }
    }
    else if ($group_by_options['type'] == 'types') {
      foreach (array_keys($generic_groups) as $type) {
        $node_type = new stdClass();
        $node_type->type = $type;
        $node_type->name = node_get_types('name', $type);
        $generic_terms[$node_type->type] = $node_type;
      }
    }
    else if ($group_by_options['type'] == 'taxonomy') {
      $generic_terms = sphinxsearch_taxonomy_get_terms($vid, array_keys($generic_groups));
    }
  }

  // Find minimum and maximum log-count. Algorithm based on tagadelic.module.
  $tags = array();
  $min = 1e9;
  $max = -1e9;
  foreach ($generic_groups as $tid => $group_info) {
    if (isset($generic_terms[$tid])) {
      $tag = $generic_terms[$tid];
      $tag->tagadelic_items = $group_info['count'];
      $tag->tagadelic_count = log($group_info['count']);
      $min = min($min, $tag->tagadelic_count);
      $max = max($max, $tag->tagadelic_count);
      $tags[$tid] = $tag;
    }
  }
  if (empty($tags) && !$faceted_search) {
    return FALSE;
  }
  unset($generic_groups, $generic_terms);

  // Note: we need to ensure the range is slightly too large to make sure even
  // the largest element is rounded down.
  $range = max(.01, $max - $min) * 1.0001;
  foreach ($tags as $tid => $tag) {
    $tags[$tid]->tagadelic_weight = 1 + floor($levels * ($tag->tagadelic_count - $min) / $range);
  }

  // Sort tags.
  list($sort_by, $sort_order) = explode(',', $sortmode);
  switch ($sort_by) {
    case 'title':
      usort($tags, create_function('$a,$b', 'return strnatcasecmp($a->name, $b->name);'));
      break;
    case 'weight':
      usort($tags, create_function('$a,$b', 'return $a->tagadelic_weight > $b->tagadelic_weight;'));
      break;
    case 'random':
      shuffle($tags);
      break;
  }
  if ($sort_order == 'desc') {
    $tags = array_reverse($tags, TRUE);
  }

  // Additional faceted search processing (internal use only).
  // @see sphinxsearch_faceted_block()
  if ($faceted_search) {
    // Return weighted tags, filtered items and search options.
    return array($tags, $filtered_items, $search_options);
  }

  return $tags;
}

/**
 * Menu callback; Tagadelic page (weighted tags cloud).
 *
 * @param string $vids
 */
function sphinxsearch_tagadelic_page($vids = NULL) {
  $vocabularies = sphinxsearch_get_enabled_vocabularies();
  if (!isset($vids)) {
    $vids = array_keys($vocabularies);
  }
  else {
    $vids = array_filter(array_map('intval', array_map('trim', explode(',', $vids))));
    $vids = array_intersect(array_keys($vocabularies), array_unique($vids));
  }
  $blocks = array();
  foreach ($vids as $vid) {
    $tags = sphinxsearch_tagadelic_build_data(array('type' => 'taxonomy', 'vid' => $vid),
      (int)variable_get('sphinxsearch_page_tagadelic_tags', 100),
      (int)variable_get('sphinxsearch_page_tagadelic_levels', 10),
      variable_get('sphinxsearch_page_tagadelic_sortmode', 'title,asc')
    );
    if (!empty($tags)) {
      $blocks[$vid] = array(
        'vocabulary' => $vocabularies[$vid],
        'content' => theme('sphinxsearch_tagadelic_block', $tags),
      );
    }
  }
  if (empty($blocks)) {
    drupal_not_found();
    return;
  }
  return theme('sphinxsearch_tagadelic_page', $blocks);
}

/**
 * Build faceted items for the given options.
 *
 * @param array $vocabularies
 *   Hash of vocabularies to include in the block.
 * @param int $count
 *   Number of tags to render per vocabulary.
 * @param int $levels
 *   Amount of tag-sizes.
 * @param string $sortmode
 *   Sort order options.
 * @param array $request_options
 *   Requested search options (optional).
 *
 * @return array
 *   List of facets.
 */
function sphinxsearch_faceted_build_data($count = 20, $levels = 10, $sortmode = 'title,asc', $request_options = NULL) {
  if (!isset($request_options)) {
    $request_options = $_GET;
  }

  $faceted_blocks = array();

  // Build facets by taxonomy.
  foreach (sphinxsearch_get_enabled_vocabularies() as $vid => $vocabulary) {
    list($tags, $filtered_items, $search_options) = sphinxsearch_tagadelic_build_data(array('type' => 'taxonomy', 'vid' => $vid), $count, $levels, $sortmode, TRUE, $request_options);
    $faceted_blocks[] = array(
      'type' => 'taxonomy', 'vid' => $vid,
      'title' => $vocabulary->name,
      'tags' => $tags,
      'filtered_items' => $filtered_items,
      'search_options' => $search_options,
    );
  }

  // Build facets by node type.
  list($tags, $filtered_items, $search_options) = sphinxsearch_tagadelic_build_data(array('type' => 'types'), $count, $levels, $sortmode, TRUE, $request_options);
  $faceted_blocks[] = array(
    'type' => 'types',
    'title' => t('Content type'),
    'tags' => $tags,
    'filtered_items' => $filtered_items,
    'search_options' => $search_options,
  );

  // Build facets by author.
  list($tags, $filtered_items, $search_options) = sphinxsearch_tagadelic_build_data(array('type' => 'author'), $count, $levels, $sortmode, TRUE, $request_options);
  $faceted_blocks[] = array(
    'type' => 'author',
    'title' => t('Author'),
    'tags' => $tags,
    'filtered_items' => $filtered_items,
    'search_options' => $search_options,
  );

  $facets = array();
  foreach ($faceted_blocks as $facet_data) {
    $facet = new stdClass();

    // Compute faceted search path/query string for each tag.
    if (!empty($facet_data['tags'])) {
      foreach ($facet_data['tags'] as $i => $tag) {
        $faceted_search_options = $facet_data['search_options'];
        if ($facet_data['type'] == 'author') {
          $faceted_search_options['filters']['author'] = array('uid' => $tag->uid, 'name' => $tag->name);
        }
        else if ($facet_data['type'] == 'types') {
          $faceted_search_options['filters']['types'][] = $tag->type;
        }
        else if ($facet_data['type'] == 'taxonomy') {
          if (!isset($faceted_search_options['filters']['taxonomy'][$facet_data['vid']][$tag->tid])) {
            $faceted_search_options['filters']['taxonomy'][$facet_data['vid']][$tag->tid] = $tag;
          }
        }
        $facet_data['tags'][$i]->faceted_path = sphinxsearch_get_search_path();
        $facet_data['tags'][$i]->faceted_query = sphinxsearch_get_query_string($faceted_search_options);
      }
      $facet->tags = $facet_data['tags'];
    }

    // Compute faceted search path/query string for each term and the facet itself.
    if (!empty($facet_data['filtered_items'])) {
      $reversed_terms = array_reverse($facet_data['filtered_items'], TRUE);
      $remaining_terms = $facet_data['filtered_items'];
      foreach ($reversed_terms as $tid => $term) {
        $faceted_search_options = $facet_data['search_options'];
        if ($facet_data['type'] == 'author') {
          unset($faceted_search_options['filters']['author']);
        }
        else if ($facet_data['type'] == 'types') {
          $faceted_search_options['filters']['types'] = array_keys($remaining_terms);
        }
        else if ($facet_data['type'] == 'taxonomy') {
          $faceted_search_options['filters']['taxonomy'][$facet_data['vid']] = $remaining_terms;
        }
        $facet_data['filtered_items'][$tid]->faceted_path = sphinxsearch_get_search_path();
        $facet_data['filtered_items'][$tid]->faceted_query = sphinxsearch_get_query_string($faceted_search_options);
        unset($remaining_terms[$tid]);
      }

      $faceted_search_options = $facet_data['search_options'];
      if ($facet_data['type'] == 'author') {
        unset($faceted_search_options['filters']['author']);
      }
      else if ($facet_data['type'] == 'types') {
        unset($faceted_search_options['filters']['types']);
      }
      else if ($facet_data['type'] == 'taxonomy') {
        unset($faceted_search_options['filters']['taxonomy'][$facet_data['vid']]);
      }
      $facet->path = sphinxsearch_get_search_path();
      $facet->query = sphinxsearch_get_query_string($faceted_search_options);
      $facet->terms = $facet_data['filtered_items'];
    }

    if (!empty($facet->tags) || !empty($facet->terms)) {
      $facet->type = $facet_data['type'];
      $facet->title = $facet_data['title'];
      $facets[] = $facet;
    }
  }
  return $facets;
}

/**
 * Render faceted search block for the given facets.
 *
 * @param array $facets
 *
 * @ingroup themeable
 */
function theme_sphinxsearch_faceted_block($facets) {
  $output = '';

  // Build superset section.
  $superset = array();
  foreach ($facets as $facet) {
    if (!empty($facet->terms)) {
      $links = array();
      $last_term = array_pop($facet->terms);
      foreach ($facet->terms as $tid => $term) {
        $url = url($term->faceted_path, $term->faceted_query);
        $link = '<span><a href="'. $url .'">'. check_plain($term->name) .'</a></span>';
        $links[] = $link;
      }
      $links[] = '<span class="faceted-superset-youarehere">'. check_plain($last_term->name) .'</span>';
      if (!empty($links)) {
        $url = url($facet->path, $facet->query);
        $link = '<span><a href="'. $url .'">'. t('All') .'</a></span>';
        $superset[] = '<div><strong>'. check_plain($facet->title) .':</strong> '. implode(' › ', array_merge(array($link), $links)) .'</div>';
      }
    }
  }
  if (!empty($superset)) {
    $output .= '<div class="faceted-superset"><h2>'. t('You are here') .':</h2><div class="faceted-superset-lists">'. theme('item_list', $superset) .'</div></div>';
  }

  // Build refine search by terms section.
  foreach ($facets as $facet) {
    if (!empty($facet->tags)) {
      $facet_tags = theme('sphinxsearch_tagadelic_block', $facet->tags);
      if (!empty($facet_tags)) {
        $output .= '<div class="faceted-vocabulary"><h2>'. t('Refine search by: @vocabulary', array('@vocabulary' => $facet->title)) .'</h2>'. $facet_tags .'</div>' ."\n";
      }
    }
  }

  if (empty($output)) {
    return '';
  }
  return '<div class="faceted-wrapper">'. $output .'</div>';
}

/**
 * Render tagadelic block for the given tags.
 *
 * @param array $tags
 *
 * @ingroup themeable
 */
function theme_sphinxsearch_tagadelic_block($tags) {
  $output = '';

  foreach ($tags as $tag) {
    // Note: we don't use l() because it may append class="active".
    $attributes = array('class' => 'tagadelic-level'. $tag->tagadelic_weight, 'rel' => (!empty($tag->faceted_path) ? 'nofollow' : 'tag'));
    if (!empty($tag->tagadelic_items)) {
      $attributes['title'] = $tag->name .' ('. $tag->tagadelic_items .')';
    }
    $url = (!empty($tag->faceted_path) ? url($tag->faceted_path, $tag->faceted_query) : url(taxonomy_term_path($tag)));
    $output .= '<a href="'. $url .'"'. drupal_attributes($attributes) .'>'. check_plain($tag->name) .'</a>' ."\n";
  }

  if (empty($output)) {
    return '';
  }
  return '<div class="tagadelic-wrapper">'. $output .'</div>';
}

/**
 * Render more link for tagadelic block.
 *
 * @param int $vid
 *
 * @ingroup themeable
 */
function theme_sphinxsearch_tagadelic_more($vid) {
  return '<div class="more-link">'. l(t('more'), 'tagadelic/'. $vid) .'</div>';
}

/**
 * Render tagadelic page for the given blocks.
 *
 * @param array $blocks
 *   Hash of blocks per vocabulary. Elements:
 *     - vocabulary: object
 *     - content: Tagadelic block content in HTML format.
 *
 * @ingroup themeable
 */
function theme_sphinxsearch_tagadelic_page($blocks) {
  $output = '';
  $blocks_count = count($blocks);
  foreach ($blocks as $vid => $block) {
    $output .= '<div class="tagadelic-page-block">';
    if ($blocks_count == 1) {
      drupal_set_title(t('Tags in @vocabulary', array('@vocabulary' => $block['vocabulary']->name)));
    }
    else {
      $output .= '<h2>'. check_plain($block['vocabulary']->name) .'</h2>';
    }
    $output .= $block['content'] .'</div>';
  }
  return $output;
}
