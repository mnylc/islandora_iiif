<?php

/**
 * @file
 * Cleans a fake filename for IIIF´s Server API.
 *
 * @param string $filename
 *   A File name as IIIF Server API would expect.
 * @param array $allowedExtensionse
 *   An array with allowed extensions.
 *
 * @return array
 *   Containing basename and its extension
 *
 * @author Diego Pino Navarro
 */

/**
 *
 */
function islandora_iiif_safefile($filename, $allowedExtensions = array('jpg')) {
  // @TODO: this should be defined per backend image implementation.
  list($filename) = explode('?', $filename);
  list($filename) = explode('#', $filename);
  $filename = rawurldecode(drupal_basename($filename));
  // Remove white space variations.
  $filename = trim($filename, " \t\n\r\0\x0B.");

  if (strpos($filename, '.') === FALSE) {
    $extension = FALSE;
  }
  else {
    $dotpos = strrpos($filename, '.');
    $extension = drupal_strtolower(substr($filename, $dotpos + 1));
    $basename = drupal_strtolower(substr($filename, 0, $dotpos));
  }

  if (!$extension || !in_array($extension, $allowedExtensions, TRUE)) {
    throw new RuntimeException(t('The file @file has an invalid extension.', array('@file' => $filename)));
  }

  $basename = file_munge_filename($basename, implode(" ", $allowedExtensions), FALSE);
  return array($basename, $extension);
}

/**
 *
 */
function islandora_iiif_adoredjatoka_info_json($image_info) {

  // Note: Adore-djatoka tiles work on 96px multipliers.
  $info = array(
    "@context" => "http://iiif.io/api/image/2/context.json",
    "@embed" => "true",
    "protocol" => "http://iiif.io/api/image",
    "width" => (int) $image_info["width"],
    "height" => (int) $image_info["height"],
    "tiles" => array(
      array("width" => 256, "scaleFactors" => range(1, $image_info['levels'])),
    ),
    "profile" => array("http://iiif.io/api/image/2/level2.json"),
  );
  return $info;
}

/**
 *
 */
function islandora_iiif_djatoka_region($image_info, $iiif_params) {
  // We need to translate basically everything to zoom level + region + rotate.
  $offsets = array();
  $region_dimensions = array();

  $match = array();
  $h = (int) $image_info["height"];
  $w = (int) $image_info["width"];
  // Set this by default in case nothing matches.
  $region_dimensions = array($w, $h);
  $djatoka_params_translated = array(
    'region' => "0,0," . implode(",", array_reverse($region_dimensions)),
    'rotate' => 0,
    'level' => $image_info['levels'],
    'scale' => NULL,
  );
  // Preventive in case region does not fit anything else:
  // I will precalculate some of the non string options here.
  // First Region:
  preg_match('/^([pct:]*)([\d\.]+),([\d\.]+),([\d\.]+),([\d\.]+)$/', $iiif_params['region'], $match);
  if (count($match) == 6) {
    if ($match[1] == "pct:") {
      $x = round(($match[2] / 100) * $w, 0, PHP_ROUND_HALF_DOWN);
      $y = round(($match[3] / 100) * $h, 0, PHP_ROUND_HALF_DOWN);
      $dw = round(($match[4] / 100) * $w, 0, PHP_ROUND_HALF_DOWN);
      $dh = round(($match[5] / 100) * $h, 0, PHP_ROUND_HALF_DOWN);
    }
    else {
      $x = (int) $match[2];
      $y = (int) $match[3];
      $dw = (int) $match[4];
      $dh = (int) $match[5];
    }
    $offsets = array($x, $y);
    $region_dimensions = array($dw, $dh);
  }
  else {
    switch ($iiif_params['region']) {
      case 'full':
        break;

      case 'square':
        $min = min($h, $w);
        $max = max($h, $w);
        $offset = round(($max - $min) / 2, 0, PHP_ROUND_HALF_DOWN);
        $offsets = array($offset, 0);
        $region_dimensions = array($min, $min);
        if ($h >= $w) {
          $offsets = array_reverse($offsets);
        }
        break;

      default:
        // Hell breaks loose, nothing in our Level 2 Profile matches.
        // @TODO return something i can catch in a parent call
        // Failing argument or format should return a 400.
    }
  }
  $match = array();
  // Second Size:
  switch (TRUE) {
    case $iiif_params['size'] == "full" || $iiif_params['size'] == "max":
      // We will clean all NULLs at the end.
      $djatoka_params_translated['scale'] = NULL;
      break;

    case 1 === preg_match('/^(\d+),$/', $iiif_params['size'], $match):
      $djatoka_params_translated['scale'] = (int) $match[1] . ",0";
      $server_ratio = (int) ($region_dimensions[0]) / (int) $match[1];
      $level = $image_info['levels'] - floor(log($server_ratio, 2));
      $level = $level < 1 ? 1 : $level;
      $djatoka_params_translated['level'] = $level;
      break;

    case 1 === preg_match('/^,(\d+)$/', $iiif_params['size'], $match):
      $djatoka_params_translated['scale'] = "0," . (int) $match[1];
      $server_ratio = (int) ($region_dimensions[1]) / (int) $match[1];
      $level = $image_info['levels'] - floor(log($server_ratio, 2));
      $level = $level < 1 ? 1 : $level;
      $djatoka_params_translated['level'] = $level;
      break;

    case 1 === preg_match('/^pct:(\d*\.?\d*)$/i', $iiif_params['size'], $match):
      // Not sure if we should allow over zoom
      // without the log() we can really go over 1.0 on scale as much as we want
      // But since IIIF is all about exposing, should we allow that much traffic?
      $server_ratio = $match[1] / 100;
      $level = $image_info['levels'] - floor(log(1 / $server_ratio, 2));
      $level = $level < 1 ? 1 : $level;
      $reduction = $image_info['levels'] - $level;
      // Would love to use ** but was introduced in php 5.6.
      $scale = (float) ($server_ratio * pow(2, $reduction));
      // This keeps all in the 100% limit.
      $scale = $scale <= 1 ? $scale : 1;
      $djatoka_params_translated['scale'] = number_format($scale, 1, '.', ',');
      $djatoka_params_translated['level'] = $level;
      break;

    case 1 === preg_match('/^([\d]+),([\d]+)$/', $iiif_params['size'], $match):
      // This is really fake. Adore-djatoka can't deliver distorted images at all.
      $djatoka_params_translated['scale'] = (int) $match[0];
      $server_ratio = (int) ($region_dimensions[0]) / (int) $match[1];
      $level = $image_info['levels'] - round(log($server_ratio, 2), 0);
      $level = $level < 1 ? 1 : $level;
      $djatoka_params_translated['level'] = $level;
      break;

    case 1 === preg_match('/^!(\d+),(\d+)$/', $iiif_params['size'], $match):
      $best_scale = min($match[1] / $region_dimensions[0], $match[2] / $region_dimensions[1]);
      $dw = floor(ceil($region_dimensions[0] * $best_scale), 0);
      $dh = floor(ceil($region_dimensions[1] * $best_scale), 0);
      $djatoka_params_translated['scale'] = $dw . "," . $dh;
      $server_ratio = ($region_dimensions[0] / $dw);
      $level = $image_info['levels'] - round(log($server_ratio, 2), 0);
      $level = $level < 1 ? 1 : $level;
      $djatoka_params_translated['level'] = $level;
      break;
  }
  // Offset offsets things...jiji.
  if (count($offsets) > 0) {

    $exp_reduction = pow(2, $image_info['levels'] - $djatoka_params_translated['level']);
    // dpm("calculated level" . $djatoka_params_translated['level']);
    // dpm("reduction" . $exp_reduction);
    $powpow = function ($v) use ($exp_reduction) {
      if ($exp_reduction > 0) {
        return round($v / $exp_reduction, 0);
      }
      return $v;
    };
    list($dw, $dh) = array_map($powpow, $region_dimensions);
    list($dx, $dy) = $offsets;
    $djatoka_params_translated['region'] = $dy . "," . $dx . "," . $dh . "," . $dw;
  }

  // Third Rotation.
  $rotmod = (int) $iiif_params['rotation'] % 90;
  // This basically converts any degree in chunks of 90 between 0 and 360.
  // Can't remember last time i had to do this.
  $djatoka_params_translated['rotation'] = ((int) $iiif_params['rotation'] - $rotmod) % 360;
  return $djatoka_params_translated;
}

