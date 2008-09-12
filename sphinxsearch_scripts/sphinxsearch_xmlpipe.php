<?php
// $Id: sphinxsearch_xmlpipe.php,v 1.3 2008/09/12 02:44:22 markuspetrux Exp $

/**
 * @file
 * Handles incoming requests from Sphinx indexer to generate XMLPipe stream.
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
$module_path = drupal_get_path('module', 'sphinxsearch');
require_once($module_path .'/sphinxsearch.common.inc');
require_once($module_path .'/sphinxsearch.xmlpipe.inc');

// Since this script is located on a subdirectory that should be copied to
// Drupal root directory, we use version numbers as a method to handshake with
// currently installed module, so we can prevent possible problems if someone
// updated the module but forgot to update this script.
// Anyway, this version number needs to be altered only if/when this script
// is changed in a way that may generate incompatibility issues. Otherwise,
// it doesn't matter much if it is not updated. It is just safety measure in
// case we need to check for incompatibility issues in the future.
$sphinxsearch_xmlpipe_generator_version = 2;

// Launch XMLPipe generator.
sphinxsearch_xmlpipe($sphinxsearch_xmlpipe_generator_version);
