;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Sphinx search module for Drupal 5.x
;; $Id: README.txt,v 1.2 2008/08/18 15:13:44 markuspetrux Exp $
;;
;; Original author: markus_petrux at drupal.org (July 2008)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

REQUIREMENTS
============

  - Drupal 5.x (planned port to D6)
  - PHP 4.4.x or PHP 5.x (PHP needs to be compiled with --enable-memory-limit).
  - It should work for any DB engine supported by Drupal.
  - Sphinx 0.9.8
  - Shell access to the box where Sphinx is installed.


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

INSTALLATION
============

  1) Install Sphinx.

     It is recommended to install Sphinx on separate box, but it may also work
     on any other server of your farm, or even in the same box your web server,
     mysql or whatever is installed.

     For more details, additional requirements, etc. please, read Sphinx
     documentation. Here's just a quick start guide. You need root access
     to the box.

     # move to a temp directory.
     cd /opt
     # download and untar Sphinx source.
     wget http://www.sphinxsearch.com/downloads/sphinx-0.9.8.tar.gz
     tar xzf sphinx-0.9.8.tar.gz
     cd sphinx-0.9.8
     # download and untar libstemmer.
     wget http://snowball.tartarus.org/dist/libstemmer_c.tgz
     tar xzf libstemmer_c.tgz
     # you may need to adjust file ownerships.
     chown -R root.root *
     # build, compile and install sphinx + libstemmer.
     ./configure --with-mysql --with-libstemmer --prefix=/usr/local/sphinx
     make
     make install


  2) See sphinxsearch/contrib subdirectory contains samples for sphinx.conf
     and sphinx start/stop script.

     ***** IMPORTANT *****
     Files in contrib subdirectory are just samples. Please, note they are
     provided in order to help you setup your Sphinx installation, but without
     warranties of any kind. Note that I started to learn it just recently.
     Also, my environment and needs may differ a lot from yours. Please, don't
     use them as-is. If you do, it is at your own risk.
     *********************

  3) Install sphinxsearch Drupal module as usual.

     - Copy to modules/sphinxsearch all files and directories.
     - Copy sphinxsearch_scripts subdirectory provided within this module to
       your Drupal root directory.
       Instead, you may wish to setup a symbolic link from your Drupal root to
       the sphinxsearch_scripts subdirectory of this module. This way you don't
       need to copy files when module is updated. Please, see README-XMLPIPE.txt
       for further information and examples.
     - Goto admin/build/modules to install the module.
     - Goto admin/user/access to adjust permissions.
         (use sphinxsearch, administer sphinxsearch)
     - Goto admin/settings/sphinxsearch to configure module options.
         (see below)

  4) Customization:

     - Check module settings and adjust them to your environment.
     - Create and/or adjust your sphinx.conf to include definitions for all
       indexes required by your Drupal site. You need at least one main index,
       optionally as many main indexes as you need, and also optionaly one
       single delta index.
       It is also necessary to create a distributed index that will be used
       to join all your indexes when resolving search queries.
         (see contrib subdirectory for examples and further information).
     - Setup crontab to build your main and delta indexes at intervals.

     ***** IMPORTANT *****
     There are options in the module settings panel that require you to
     rebuild main indexes. Otherwise, you may get errors when searching.
     *********************

  - Watchdog logging:

    XMLPipe processing generates watchdog records with information on memory
    used, execution time, nodes processed, etc., to help you adjust module
    settings to suit your needs.


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

SPHINX INDEXES
==============

- This module supports XMLPipe index type generation so we can index
  preprocessed node content, which allow us to pass on to Sphinx text generated
  by any kind of Drupal module. ie. node itself, but also comments, taxonomies,
  CCK, etc. However, it is MUCH slower than indexing content using mysql/pgsql
  Sphinx index types.

- At this time, Sphinx does not support index additions or deletions in real
  time. It only supports updates of non-text attributes. This method is used to
  updated the 'is_delete' attribute of Sphinx documents when a node is deleted.
  All search queries sent by this module filters nodes with 'is_delete'
  attribute set to 1, so this allows us to emmulate Sphinx document deletions.

- In this scenario, we need to work in Sphinx with the so called main+delta
  scheme. See Sphinx documentation for more details. In short, main indexes
  should be rebuilt periodically in order to recover space used by deleted
  documents, and delta index should be rebuild as often as possible to take
  care of new and updates documents until your main indexes are rebuilt.

- Once you have created your main indexes, new and/or updated nodes will be
  stored in delta index. You may wish to rebuild your delta index at short
  intervals using crontab from the server where Sphinx has been installed.
  These intervals basically depend on the time required to process each delta
  and the number of node updates in your site. You may wish to start with 5
  minutes and adjust your crontab as you get more experience. The module
  generate full reports in watchdog to help you monitor index processing.

- I had a lot of problems related to resource consumption in Drupal while
  generating the XMLPipe stream used by Sphinx to build indexes. It all depends
  on the complexity of your Drupal intallation, modules installed, overall size
  of your nodes, available infraestructure, etc.
  In order to minimize these problems, I implemented in the XMLPipe generation
  script a few checks that will abort XMLPipe stream generation and report the
  cause of the problem in watchdog. Depending on module settings, it is also
  possible to setup the XMLPipe generation script to restart DB server to
  prevent from getting max connection time problems (see module settings).
  However, you may wish to adjust PHP settings from the .htaccess file provided
  within the sphinxsearch_scripts subdirectory of this module.

- New or different ideas to fight against "limitations" related to XMLPipe
  method, Sphinx index maintenance, etc. are welcome.
  Please, use issue tracker of the module.


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

TODO
====

In no particular order...

- Re-work the way search matching methods are implemented. There might be
  options for users to select between "search all", "search any",
  "search phrase" or "boolean search".

- Provide new hooks to allow external modules extend Sphinx document attributes
  and/or alter search user interface with additional filters.

- Use Sphinx grouping capabilities to generate tagadelic page and blocks.

- Explore the possibilities to provide some level of faceted search.

- Provide a better integration / user interface to co-exist with other search
  modules that may provide solutions for searching different kinds of content,
  such as users, etc. Suggestions are welcome.
