#!/usr/bin/env python3
# -*- coding: utf-8 -*-
#
# pg_auto_failover documentation build configuration file, created by
# sphinx-quickstart on Sat May  5 14:33:23 2018.
#
# This file is execfile()d with the current directory set to its
# containing dir.
#
# Note that not all possible configuration values are present in this
# autogenerated file.
#
# All configuration values have a default; values that are commented out
# serve to show the default.

# If extensions (or modules to document with autodoc) are in another directory,
# add these directories to sys.path here. If the directory is relative to the
# documentation root, use os.path.abspath to make it absolute, like shown here.
#
# import os
# import sys
# sys.path.insert(0, os.path.abspath('.'))

#
# https://stackoverflow.com/questions/9899283/how-do-you-change-the-code-example-font-size-in-latex-pdf-output-with-sphinx
#

from sphinx.highlighting import PygmentsBridge
from pygments.formatters.latex import LatexFormatter


class CustomLatexFormatter(LatexFormatter):
    def __init__(self, **options):
        super(CustomLatexFormatter, self).__init__(**options)
        self.verboptions = r"formatcom=\scriptsize"


PygmentsBridge.latex_formatter = CustomLatexFormatter


# -- General configuration ------------------------------------------------

# If your documentation needs a minimal Sphinx version, state it here.
#
# needs_sphinx = '1.0'

# Add any Sphinx extension module names here, as strings. They can be
# extensions coming with Sphinx (named 'sphinx.ext.*') or your custom
# ones.
extensions = ["sphinx.ext.githubpages"]

# Add any paths that contain templates here, relative to this directory.
templates_path = ["_templates"]

# The suffix(es) of source filenames.
# You can specify multiple suffix as a list of string:
#
# source_suffix = ['.rst', '.md']
source_suffix = ".rst"

# The master toctree document.
master_doc = "index"

# General information about the project.
project = "pg_auto_failover"
copyright = "Copyright (c) Microsoft Corporation. All rights reserved."
author = "Microsoft"

# The version info for the project you're documenting, acts as replacement for
# |version| and |release|, also used in various other places throughout the
# built documents.
#
# The short X.Y version.
version = "1.5"
# The full version, including alpha/beta/rc tags.
release = "1.5.1"

# The language for content autogenerated by Sphinx. Refer to documentation
# for a list of supported languages.
#
# This is also used if you do content translation via gettext catalogs.
# Usually you set "language" from the command line for these cases.
language = None

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
# This patterns also effect to html_static_path and html_extra_path
exclude_patterns = ["_build", "Thumbs.db", ".DS_Store", ".venv"]

# The name of the Pygments (syntax highlighting) style to use.
pygments_style = "sphinx"

# If true, `todo` and `todoList` produce output, else they produce nothing.
todo_include_todos = False


# -- Options for HTML output ----------------------------------------------

# The theme to use for HTML and HTML Help pages.  See the documentation for
# a list of builtin themes.
#
html_theme = "sphinx_rtd_theme"

# Theme options are theme-specific and customize the look and feel of a theme
# further.  For a list of options available for each theme, see the
# documentation.
#
# html_theme_options = {}

# Add our custom CSS
def setup(app):
  app.add_stylesheet('css/citus.css')
  app.add_stylesheet('css/pygments.css')

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ["_static"]

# Custom sidebar templates, must be a dictionary that maps document names
# to template names.
#
# This is required for the alabaster theme
# refs: http://alabaster.readthedocs.io/en/latest/installation.html#sidebars
html_sidebars = {
    "**": [
        "relations.html",  # needs 'show_related': True theme option to display
        "searchbox.html",
    ]
}


# -- Options for HTMLHelp output ------------------------------------------

# Output file base name for HTML help builder.
htmlhelp_basename = "pg_auto_failoverdoc"


# -- Options for LaTeX output ---------------------------------------------

# Grouping the document tree into LaTeX files. List of tuples
# (source start file, target name, title,
#  author, documentclass [howto, manual, or own class]).
latex_documents = [
    (
        master_doc,
        "pg_auto_failover.tex",
        "pg\_auto\_failover Documentation",
        "Microsoft",
        "manual",
    ),
]

# for some reasons, doesn't work with the current setup.
# latex_engine = 'xelatex'

latex_elements = {
    "geometry": r"\usepackage[paperwidth=441pt,paperheight=666pt]{geometry}",
    "pointsize": "12pt",
    "fncychap": r"\usepackage[Sonny]{fncychap}",
    "extraclassoptions": "oneside",
    "sphinxsetup": "hmargin=1cm,vmargin=2cm,verbatimwithframe=false,VerbatimColor={rgb}{1.0, 0.97, 0.97}",
}

latex_show_urls = "footnote"

# -- Options for manual page output ---------------------------------------

