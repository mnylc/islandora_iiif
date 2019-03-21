<?php

/**
 * @file
 */

/**
 * This is the actual annotation building function.
 */
function islandora_iiif_annotation_callback(AbstractObject $object) {
  module_load_include('inc', 'islandora_solr_metadata', 'includes/db');
  module_load_include('inc', 'islandora_solr_metadata', 'theme/theme');

  $iiif_base_url = variable_get('islandora_iiif_iiif_url', 'iiif/2');
  $tile_prefix = urlencode(variable_get('islandora_iiif_tile_url_prefix', 'http://127.0.0.1:8080/fedora/objects/'));
  $tile_suffix = urlencode(variable_get('islandora_iiif_tile_url_suffix', '/datastreams/OBJ/content'));

  $info_response = file_get_contents($iiif_base_url . '/' . $tile_prefix . urlencode($object->id). $tile_suffix .'/info.json');
  $jsonArray = json_decode($info_response,true);
  $width = isset($jsonArray['width']) ? $jsonArray['width'] : 640;
  $height = isset($jsonArray['height']) ? $jsonArray['height'] : 480;

  $on_id = urldecode(url("islandora_iiif/{$object->id}/canvas/default", array(
    'absolute' => TRUE,
    'language' => (object) array('language' => FALSE),
  )));

  $annotation = new \stdClass();
  $annotation->{'@context'} = "http://iiif.io/api/presentation/2/context.json";
  $annotation->{'@id'} = urldecode(url("islandora_iiif/{$object->id}/annotation/default", array(
       'absolute' => TRUE,
       'language' => (object) array('language' => FALSE),
     ))
  );

  $annotation->{'@type'} = "oa:Annotation";
  $annotation->{'motivation'} = "sc:painting";
  $annotation->resource = array(
        "@id" => $iiif_base_url . '/' . $tile_prefix . urlencode($object->id) . $tile_suffix . '/full/full/0/default.jpg',
        "@type" => "dctypes:Image",
        "height" => $height,
        "width" => $width,
        "format" => "image/jpeg",
        "service" => array(
              "@context" => "http://iiif.io/api/image/2/context.json",
              "profile" => "http://iiif.io/api/image/2/level2.json",
              "@id" => $iiif_base_url . '/' . $tile_prefix . urlencode($object->id) . $tile_suffix ,
        ),
  );
  $annotation->{'on'} = $on_id;

  // And to make this legit we need a sequence with some canvases.
  drupal_add_http_header('Content-Type', 'application/json; charset=utf-8');
  drupal_add_http_header('access-control-allow-origin', '*');
  echo json_encode($annotation, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE | JSON_NUMERIC_CHECK);
  drupal_page_footer();

}
