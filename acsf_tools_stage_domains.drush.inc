<?php

/**
 * Implements hook_drush_command().
 */
function acsf_tools_stage_domains_drush_command() {
  $items = array();

  $items['acsf-tools-stage-domains'] = array(
    'description' => dt('Automatically stage the production Factories\' vanity domains to a lower environment.'),
    'arguments' => array(
      'env' => 'The environment we\'re staging domains to. E.g. dev\test',
    ),
    'required-arguments' => TRUE,
    'bootstrap' => DRUSH_BOOTSTRAP_DRUPAL_FULL,
    'examples' => array(
      'drush @mysite.local acsf-stage-domains dev',
    ),
    'aliases' => ['sfdo'],
  );

  return $items;
}

/**
 * Drush callback to copy custom domain names from production down to
 * a lower environment (dev/stage).
 *
 * @param $env
 * @return bool
 */
function drush_acsf_tools_stage_domains($env) {

  if (!in_array($env, array('dev','test'))) {
    return drush_set_error('Invalid staging environment.');
  }

  $config = acsf_tools_get_rest_config();

  // Get Sites in the prod factory.
  $sites = get_factory_sites($config, 'prod');

  foreach ($sites as $site) {

    post_vanity_domain($config, $site, $env);
  }
}

/**
 * Helper function to get a list of sites in the production factory.
 *
 * @param $username
 * @param $password
 * @return mixed
 */
function get_factory_sites($config, $env = 'prod') {
  $limit = 100;
  // Find out how many sites are in the environment first. Then break it up into
  // multiple api calls.
  $sites_count_url = acsf_tools_get_factory_url($config, '/api/v1/sites?limit=1', $env);
  $sites_count = acsf_tools_curl_wrapper($config->username, $config->password, $sites_count_url);
  $total_pages = ceil($sites_count->count / $limit);
  $sites = [];

  // For each page, get the list of site machine names.
  for ($page = 1; $page <= $total_pages; $page++) {
    // $page_sites = stanford_acsf_api_call("sites?limit=$limit&page=$page");
    $sites_url = acsf_tools_get_factory_url($config, "/api/v1/sites?limit=$limit&page=$page", $env);
    $page_sites = acsf_tools_curl_wrapper($config->username, $config->password, $sites_url)->sites;
    foreach ($page_sites as $i => $site) {
      $sites[$site->site] = $site;
    }
  }
  return $sites;
}

/**
 * Get a list of all sites on the provided stack.
 *
 * @param int $stack
 *   Stack id in ACSF.
 *
 * @return array
 *   List of all site names on the stack.
 *
 * @throws \Exception
 */
function stanford_acsf_get_sites($config, $env, $stack = 2) {
  $limit = 100;
  // Find out how many sites are in the environment first. Then break it up into
  // multiple api calls.
  $sites_count = stanford_acsf_api_call('sites?limit=1');
  $total_pages = ceil($sites_count['count'] / $limit);
  $sites = [];
  // For each page, get the list of site machine names.
  for ($page = 1; $page <= $total_pages; $page++) {
    $page_sites = stanford_acsf_api_call("sites?limit=$limit&page=$page");
    // Remove sites that are collections or aren't on the provided stack.
    $page_sites = array_filter($page_sites['sites'], function ($site) use ($stack) {
      return !$site['site_collection'] && $site['stack_id'] == $stack;
    });
    // Trim down the data to just the site machine name.
    array_walk($page_sites, function (&$site) {
      $site = $site['site'];
    });
    // Merge them all together.
    $sites = array_merge($sites, $page_sites);
  }
  return array_values($sites);
}

/**
 * [get_prod_vanity_domain_for_site description]
 * @param  [type] $config [description]
 * @param  [type] $site   [description]
 * @return [type]         [description]
 */
function get_prod_vanity_domain_for_site($config, $site) {

  $domain_url = acsf_tools_get_factory_url($config,"/api/v1/domains/$site->id");
  $result = acsf_tools_curl_wrapper($config->username, $config->password, $domain_url);

  return $result->domains->custom_domains[0];
}

/**
 * Helper function to post a vanity domain to a staging environment.
 *
 * @param $config
 * @param $site
 * @param $env
 */
function post_vanity_domain($config, $site, $env) {

  $post_domain_url = acsf_tools_get_factory_url($config,"/api/v1/domains/$site->id/add", $env);

  if ($stage_domain = get_stage_domain($config, $site, $env)) {

    $data = array(
      'domain_name' => $stage_domain
    );
    $result = acsf_tools_curl_wrapper($config->username, $config->password, $post_domain_url, $data);

    drush_print("$stage_domain set OK.");
  }
}

/**
 * Utility function to parse and set the staging domain based on a user defined
 * pattern set in acsf_tools_config.yml.
 *
 * @param $config
 * @param $site
 * @param $env
 * @return string
 */
function get_stage_domain($config, $site, $env) {

  // Set the url_env string to 'stage' if env is test.
  $url_env = ($env == 'test') ? 'stage' : $env;

  // Find the existing prod vanity domain.
  if ($prod_domain = get_prod_vanity_domain_for_site($config, $site)) {

    // Get the subdomain off the prod vanity url. E.g., 'www.foo.com'
    // is 'www'. Or, 'coolsite.domain.com' is 'coolsite'.
    $parts = explode('.', $prod_domain, 2);
    $subdomain = $parts[0];
    $prod_root_domain = $parts[1];

    if ($subdomain == 'www') {
      // If subdomain is www, then stage pattern should be dev.foo.com,
      // test.foo.com, etc.
      $new_subdomain = $url_env;
    } else {
      // If subdomain is custom, e.g., 'coolsite.foo.com', then follow
      // the pattern set in acsf_tools_config.yml - default is {subdomain}-{env},
      // e.g., coolsite-dev.foo.com.
      $new_subdomain = str_replace('{subdomain}', $subdomain, $config->subdomain_pattern);
      $new_subdomain = str_replace('{env}', $url_env, $new_subdomain);
    }

    return "$new_subdomain.$prod_root_domain";
  }

  return FALSE;
}
