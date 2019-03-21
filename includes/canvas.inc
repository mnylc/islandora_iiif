<?php

/**
 * @file
 */

/**
 * Implements hook_islandora_iiif_manifest_query_backends().
 */
function islandora_iiif_islandora_iiif_canvas_query_backends() {
  $module_path = drupal_get_path('module', 'islandora_solr');
  return array(
    'islandora_solr_query_backend' => array(
      'title' => t('Manifest via Solr'),
      'callable' => 'islandora_iiif_islandora_canvas_solr_backend_callable',
      'file' => "$module_path/includes/canvas.inc",
    ),
  );
}

/**
 * Implements callback_islandora_iiif_canvas_query_backends()
 */
function islandora_iiif_islandora_canvas_solr_backend_callable($object) {
  module_load_include('inc', 'islandora_solr', 'includes/db');
  module_load_include('inc', 'islandora_solr', 'includes/db');
  $qp = new IslandoraSolrQueryProcessor();

  $qp->buildQuery(format_string('PID:"!pid"', array(
    '!pid' => $object->id,
  )));

  $limit = variable_get('islandora_solr_num_of_results', '20');

  $qp->solrStart = 0;
  $qp->solrLimit = 1;

  // We should have a default sort here for sequences.
  $qp->executeQuery();
  // dpm($qp->islandoraSolrResult['response']);

  return array(
    $qp->islandoraSolrResult['response']['numFound'],
    $qp->islandoraSolrResult['response']['objects'],
  );
}

/**
 * This is the actual canvas building function.
 */
function islandora_iiif_canvas_callback(AbstractObject $object) {
  module_load_include('inc', 'islandora_solr_metadata', 'includes/db');
  module_load_include('inc', 'islandora_solr_metadata', 'theme/theme');

  $iiif_base_url = variable_get('islandora_iiif_iiif_url', 'iiif/2');
  $tile_prefix = urlencode(variable_get('islandora_iiif_tile_url_prefix', 'http://127.0.0.1:8080/fedora/objects/'));
  $tile_suffix = urlencode(variable_get('islandora_iiif_tile_url_suffix', '/datastreams/OBJ/content'));


/** width and height by info.json not solr 
/
  $canvas_info_total = islandora_iiif_islandora_canvas_solr_backend_callable($object);

  $canvas_info = $canvas_info_total[1];
  $intwidth = isset($canvas_info['solr_doc']['RELS_INT_width_literal_s']) ? $canvas_info['solr_doc']['RELS_INT_width_literal_s'] : 640;
  $intheight = isset($canvas_info['solr_doc']['RELS_INT_height_literal_s']) ? $canvas_info['solr_doc']['RELS_INT_height_literal_s'] : 480;
  $fitswidth = isset($canvas_info['solr_doc']['fits_metadata_image_imageWidth_ms']) ? $canvas_info['solr_doc']['fits_metadata_image_imageWidth_ms'][0] : 640 ; 
  $fitsheight = isset($canvas_info['solr_doc']['fits_metadata_image_imageHeight_ms']) ? $canvas_info['solr_doc']['fits_metadata_image_imageHeight_ms'][0] : 480 ; 
  $height = max($intheight, $fitsheight);
  $width = max($intwidth ,  $fitswidth);
**/

  $info_response = file_get_contents($iiif_base_url . '/' . $tile_prefix . urlencode($object->id). $tile_suffix .'/info.json');
  $jsonArray = json_decode($info_response,true);
  $width = isset($jsonArray['width']) ? $jsonArray['width'] : 640;
  $height = isset($jsonArray['height']) ? $jsonArray['height'] : 480;
  

  $canvas = new \stdClass();
  $canvas->{'@context'} = "http://iiif.io/api/presentation/2/context.json";
  $canvas->{'@id'} = urldecode(url(current_path(),
     array(
       'absolute' => TRUE,
       'language' => (object) array('language' => FALSE),
     ))
  );

  $on_id = urldecode(url("islandora_iiif/{$object->id}/canvas/default", array(
    'absolute' => TRUE,
    'language' => (object) array('language' => FALSE),
  )));

  $canvas->{'@type'} = "sc:Canvas";
  $canvas->label = $object->label;
  $canvas->height = $height;
  $canvas->width = $width;
  $canvas->images = array(
    array(
       "@id" => urldecode(url("islandora_iiif/{$object->id}/annotation/default", array(
          'absolute' => TRUE,
          'language' => (object) array('language' => FALSE),
          ))
       ),
      "@type" => "oa:Annotation",
      "motivation" => "sc:painting",
      "resource" => array(
//        "@id" => urldecode(url("iiif-server/iiif/2/http%3A%2F%2F150.145.48.37%3A8080%2Ffedora%2Fobjects%2F{$object->id}%2Fdatastreams%2FOBJ%2Fcontent/full/full/0/default.jpg", array(
        "@id" => $iiif_base_url . '/' . $tile_prefix . urlencode($object->id) . $tile_suffix . '/full/full/0/default.jpg',
//          'absolute' => TRUE,
//          'language' => (object) array('language' => FALSE),
//        ))
//        ),
        "@type" => "dctypes:Image",
        "height" => $height,
        "width" => $width,
        "format" => "image/jpeg",
            "service" => array(
              "@context" => "http://iiif.io/api/image/2/context.json",
              "profile" => "http://iiif.io/api/image/2/level2.json",
              "@id" => $iiif_base_url . '/' . $tile_prefix . urlencode($object->id) . $tile_suffix ,
//              "@id" => urldecode(url("iiif-server/iiif/2/http%3A%2F%2F150.145.48.37%3A8080%2Ffedora%2Fobjects%2F{$object->id}%2Fdatastreams%2FOBJ%2Fcontent", array(
//                 'absolute' => TRUE,
//                 'language' => (object) array('language' => FALSE),
//              ))
//              ),
            ),
      ),
    "on" => $on_id,
      "otherContent" => array(
        "@type" => "sc:AnnotationList",
        "@embed" => FALSE,
      ),
    ),
  );

  // And to make this legit we need a sequence with some canvases.
  drupal_add_http_header('Content-Type', 'application/json; charset=utf-8');
  drupal_add_http_header('access-control-allow-origin', '*');
  echo json_encode($canvas, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE | JSON_NUMERIC_CHECK);
  drupal_page_footer();

}
