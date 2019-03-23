<?php

/**
 * @file
 */

/**
 * Implements hook_islandora_iiif_collection_query_backends().
 */
function islandora_iiif_islandora_iiif_collection_query_backends() {
  $module_path = drupal_get_path('module', 'islandora_solr');
  return array(
    'islandora_solr_query_backend' => array(
      'title' => t('Collection via Solr'),
      'callable' => 'islandora_iiif_islandora_collection_solr_backend_callable',
      'file' => "$module_path/includes/collection.inc",
    ),
  );
}

/**
 * Implements callback_islandora_iiif_collection_query_backends()
 */
function islandora_iiif_islandora_collection_solr_backend_callable($object, $index) {
  module_load_include('inc', 'islandora_solr', 'includes/db');
  $qp = new IslandoraSolrQueryProcessor();

  $qp->buildQuery(format_string('!member_field:("info:fedora/!pid" OR "!pid") OR !collection_member_field:("info:fedora/!pid" OR "!pid")', array(
    '!member_field' => variable_get('islandora_solr_member_of_field', 'RELS_EXT_isMemberOf_uri_ms'),
    '!pid' => $object->id,
    '!collection_member_field' => variable_get('islandora_solr_member_of_collection_field', 'RELS_EXT_isMemberOfCollection_uri_ms'),
  )), drupal_get_query_parameters());

  $limit = variable_get('islandora_iiif_solr_num_of_results', '20');

  $qp->solrStart = $index;
  $qp->solrLimit = $limit;

  if (isset($qp->internalSolrParams['sort'])) {
    $qp->solrParams['sort'] = $qp->internalSolrParams['sort'];
  }

  $qp->executeQuery(FALSE);

  // dpm($qp->islandoraSolrResult['response']);

  return array(
    $qp->islandoraSolrResult['response']['numFound'],
    $qp->islandoraSolrResult['response']['objects'],
  );
};

/**
 * This is the actual collection building function.
 */
