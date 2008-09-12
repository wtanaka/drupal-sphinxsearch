;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
;; Sphinx search module for Drupal 5.x
;; $Id: README.txt,v 1.4 2008/09/12 02:44:22 markuspetrux Exp $
;;
;; Original author: markus_petrux at drupal.org (July 2008)
;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

REQUIREMENTS
============

  - PHP 4.4.x or PHP 5.x (PHP needs to be compiled with --enable-memory-limit).
  - Sphinx 0.9.8 (shell access is required here).


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

     # optionally, download and untar libstemmer.
     wget http://snowball.tartarus.org/dist/libstemmer_c.tgz
     tar xzf libstemmer_c.tgz

     # you may need to adjust file ownerships.
     chown -R root.root *

     # build, compile and install sphinx + libstemmer.
     ./configure --with-mysql --with-libstemmer --prefix=/usr/local/sphinx
     make
     make install


  2) See sphinxsearch/contrib subdirectory. It contains samples for sphinx.conf
     and sphinx start/stop script.

     ***** IMPORTANT *****
     Files in contrib subdirectory are just samples. Please, note they are
     provided in order to help you setup your Sphinx installation, but without
     warranties of any kind. Note that I started to learn it just recently.
     Also, my environment and needs may differ a lot from yours. Please, don't
     use them as-is. If you do, it is at your own risk.
     *********************


  3) Install sphinxsearch Drupal module.

     - Copy package contents to modules/sphinxsearch.
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


  4) Customization.

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


  - Steps to create your initial set of indexes:

    It is assumed that your sphinxsearch module has been installed and
    configured, also that you have already installed and configured your
    Sphinx server accordingly.

    1) Stop your searchd daemon.
    2) Use Sphinx indexer to build all your main indexes.
    3) Start your searchd daemon.
    4) Setup cron task to rebuild your delta index at short intervals.
    5) Setup cron task to rebuild your main indexes once a day or so.

    Once your initial set of indexes is created, you don't need to stop
    your searchd daemon. Instead, you can invoke Sphinx indexer with
    --rotate argument.

    See docs/contrib subdirectory of this package for sample script.


  - Troubleshooting:

    Symptom: When creating your initial set of main/delta indexes, you may
    endup with index file names with ".new" in them. Often, Sphinx searchd
    daemon deals with this naming convention transparently. However, it may
    sometimes fail to recognise these files. Not exactly sure why, though.
    Solution: Stop searchd daemon and rename you index files to remove
    the ".new" part. ie. if you see something like "main.new.spp", you can
    rename it to "main.spp". Note than each Sphinx index uses several files
    with same name and different extension. Start again searchd daemon when
    all files have been renamed.



;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

SPHINX IMPLEMENTATION DETAILS
=============================

- Sphinx is a fast and scalable full-text search engine. However, it currently
  has a few limitations related to the way text is indexed.

