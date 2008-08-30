================================================================================
$Id: README-XMLPIPE.txt,v 1.2 2008/08/30 06:53:32 markuspetrux Exp $
================================================================================

The sphinxsearch_scripts subdirectory under the sphinxsearch module contains the
script that is used to generate XMLPipe sources for your Sphinx indexes.

INSTALLATION
============

The sphinxsearch_scripts subdirectory should be moved to the root directory of
your Drupal installation. You may wish to setup a symbolic link, however, which
is preferred as you won't need to repeat this task over and over again when
updating the sphinxsearch module.

Examples below assume:
  1) Your Drupal root is installed at /path-to-drupal-root
  2) sphinxsearch module is installed at /path-to-drupal-modules/sphinxsearch

- Example 1: Copy sphinxsearch_scripts directory.

  cp -R \
    /path-to-drupal-modules/sphinxsearch/sphinxsearch_scripts \
    /path-to-drupal-root


- Example 2: Create a symbolic link for sphinxsearch_scripts directory.

  ln -s \
    /path-to-drupal-modules/sphinxsearch/sphinxsearch_scripts \
    /path-to-drupal-root/sphinxsearch_scripts



USAGE
=====

Once your sphinxsearch module has been installed, you can setup your Sphinx
sources in xmlpipe mode and setup the corresponding xmlpipe command to use the
URL to the sphinxsearch_xmlpipe.php script as described below.

Examples assume the URL to your Drupal installation is http://www.example.com/

Note that the amount of nodes used in these examples may vary for your
particular installation depending on the time required to generate each index.
Generation of Sphinx indexes may take from several seconds to one hour or more,
depending on the complexity of your Drupal installation, hardware, etc.

- Example 1: Site with small number of nodes, say less than 10000.

  - You may just need to setup your sphinx.conf with 1 main + 1 delta indexes.
    xmlpipe commands for these indexes would look like:

    http://www.example.com/sphinxsearch_scripts/sphinxsearch_xmlpipe.php?mode=main
    http://www.example.com/sphinxsearch_scripts/sphinxsearch_xmlpipe.php?mode=delta

  - Your main index would hold documents for all existing nodes at the time
    index was created.

  - Your delta index would hold documents for all new or updated nodes since
    last time your main index was created.

  - You may wish to rebuild your main index from time to time. For instance,
    once a day might be a good option to start with.

  - Delta index could be rebuilt every 5 minutes or so, depending on the time
    required for the process, and/or the number of new/updated nodes per period.


- Example 2: Site with more than 200000 nodes.

  - You may need to setup your sphinx.conf with 3 main + 1 delta indexes. Each
    main index would have capacity for at least 100000 documents. xmlpipe
    commands for these indexes would look like:

    http://www.example.com/sphinxsearch_scripts/sphinxsearch_xmlpipe.php?mode=main&id=0&first_nid=0&last_nid=99999
    http://www.example.com/sphinxsearch_scripts/sphinxsearch_xmlpipe.php?mode=main&id=1&first_nid=100000&last_nid=199999
    http://www.example.com/sphinxsearch_scripts/sphinxsearch_xmlpipe.php?mode=main&id=2&first_nid=200000
    http://www.example.com/sphinxsearch_scripts/sphinxsearch_xmlpipe.php?mode=delta

  - Your main indexes would hold documents for all existing nodes within
    specified range at the time each main index was created.

  - Last main index would hold documents from nid 200000 up to the last node in
    your site. You may need to add more main indexes as your site grows.

  - Your delta index would hold documents for all new or updated nodes since
    last time your main indexes were created. Note that you don't need a delta
    index for each main index. One single delta index is enough.

  - You may wish to rebuild your main indexes from time to time. For instance,
    once a day might be a good option to start with. However, if the process
    takes a long time, then you may wish to rebuild just one main index a day.

  - Delta index could still be rebuilt every 5 minutes or so, depending on the
    time required for the process, and/or the number of new/updated nodes per
    period.

Please, check out the sample sphinx.conf provided in the docs/contrib
subdirectory of this package for further information and examples on how to
setup your main + delta index scheme.