# One entry per manual page. List of tuples
# (source start file, name, description, authors, manual section).
man_pages = [
    (
        master_doc,
        "pg_auto_failover",
        "pg_auto_failover Documentation",
        [author],
        1,
    ),
    ("ref/pg_autoctl", "pg_autoctl", "pg_autoctl", [author], 1),
    (
        "ref/pg_autoctl_create_monitor",
        "pg_autoctl create monitor",
        "pg_autoctl create monitor",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_create_postgres",
        "pg_autoctl create postgres",
        "pg_autoctl create postgres",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_create_formation",
        "pg_autoctl create formation",
        "pg_autoctl create formation",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_drop_monitor",
        "pg_autoctl drop monitor",
        "pg_autoctl drop monitor",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_drop_node",
        "pg_autoctl drop node",
        "pg_autoctl drop node",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_drop_formation",
        "pg_autoctl drop formation",
        "pg_autoctl drop formation",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_config_get",
        "pg_autoctl config get",
        "pg_autoctl config get",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_config_set",
        "pg_autoctl config set",
        "pg_autoctl config set",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_config_check",
        "pg_autoctl config check",
        "pg_autoctl config check",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_show_uri",
        "pg_autoctl show uri",
        "pg_autoctl show uri",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_show_events",
        "pg_autoctl show events",
        "pg_autoctl show events",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_show_state",
        "pg_autoctl show state",
        "pg_autoctl show state",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_show_file",
        "pg_autoctl show file",
        "pg_autoctl show file",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_show_settings",
        "pg_autoctl show settings",
        "pg_autoctl show settings",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_show_standby_names",
        "pg_autoctl show standby-names",
        "pg_autoctl show standby-names",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_enable_maintenance",
        "pg_autoctl enable maintenance",
        "pg_autoctl enable maintenance",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_enable_secondary",
        "pg_autoctl enable secondary",
        "pg_autoctl enable secondary",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_enable_ssl",
        "pg_autoctl enable ssl",
        "pg_autoctl enable ssl",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_enable_monitor",
        "pg_autoctl enable monitor",
        "pg_autoctl enable monitor",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_disable_maintenance",
        "pg_autoctl disable maintenance",
        "pg_autoctl disable maintenance",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_disable_secondary",
        "pg_autoctl disable secondary",
        "pg_autoctl disable secondary",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_disable_ssl",
        "pg_autoctl disable ssl",
        "pg_autoctl disable ssl",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_disable_monitor",
        "pg_autoctl disable monitor",
        "pg_autoctl disable monitor",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_get_formation_settings",
        "pg_autoctl get formation settings",
        "pg_autoctl get formation settings",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_get_formation_number_sync_standbys",
        "pg_autoctl get formation number-sync-standbys",
        "pg_autoctl get formation number-sync-standbys",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_get_node_replication_quorum",
        "pg_autoctl get node replication-quorum",
        "pg_autoctl get node replication-quorum",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_get_node_candidate_priority",
        "pg_autoctl get node candidate-priority",
        "pg_autoctl get node candidate-priority",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_set_formation_number_sync_standbys",
        "pg_autoctl set formation number-sync-standbys",
        "pg_autoctl set formation number-sync-standbys",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_set_node_replication_quorum",
        "pg_autoctl set node replication-quorum",
        "pg_autoctl set node replication-quorum",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_set_node_candidate_priority",
        "pg_autoctl set node candidate-priority",
        "pg_autoctl set node candidate-priority",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_perform_failover",
        "pg_autoctl perform failover",
        "pg_autoctl perform failover",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_perform_switchover",
        "pg_autoctl perform switchover",
        "pg_autoctl perform switchover",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_perform_promotion",
        "pg_autoctl perform promotion",
        "pg_autoctl perform promotion",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_run",
        "pg_autoctl run",
        "pg_autoctl run",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_stop",
        "pg_autoctl stop",
        "pg_autoctl stop",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_status",
        "pg_autoctl status",
        "pg_autoctl status",
        [author],
        1,
    ),
    (
        "ref/pg_autoctl_reload",
        "pg_autoctl reload",
        "pg_autoctl reload",
        [author],
        1,
    ),
    # ("ref/reference", "pg_autoctl", "pg_auto_failover agent", [author], 1),
    (
        "ref/configuration",
        "pg_autoctl",
        "pg_auto_failover Configuration",
        [author],
        5,
    ),
]


# -- Options for Texinfo output -------------------------------------------

# Grouping the document tree into Texinfo files. List of tuples
# (source start file, target name, title, author,
#  dir menu entry, description, category)
texinfo_documents = [
    (
        master_doc,
        "pg_auto_failover",
        "pg_auto_failover Documentation",
        author,
        "pg_auto_failover",
        "One line description of project.",
        "Miscellaneous",
    ),
]