- Sphinx index documents that are composed of fields of different types. It
  basically supports text fields, integers, timestamps, booleans, multi-valued
  attributes (lists of integers that can be used to implement 1-N relations),
  etc. For instance, to manage basic Drupal content (nodes) we can use text
  fields to index titles and bodies, an integer field to store the node author
  id, a timestamp field to store last updated time, a boolean field to store
  the is_deleted attribute (we'll see this later) or multi-valued attributes
  field to store the list of terms related to each node.

- Current version of Sphinx does not support full live index updates. Instead,
  it is necessary to build indexes in jobs that transform your data into a
  special kind of documents that are stored in Sphinx indexes. This process is
  executed by the Sphinx indexer command and it should be invoked from the
  server where Sphinx is installed. A particular Sphinx installation can manage
  a number of indexes, you can partition indexes managed locally, or even
  remotely from other Sphinx servers. You can install your Sphinx server on a
  dedicated server (recommended) or it can coexist in one server with any other
  software of your choice.

- Each Sphinx instance is configured with its own sphinx.conf file where you
  can specify how your indexes are built, structure of your Sphinx documents,
  how your data should be extracted to build them, as well as options that tell
  Sphinx how the searchd daemon should work. The searchd daemon can be
  configured to listen on a particular TCP port of the server. Then, Sphinx
  provides a series of APIs that can be used to connect your application to the
  searchd daemon (locally or remotely) to perform search queries, retrieve
  results, build excerpts highlighting keywords, or even update some kind of
  attributes. However, it is not possible to index new documents, it is not
  possible to update text fields and it is not possible to delete indexed
  documents. It is only possible to update non-text fields.

- Therefore, it is necessary to create indexes in batch jobs. These jobs will
  index all your content at once, and it is necessary to repeat this task
  periodically in order to recover space used by documents marked as being
  deleted, index new documents and/or reindex documents that have been updated
  since last time indexes were built.

- A note on document deletions. This module creates Sphinx documents with a
  boolean attribute, is_deleted, that is used as a flag to keep track of
  nodes that have been deleted from Drupal database, but that still exist
  in Sphinx indexes. When a node is indexed, its own is_deleted attribute is
  set to 0. When a node is deleted from the Drupal database, the Sphinx API is
  used to set the is_deleted attribute of that node to 1. Finally, all search
  queries sent by this module filter out documents with this attribute enabled.
  This method allows us to tell Sphinx supports live document deletions, but
  as you can see this is not the case.

- In this scenario, we need to work in Sphinx with the so called main + delta
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
  generates full reports in watchdog to help you monitor index processing.

- Sphinx also supports distributed indexes. This type of indexes can be used
  to join a number of indexes that share exact same structure. In this case,
  we join as many main indexes as we may need, plus the delta index. In case a
  document is stored in more than one index, the one stored in the last index
  in the list "wins". Joined indexes can be local (managed by the same Sphinx
  instance) or remote. This is great in terms of scalability. In fact, this
  means we can split the index rebuild process in chunks that can be easily
  managed, or even spread to other servers in your infrastructure. Queries sent
  to distributed indexes are resolved by Sphinx transparently, as if it was a
  single index.

- Data sources to build Sphinx indexes can be of type MySQL, PostgreSQL and
  XMLPipe. In the case of MySQL or PostgreSQL source types it is possible to
  tell Sphinx indexer to extract data directly from your database, and this
  method is impressively fast. However, these methods cannot be used to index
  Drupal nodes, or at least it would be so difficult to achieve, because data
  related to nodes often needs to be proprocessed by a number of hooks that may
  involve a lot of small and quick (or not so quick) SQL queries and further
  processing performed by core modules as well as contrib modules.
  For instance, XMLPipe is the only method that allows us to index nodes along
  with their comments, cck fields, taxonomy terms, etc. In fact, this method
  allows us to index content the same way Drupal core search works.

- It is something important to take into account that XMLPipe generation may
  require more resources than what one would expect at first, compared to other
  Sphinx implementations. It all depends on the complexity of your Drupal
  intallation, modules installed, size and number of nodes, available
  infraestructure, etc. Note that Drupal search core solves this problem by
  splitting index generation in chunks where a number of nodes is indexed at
  cron intervals, however with Sphinx we need to index all content at once. Of
  course, it is also possible to partition indexes so your nodes are spread
  into several storage units, though this method might only be recommended when
  your site has thousands of nodes, maybe millions. Again, it all depends on
  the time it takes to create your indexes, which may be from a few minutes up
  to one or more hours.

- So here's why this module is based on and supports XMLPipe index type
  generation. Problem is now, this method is MUCH slower than indexing content
  using MySQL/PostgreSQL index types. You may wish now take a look at the
  docs subdirectory of this project to see the options this module provides
  to help you setup and manage your index creation jobs, etc.

- In order to minimize these problems, the XMLPipe generation script provided
  with this module implements a few checks that will abort XMLPipe stream
  generation and report the cause of the problem to watchdog. Actually, the
  module monitors memory usage and execution time in order to prevent crashes
  when PHP memory_limit and/or max_execution_time values are exceeded.
  Depending on module settings, it is also possible to setup the XMLPipe
  generation script to restart client connection to DB server to prevent from
  getting max connection time problems. You may also wish to adjust PHP
  settings from the .htaccess file provided within the sphinxsearch_scripts
  subdirectory of this module.

- Here's a couple of examples where I have implemented Sphinx, so you can get
  an idea of how many time it may take to process your indexes, and/or a sample
  reference on how to setup your Sphinx installation.

  a) phpBB based board with 14+ million posts, 15,000 posts a day average, and
     growing. Here, I used 4 main indexes with capacity for 5 million posts
     each, and one delta index. Generation of each main index takes around 1
     hour. 1 or 2 main indexes are built daily. Generation of delta index just
     takes seconds and it is scheduled to run at 1 minute intevals from cron.
     If you wish, you can test Sphinx search engine implemented on this site
     from here: http://zonaforo.meristation.com/foros/search.php

  b) Drupal based site running this module. Site has 10,000+ blog entries and
     30,000+ comments. It uses 1 main index + 1 delta. Main index takes less
     than 5 minutes to build and it is executed daily. Delta index takes a few
     seconds and it is executed at 5 minutes intervals from cron. Again, if you
     wish, you can test Sphinx search engine implemented on this site from
     here: http://blogs.gamefilia.com/search

  It all depends on several factors. Of course, your mileage may vary.

- New or different ideas to fight against forementioned "limitations" are
  welcome. Please, use issue tracker of the module.


;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;

TODO
====

- Provide new hooks to allow external modules extend Sphinx document attributes
  and/or alter search user interface with additional filters.

- Think about a reasonable method to implement access control to indexed data.
  Currently, all content indexed by this module is available to anyone with
  'use sphinxsearch' permission.

- Provide a better integration / user interface to co-exist with other search
  modules that may provide solutions for searching different kinds of content,
  such as users, etc. Suggestions are welcome. However, I believe this is more
  a job for Drupal search framework itself. Hopefully Sphinx search integration
  provided with this module can be used as proof of concept of Sphinx
  capabilities and limitations that maybe can help here in some way...
