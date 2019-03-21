# Islandora IIIF

Version IIIF Presentation API 2.1.x.
This is still experimental and we (Diego Pino and Giancarlo Birello) don't recommend running it in production. 
Still, it is very cool =)

ToDO: update readme
==================================

## Requirements

This module requires the following modules/libraries:

* [Islandora](https://github.com/islandora/islandora)
* [Islandora Collection](https://github.com/islandora/islandora_solution_pack_collection)
* [Islandora Solr Search ](https://github.com/islandora/islandora_solr_search)
* [Islandora Solr Metadatata ](https://github.com/Islandora/islandora_solr_metadata)

This module also requires a working IIIF server API image (i.e. Cantaloupe), the same you are already running for OpenSeadragon and Internet Archive Bookreader viewers.

This module does not require (kind-of really) but recommends having also installed
* [Islandora Paged ](https://github.com/islandora/islandora_paged)

## Installation

Install as usual, see [this](https://drupal.org/documentation/install/modules-themes/modules-7) for further information.


## Documentation

It provides responses for all IIIF Presentation API 2.1 types

In the form of:

http://localhost:8000/islandora_iiif/islandora%3A1/info.json

http://localhost:8000/islandora_iiif/islandora%3A1/manifest

http://localhost:8000/islandora_iiif/islandora%3A1/sequence/default

http://localhost:8000/islandora_iiif/islandora%3A1/canvas/default

http://localhost:8000/islandora_iiif/islandora%3A1/annotation/default


To check
http://localhost:8000/islandora_iiif/collection/islandora%3A1