// islandora_iiif/collection/PID/page_number
//
// Enable Collection paging if $total_children > $limit
//
// Collection paging:
// Paged collection response (API 2.1): 1 collection parent + n collection pages
// Number of Collection pages = ceil ( $total_children / $limit)
// Solr index start for page x = (x - 1) * $limit [Solr index start from 0 to total_children-1]
// page_number = NULL then collection parent
// page_number = x then collection page x
// if page_number > Number of Collection pages then drupal_not_found
// if page_number < 1 then drupal_not_found
// if page_number not numeric then drupal_not_found
//
// Collection without paging: if page_number is not null return drupal_not_found
//
// Collection empty return drupal_not_found
//
function islandora_iiif_collection_callback(AbstractObject $object, $page_from_url) {

  module_load_include('inc', 'islandora_solr', 'includes/breadcrumbs');
  if (!in_array('islandora:collectionCModel', $object->models)) {
    drupal_not_found();
  }

  	// $limit MUST be an integer
  	$limit = intval(variable_get('islandora_iiif_solr_num_of_results', '20'));

	//check page_number is NULL
	if (is_null($page_from_url)) {
		$page_number = 0;
	} else {		
		//check page_number is numeric and > 0
		if ((!is_numeric($page_from_url)) OR (intval($page_from_url) < 1) ) {
			drupal_not_found();
		} else {
			$page_number = intval($page_from_url);
		}
	}

	// We need a Solr query to get $total_children value
	// and to avoid a second query we compute supposed Solr index start for this page
	$solr_index = ($page_number == 0) ? 0 : ($page_number - 1) * $limit;
	$children_list = islandora_iiif_islandora_collection_solr_backend_callable($object, $solr_index);
	$total_children = $children_list[0];

	if ($total_children == 0) {
		drupal_not_found();
	}

	// Some more checks
	// for paging
	if ($total_children > $limit) {
		$total_pages = ceil ( $total_children / $limit);
		if ($page_number > $total_pages) {
			drupal_not_found();
		}
		$is_paging = TRUE;
	//for not paging		
	} else {
		// Here if page_number=0 means NULL in URL
		if ($page_number != 0) {
			drupal_not_found();
		}
		$is_paging = FALSE;
	}

//$index= $solr_index;

  $current_response = $children_list[1];
  $collection_members = array();
  $collection_manifests = array();
  $collection_collections = array();
  $istop = variable_get('islandora_repository_pid', 'islandora:root') == $object->id;

//  if ($children_list[0] != 0) {
    foreach ($current_response as $children_info) {
      $label = "";

      $label = $children_info['object_label'];

/** TODO: check thumbnail is correct here and
*   Elements must include @id, @type, and label (If the entry is a collection, then viewingHint must also be present.)
*
*   then ToDO: thumbnail for islandora collections and books: what DS we have to use
*
      $thumb = array(
        "@id" => urldecode(url("islandora_iiif/{$object->id}/full/50,/0/default.jp2", array(
          'absolute' => TRUE,
          'language' => (object) array('language' => FALSE),
        ))),
        "service" => array(
          "@context" => "http://iiif.io/api/image/2/context.json",
          "@id" => urldecode(url("islandora_iiif/{$children_info['PID']}/info.json", array(
            'absolute' => TRUE,
            'language' => (object) array('language' => FALSE),
          ))),
          "profile" => "http://iiif.io/api/image/2/level2.json",
        ),
      );
**/

	// an Islandora collection as sc:Collection
      if (in_array('info:fedora/islandora:collectionCModel', $children_info['content_models'])) {
        $id = urldecode(url("islandora_iiif/collection/{$children_info['PID']}", array(
          'absolute' => TRUE,
          'language' => (object) array('language' => FALSE),
        )));
        $type = "sc:Collection";
	// If the entry is a collection, then viewingHint must also be present. (API 2.1)
	$viewingHint = "multi-part";
        $collection_collections[] = array(
          "@id" => $id,
          "@type" => $type,
          "label" => $label,
	  "viewingHint" => $viewingHint,
// see above
//          "thumbnail" => $thumb,
        );
      }
	// an object as sc:Manifest
      else {
        $id = urldecode(url("islandora_iiif/{$children_info['PID']}/manifest", array(
          'absolute' => TRUE,
          'language' => (object) array('language' => FALSE),
        )));
        $type = "sc:Manifest";
        $collection_manifests[] = array(
          "@id" => $id,
          "@type" => $type,
          "label" => $label,
// see above
//          "thumbnail" => $thumb,
        );
      }
      // I now it looks weird, but API 3.o will use members and i want to respect
      // Islandora's ordering versus what 2.1 imposes, manifest v/s collection separation.

/**
* ToDO
* API 3.0 DRAFT no more members, see https://iiif.io/api/presentation/3.0/#51-collection

      $collection_members[] = array(
        "@id" => $id,
        "@type" => $type,
        "label" => $label,
        "thumbnail" => $thumb,
      );
**/

    }
//  }

	//At this point we have 3 type of response:
	//
	//A: Collection without paging
	//	$is_paging = FALSE AND page_number = 0
	//B: Collection paging parent
	//	$is_paging = TRUE AND page_number = 0
	//C: Collection page x
	//	$is_paging = TRUE AND page_number = x


  $collection = new \stdClass();
  $collection->{'@context'} = "http://iiif.io/api/presentation/2/context.json";
  $collection->{'@id'} = urldecode(url(current_path(), array(
        'absolute' => TRUE,
        'language' => (object) array('language' => FALSE),
      )
    )
  );
  $collection->{'@type'} = "sc:Collection";
  // Yes, this is ours.
  $collection->viewingHint = "multi-part";

	// NOT on Collection page x
	if ($page_number == 0) {
  		$collection->label = $object->label;
	}

  	// ONLY on Collection page x
	if ( ($is_paging) && ($page_number > 0) ) {
  		$collection->startIndex = $solr_index;
	}

	// ONLY on Collection paging parent  
	if ( ($is_paging) && ($page_number == 0) ) {
		$collection->total = (int) $total_children;

		// the paged resource must link to the first page resource, 
		// and must not include the corresponding list property.
		$first_url = "islandora_iiif/collection/{$object->id}/1";
		$collection->first = urldecode(url($first_url, array(
					'absolute' => TRUE,
					'language' => (object) array('language' => FALSE),
      					)));
	}

	// ONLY on Collection page x if not the last
	if ( ($is_paging) && ($page_number > 0) && ($page_number < $total_pages)) {
		$nextid = $page_number + 1;
		$next_url_str = "islandora_iiif/collection/{$object->id}/{$nextid}";
		$collection->next = urldecode(url($next_url_str, array(
					'absolute' => TRUE,
					'language' => (object) array('language' => FALSE),
      					)));
	}

	// ONLY on Collection page x if not the first
	if ( ($is_paging) && ($page_number > 0) && ($page_number > 1)) {
		$previd = $page_number - 1;
		$prev_url_str = "islandora_iiif/collection/{$object->id}/{$previd}";
		$collection->prev = urldecode(url($prev_url_str, array(
					'absolute' => TRUE,
					'language' => (object) array('language' => FALSE),
      					)));
	}


/**

  $next_url_str = NULL;
  $prev_url_str = NULL;

  // Few assumptiomns
  // A. If total results is less than $limit, then no $next,
  // B. if $index + $limit is more than total, then no $next
  // C. if $index - $limit <= 0 then $prev is parent top
  // if $index = 1 then no $prev.

  if (($total_children > $index + $limit) && ($total_children > 0)) {
    $nextid = $index+$limit+1;
    $next_url_str = "islandora_iiif/collection/{$object->id}/{$nextid}";
  }

  if (($index > $limit) && ($total_children > 0)) {
    $nextid = ($index-$limit+1);
    $prev_url_str = "islandora_iiif/collection/{$object->id}/{$nextid}";
  }

  if ($next_url_str) {
    $collection->next = array(
      "@id" => urldecode(url($next_url_str, array(
        'absolute' => TRUE,
        'language' => (object) array('language' => FALSE),
      ))),
    );
  }
  if ($prev_url_str) {
    $collection->prev = array(
      "@id" => urldecode(url($prev_url_str, array(
        'absolute' => TRUE,
        'language' => (object) array('language' => FALSE),
      ))),
    );
  }
**/

	// NOT on Collection page x
	if ($page_number == 0) {
		  if (!$istop) {
		    $parent = islandora_solr_get_breadcrumb_parent($object->id);
		    if (!empty($parent)) {
		      $collection->within = urldecode(url("islandora_iiif/collection/{$parent['PID']}", array(
			  'absolute' => TRUE,
			  'language' => (object) array('language' => FALSE),
			)));
		    }
		  }
	} else {
	// on Collection page x 
		$collection->within = urldecode(url("islandora_iiif/collection/{$object->id}", array(
			'absolute' => TRUE,
			'language' => (object) array('language' => FALSE),
			)));
	}

	// on Collection without paging or Collection page x
	if ((!($is_paging)) || (($is_paging) && ($page_number > 0))) {
	  	if (!empty($collection_collections)) {
	    		$collection->collections = $collection_collections;
	  	}
  		if (!empty($collection_manifests)) {
    			$collection->manifests = $collection_manifests;
  		}
	}


/** see above
  if (!empty($collection_members)) {
    $collection->members = $collection_members;
  }
**/

  // And to make this legit we need a sequence with some canvases.
  drupal_add_http_header('Content-Type', 'application/json; charset=utf-8');
  drupal_add_http_header('access-control-allow-origin', '*');
  echo json_encode($collection, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE | JSON_NUMERIC_CHECK);
  drupal_page_footer();

}
