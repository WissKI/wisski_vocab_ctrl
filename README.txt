
module wisski_vocab_ctrl
=============================

This module allows you to define and query controlled vocabularies
(or name authorities). The vocabularies' entries are RDF instances and may be
linked to. If an entry is linked to it will be imported into the system's
triple store. Ie. the instance along with the defined fields (see later) are
imported according to the definitions. Thus the entry can be accessed like any
other instance created within the system.

A vocabulary is defined by
1. an accesspoint (see module wisski_accesspoint) which serves as the data pool
for the vocab. 
2. an import group which is a top group defined by the Pathbuilder (see
module wisski_pathbuilder). The entries will be treated in the system as of
this group.
3. field definitions, which consist of a mapping of queries for the accesspoint
to fields and from fields to paths of the import group.
The mapping to fields allows for specifying special meaning or treatment within
the system. E.g. the field 'label' stands for the label of an entry similar to 
skos:prefLabel. Entries may be queried primarily through 'label', hence it is 
obligatory for all vocabs. See below or tab Fields for defined fields and
explanations.
Queries for accesspoints may be defined in tab Mappings. There are predefined
queries for SKOS properties and some of the WGS84 vocabulary.

Special fields:
'Label': the prefered label of the entry. Should be unique. Normally you will
search for an entry by label. The only obligatory field.
'Alternate Label': other labels. If there is a mapping, this field is also used
when searching entries by label.
'broader', 'narrower': link to other entries with the meaning of 
broader/narrower term. These properties will be used to build the hierarchy of
broader labels used in many places.
'latitude', 'longitude': Provide coordinate information for places. Usage
examples are the module wisski_editor which uses them for showing a map in the
tooltips and the module wisski_textproc which ranks places according to 
coordinate info.


installation instructions and setup
=============================

1. Copy the module files into Drupal's sites/all/modules directory.
2. Activate the module.
3. Define the vocabularies under Site configuration -> Wisski Module Settings
-> Vocabulary Control (admin/settings/wisski/vocab_ctrl). For each vocabulary
that you want to define do the following steps:
3a. Select Add, choose a name
3b. Select the AccessPoint that will serve as data pool for your vocabulary
3c. Select the import group. You can choose among the groups defined in the
wisski_pathbuilder module. If you link to an entry in this vocabulary, it will
be treated as being of this group.
3d. Specify the field mappings. You must at least specify a mapping for
'label'. If you select '<none>' for the query pattern, the same query as for 
the import path is used. This is specifically/only useful when the vocabulary
is backed by the local accesspoint or triple data that uses the same ontology.
4. It is useful to set up a vocabulary backed by the local accesspoint for each
top group.

