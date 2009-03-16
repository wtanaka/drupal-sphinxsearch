#!/bin/sh
git cvsimport -v \
 -d:pserver:anonymous@cvs.drupal.org:/cvs/drupal-contrib \
 -r officialcvs contributions/modules/sphinxsearch
