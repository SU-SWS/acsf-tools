<?php
/**
 * @file
 * Utility command to fetch ACSF drush aliases.
 */

use Symfony\Component\Yaml\Yaml;

/**
 * Implements hook_drush_command().
 */
function acsf_tools_generate_aliases_drush_command() {
  $items = array();

  $items['acsf-tools-generate-aliases'] = array(
    'description' => dt('Fetch the drush aliases for all the sites in your factory.'),
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_SITE,
    'arguments' => array(
      'env' => 'The environment whose tag we\'re requesting. I.e., dev, test, prod'
    ),
    'options' => array(
      'result-folder' => 'The folder in which the dumps will be written. Defaults to {repo_root}/drush/sites.',
    ),
    'allow-additional-options' => TRUE,
    'examples' => array(
      'drush acsf-tools-generate-aliases' => 'Fetch drush aliases for all the sites in your factory.',
    ),
    'aliases' => ['sfa'],
  );

  return $items;
}

/**
 * Drush callback to fetch all drush aliases and store them locally.
 *
 * @return bool
 */
function drush_acsf_tools_generate_aliases($env = "prod", $stack = 01) {

  $config = acsf_tools_get_rest_config();

  // Query REST API for a list of all site names.
  $sites = get_factory_sites($config, 'prod');

  // Delete the existing sites file, if present.
  $realpath = realpath(__DIR__);
  $sites_file = $realpath . '/acsf_sites_list.yml';
  if (file_exists($sites_file)) {
    unlink($sites_file);
  }

  // Repopulate the sites file.
  $site_names = array();
  foreach ($sites as $site) {
    if ($site->stack_id == $config->stack_id) {
      $site_names['sites'][] = $site->site;
    }
  }
  $yaml = Yaml::dump($site_names);
  file_put_contents($sites_file, $yaml);

  // Put our aliases file in ../site-aliases.
  $result_folder =  drush_get_option('result-folder', dirname(dirname(__FILE__)) . "/sites");

  if (!is_dir($result_folder) || !is_writable($result_folder)) {
    // Target folder does not exist. Try to create it.
    if (!mkdir($result_folder, 0777, TRUE)) {
      drush_log("\nImpossible to write to $result_folder folder.", 'error');
      return;
    }
  }

  // Clear drush cache to load the new aliases.
  drush_cache_clear_drush();
  drush_print(dt("ACSF aliases generated for $config->site_id."));
}
