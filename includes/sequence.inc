<?php

/**
 * @file
 */

/**
 * Implements hook_islandora_iiif_manifest_query_backends().
 */
function islandora_iiif_islandora_iiif_sequence_query_backends() {
  $module_path = drupal_get_path('module', 'islandora_solr');
  return array(
    'islandora_solr_query_backend' => array(
      'title' => t('Sequence via Solr'),
      'callable' => 'islandora_iiif_islandora_sequence_solr_backend_callable',
      'file' => "$module_path/includes/sequence.inc",
    ),
  );
}

/**
 * Implements hook_islandora_iiif_parentsolr_predicate()
 */
function islandora_iiif_islandora_iiif_parentsolr_predicate(AbstractObject $object = NULL) {

  // This is the simplest implementation of this, we won't
  // care here about a particular CMODEL, we simply respond with
  // all predicates we care for.
  // other implementations can simply alter or overwrite what they find.
  // No particular order here.
  $rels_predicate_compound = 'RELS_EXT_' . variable_get('islandora_compound_object_relationship', 'isConstituentOf') . '_ms';
  $rels_predicate_paged = 'RELS_EXT_isPageOf_uri_ms';
  $rels_predicate_generic_member = variable_get('islandora_solr_member_of_field', 'RELS_EXT_isMemberOf_uri_ms');

  // Using literal keys is faster way to avoid duplicates than
  // de-duplicating.
  $condition = format_string(':("info:fedora/!pid" OR "!pid")', array(
    '!pid' => $object->id,
  ));

  $predicates[$rels_predicate_compound] = $rels_predicate_compound . $condition;
  $predicates[$rels_predicate_paged] = $rels_predicate_paged . $condition;
  $predicates[$rels_predicate_generic_member] = $rels_predicate_generic_member . $condition;
  return $predicates;
}

/**
 * Implements callback_islandora_iiif_manifest_query_backends()
 */
function islandora_iiif_islandora_sequence_solr_backend_callable($object, $page, $limit) {
  module_load_include('inc', 'islandora_solr', 'includes/db');
  $qp = new IslandoraSolrQueryProcessor();
  $solr_predicates = array();
  $solr_predicates = module_invoke_all('islandora_iiif_parentsolr_predicate', $object);
  // And the alter for those who like to alter.
  drupal_alter('islandora_iiif_parentsolr_predicate', $object, $solr_predicates);

  $qp->buildQuery(format_string("!predicates", array(
    '!predicates' => !empty($solr_predicates) ? (implode(' OR ', $solr_predicates)) : '',
    '!pid' => $object->id,
  )), drupal_get_query_parameters());

///???///  $limit = variable_get('islandora_solr_num_of_results', '20');

  $qp->solrStart = $page * $limit;
  $qp->solrLimit = $limit;

  // We should have a default sort here for sequences.
////  $qp->solrParams['sort'] = "RELS_EXT_isSequenceNumber_s ASC, RELS_EXT_isPageNumber_s ASC";
  $qp->solrParams['sort'] = "RELS_EXT_isSequenceNumber_literal_s ASC";

  if (!empty($qp->internalSolrParams['sort'])) {
    $qp->solrParams['sort'] .= "," . $qp->internalSolrParams['sort'];
  }

  $qp->executeQuery();
  //dpm ($qp->islandoraSolrResult['response']);
  return array(
    $qp->islandoraSolrResult['response']['numFound'],
    $qp->islandoraSolrResult['response']['objects'],
  );
};

/**
 * This is the actual manifest building function.
 */
