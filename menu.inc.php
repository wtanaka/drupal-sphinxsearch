<?php
function _real_sphinxsearch_menu() {
  $items = array();
  $items['admin/settings/sphinxsearch'] = array(
    'title' => 'Sphinx search',
    'page callback' => 'drupal_get_form',
    'page arguments' => array('sphinxsearch_settings'),
    'access arguments' => array('administer sphinxsearch'),
    'file' => 'sphinxsearch.admin.inc',
  );
  $items['admin/settings/sphinxsearch/settings'] = array(
    'title' => 'Settings',
    'description' => 'Administer Sphinx search module settings',
    'access arguments' => array('administer sphinxsearch'),
    'weight' => -10,
    'type' => MENU_DEFAULT_LOCAL_TASK,
  );
  $items['admin/settings/sphinxsearch/check-connection'] = array(
    'title' => 'Check connection',
    'description' => 'Check connection to Sphinx searchd daemon',
    'page callback' => 'sphinxsearch_check_connection_page',
    'access arguments' => array('administer sphinxsearch'),
    'weight' => 10,
    'type' => MENU_LOCAL_TASK,
    'file' => 'sphinxsearch.admin.inc',
  );
  $items[sphinxsearch_get_search_path()] = array(
    'title' => 'Search',
    'page callback' => 'sphinxsearch_search_page',
    'access arguments' => array('use sphinxsearch'),
    'type' => MENU_SUGGESTED_ITEM,
    'file' => 'sphinxsearch.pages.inc',
  );
  if (module_exists('taxonomy') && !module_exists('tagadelic')) {
    $items['tagadelic'] = array(
      'title' => 'Tags',
      'page callback' => 'sphinxsearch_tagadelic_page',
      'access arguments' => array('use sphinxsearch'),
      'type' => MENU_SUGGESTED_ITEM,
    );
  }
  return $items;
}
