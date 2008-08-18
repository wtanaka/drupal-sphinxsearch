<?php
// $Id: sphinxsearch_xmlpipe.php,v 1.1 2008/08/18 13:52:24 markuspetrux Exp $

/**
 * @file
 * Handles incoming requests Sphinx indexer to generate XMLPipe stream.
 * Access control to this script is based on IP Addresses specified
 * in module settings panel.
 */

// Compute Drupal root path for current site.
$drupal_root_path = dirname(dirname($_SERVER['SCRIPT_FILENAME']));

if (!file_exists($drupal_root_path .'/includes/bootstrap.inc')) {
  print "Could not locate Drupal's bootstrap.inc script. This script should be installed on a subdirectory of your Drupal installation.\n";
  exit;
}

// Change current working directory to Drupal root.
chdir($drupal_root_path);

// Boot Drupal.
require_once('./includes/bootstrap.inc');
drupal_bootstrap(DRUPAL_BOOTSTRAP_FULL);

// Load XMLPipe generator code.
require_once(drupal_get_path('module', 'sphinxsearch') .'/sphinxsearch.xmlpipe.inc');

// Launch XMLPipe generator.
sphinxsearch_xmlpipe();