function islandora_iiif_sequence_generator(AbstractObject $object, $fromManifest) {
  module_load_include('inc', 'islandora_iiif', 'includes/canvas');

  // This is weird. Just to test really. I´m putting the limit to 2000 here. Would love if.
  $canvas_list = islandora_iiif_islandora_sequence_solr_backend_callable($object, 0, 2000);

  if ($canvas_list[0] == 0) {
    // IN case of no little children, we get info directly from the object.
    // @TODO: revisit this logic, not happy.
    $canvas_list = islandora_iiif_islandora_canvas_solr_backend_callable($object);
  }

  $sequence = new \stdClass();
// The default sequence should be written out in full within the manifest file, but MUST NOT have the @context property.

    if (!$fromManifest) {
       $sequence->{'@context'} = "http://iiif.io/api/presentation/2/context.json";
    }

// when call from manifest return an incorrect URL
//  $sequence->{'@id'} = urldecode(url(current_path(), array(
    $sequence->{'@id'} = urldecode(url("islandora_iiif/{$object->id}/sequence/default", array(
      'absolute' => TRUE,
      'language' => (object) array('language' => FALSE),
    )
    )
  );
  $sequence->{'@type'} = "sc:Sequence";
  $sequence->label = "Normal";
  $canvases = array();
  foreach ($canvas_list[1] as $canvas_info) {
    $label = $canvas_info['object_label'];

    $id = urldecode(url("islandora_iiif/{$canvas_info['PID']}/canvas/default", array(
      'absolute' => TRUE,
      'language' => (object) array('language' => FALSE),
    )));
    

/** width and height by info.json not solr
/
    $intwidth = isset($canvas_info['solr_doc']['RELS_INT_width_literal_s']) ? $canvas_info['solr_doc']['RELS_INT_width_literal_s'] : 640;
    $intheight = isset($canvas_info['solr_doc']['RELS_INT_height_literal_s']) ? $canvas_info['solr_doc']['RELS_INT_height_literal_s'] : 480;
    $fitswidth = isset($canvas_info['solr_doc']['fits_metadata_image_imageWidth_ms']) ? $canvas_info['solr_doc']['fits_metadata_image_imageWidth_ms'][0] : 640 ; 
    $fitsheight = isset($canvas_info['solr_doc']['fits_metadata_image_imageHeight_ms']) ? $canvas_info['solr_doc']['fits_metadata_image_imageHeight_ms'][0] : 480 ; 
    $height = max($intheight, $fitsheight);
    $width = max($intwidth ,  $fitswidth);
**/

  $iiif_base_url = variable_get('islandora_iiif_iiif_url', 'iiif/2');
  $tile_prefix = urlencode(variable_get('islandora_iiif_tile_url_prefix', 'http://127.0.0.1:8080/fedora/objects/'));
  $tile_suffix = urlencode(variable_get('islandora_iiif_tile_url_suffix', '/datastreams/OBJ/content'));
  $info_response = file_get_contents($iiif_base_url . '/' . $tile_prefix . urlencode($canvas_info['PID']). $tile_suffix .'/info.json');
//  $info_response = file_get_contents('http://v2p2arch.to.cnr.it/iiif-server/iiif/2/http%3A%2F%2F150.145.48.37%3A8080%2Ffedora%2Fobjects%2F'.urlencode($canvas_info['PID']).'%2Fdatastreams%2FOBJ%2Fcontent/info.json');

  $jsonArray = json_decode($info_response,true);
  $width = isset($jsonArray['width']) ? $jsonArray['width'] : 640;
  $height = isset($jsonArray['height']) ? $jsonArray['height'] : 480;

    $canvases[] = array(
      "@id" => $id ,
      "@type" => "sc:Canvas",
      "label" => $label ,
      "height" => $height,
      "width" => $width,
      "images" => array(
        array(
          "@id" => urldecode(url("islandora_iiif/{$canvas_info['PID']}/annotation/default", array(
             'absolute' => TRUE,
             'language' => (object) array('language' => FALSE),
             ))
          ),
          "@type" => "oa:Annotation",
          "motivation" => "sc:painting",
          "resource" => array(
//            "@id" => urldecode(url("iiif-server/iiif/2/http%3A%2F%2F150.145.48.37%3A8080%2Ffedora%2Fobjects%2F{$canvas_info['PID']}%2Fdatastreams%2FOBJ%2Fcontent/full/full/0/default.jpg", array(
//            'absolute' => TRUE,
//            'language' => (object) array('language' => FALSE),
//            ))
//            ),
            "@id" => $iiif_base_url . '/' . $tile_prefix . urlencode($canvas_info['PID']) . $tile_suffix . '/full/full/0/default.jpg',
            "@type" => "dctypes:Image",
            "height" => $height,
            "width" => $width,
            "format" => "image/jpeg",
            "service" => array(
              "@context" => "http://iiif.io/api/image/2/context.json",
              "profile" => "http://iiif.io/api/image/2/level2.json",
//              "@id" => urldecode(url("iiif-server/iiif/2/http%3A%2F%2F150.145.48.37%3A8080%2Ffedora%2Fobjects%2F{$canvas_info['PID']}%2Fdatastreams%2FOBJ%2Fcontent", array(
//                 'absolute' => TRUE,
//                 'language' => (object) array('language' => FALSE),
//              ))
//              ),
                "@id" => $iiif_base_url . '/' . $tile_prefix . urlencode($canvas_info['PID']) . $tile_suffix ,
            ),
          ),
          "on" => $id,
        ),
      ),
    );
  }

  // And to make this legit we need a sequence with some canvases.
  $sequence->canvases = $canvases;

  return $sequence;
}

/**
 * This is the actual manifest building function.
 */
function islandora_iiif_sequence_callback(AbstractObject $object) {
  $sequence = islandora_iiif_sequence_generator($object, FALSE);
  drupal_add_http_header('Content-Type', 'application/json; charset=utf-8');
  
  drupal_add_http_header('access-control-allow-origin', '*');
  echo json_encode($sequence, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE | JSON_NUMERIC_CHECK);
  drupal_page_footer();

}