/**
 *
 */
function islandora_iiif_djatoka_metadata_request($pid) {
  module_load_include('inc', 'islandora', 'includes/authtokens');
  $uri_params = array(
    'absolute' => TRUE,
    'query' => array(
      'token' => islandora_get_object_token($pid, "OBJ", 2),
    ),
  );

  $djatoka_metadata_params = array(
    'url_ver' => 'Z39.88-2004',
    'rft_id' => url("islandora/object/{$pid}/datastream/OBJ/view", $uri_params),
    'svc_id' => 'info:lanl-repo/svc/getMetadata',
  );

  $djatoka_server_url = variable_get('islandora_iiif_djatoka_url', 'http://localhost:8080/adore-djatoka/resolver');
  $djatoka_url = url($djatoka_server_url, array(
    'absolute' => TRUE,
    'language' => (object) array('language' => FALSE),
    'query' => $djatoka_metadata_params,
  ));
  return drupal_http_request($djatoka_url);
}

/**
 *
 */
function islandora_iiif_djatoka_image_request_url($pid, $djatoka_params_translated) {
  module_load_include('inc', 'islandora', 'includes/authtokens');
  $uri_params = array(
    'absolute' => TRUE,
    'query' => array(
      'token' => islandora_get_object_token($pid, "OBJ", 2),
    ),
  );

  // Output format should be requested by the caller.
  // @TODO add to $djatoka_params_translated
  $djatoka_metadata_params = array(
    'url_ver' => 'Z39.88-2004',
    'rft_id' => url("islandora/object/{$pid}/datastream/OBJ/view", $uri_params),
    'svc_id' => 'info:lanl-repo/svc/getRegion',
    'svc_val_fmt' => 'info:ofi/fmt:kev:mtx:jpeg2000',
    'svc.format' => 'image/jp2',
    'svc.level' => $djatoka_params_translated['level'],
    'svc.rotate' => $djatoka_params_translated['rotation'],
    'svc.region' => $djatoka_params_translated['region'],
    'svc.scale' => $djatoka_params_translated['scale'],
  );
  $djatoka_metadata_params = array_filter($djatoka_metadata_params);
  // @TODO make sure to change this to an argument to make the call faster

  if (variable_get('islandora_iiif_resizeondrupal', FALSE)) {
    unset($djatoka_metadata_params['svc.scale']);
    // This is checked again in the caller.
    // But we need to unset it anyway here because we are making one
    // single URL out of all this parameters.
  }

  $djatoka_server_url = variable_get('islandora_iiif_djatoka_url', 'http://localhost:8080/adore-djatoka/resolver');
  $djatoka_url = url($djatoka_server_url, array(
    'absolute' => TRUE,
    'language' => (object) array('language' => FALSE),
    'query' => $djatoka_metadata_params,
  ));
  return $djatoka_url;
}
