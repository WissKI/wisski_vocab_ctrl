<?php


/* Most functions here are docuemnted in module file.
*
*@author Martin Scholz
*/


/*============================================ *
*  VOCABULARIES
* ============================================ */



function _wisski_vocab_ctrl_get($vid = NULL, $col = NULL, $refresh = FALSE) {
  static $vocs = array(), $all = FALSE;
  
  if ($refresh) {
    $vocs = array();
    $all = FALSE;
  }

  if (!$all && (!$vid || ($vid && !isset($vocs[$vid])))) {

    $result = db_query("SELECT * FROM {wisski_vocab_ctrl_vocabularies}" . (($vid !== NULL) ? " WHERE vid = %d" : ""), $vid);

    while ($voc = db_fetch_object($result)) {
      if (isset($vocs[$voc->vid])) continue;
      $voc->indices = unserialize($voc->indices);
      $voc->store = wisski_accesspoint_get($voc->aid)->endpoint;
      $voc->fields = unserialize($voc->fields);
      $vocs[$voc->vid] = $voc;
    }
    
    if (!$vid) {
      // all vocab loaded, sort by priority
      uasort($vocs, function($a, $b) { return $b->priority - $a->priority; });
      $all = TRUE;
    }

  }
  
  if (!$vid) {
    return $vocs;
  } elseif (!isset($vocs[$vid])) {
    return NULL;
  } elseif (!$col) {
    return $vocs[$vid];
  } elseif (!isset($vocs[$vid]->$col)) {
    return NULL;
  } else {
    return $vocs[$vid]->$col;
  }

}


function _wisski_vocab_ctrl_save(&$voc) {

  if ($voc->vid === NULL) {
    // insert
    drupal_write_record('wisski_vocab_ctrl_vocabularies', $voc);
  } else {
    // update
    drupal_write_record('wisski_vocab_ctrl_vocabularies', $voc, 'vid');
  }

}


function _wisski_vocab_ctrl_delete($vid) {
  
  if (is_object($vid)) $vid = $vid->vid;
  db_query('DELETE FROM {wisski_vocab_ctrl_vocabularies} WHERE vid = %d', $vid);

}


function _wisski_vocab_ctrl_is_imported($vid, $term_uri) {
  
  // if no voc is given, we make a list of all vocs that imported this instance
  if (!$vid) {
    $vocs = wisski_vocab_ctrl_get();
    $contains = array();
    foreach ($vocs as $voc) {
      if (_wisski_vocab_ctrl_is_imported($voc, $term_uri)) $contains[] = $voc->vid;
    }
    return $contains;
  }

  if (!is_object($vid)) {
    $voc = wisski_vocab_ctrl_get($vid);
  } else {
    $voc = $vid;
  }
  if (!$voc) return array();
  
  include_once(drupal_get_path('module', 'wisski_pathbuilder') . '/wisski_pathbuilder.inc');
  $group_path_array = _wisski_pathbuilder_calculate_group_samepart($voc->group_id);
  
  $q = "ASK { <$term_uri> <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <" . $group_path_array['x0'] . '> . }';
  $store = wisski_store_getObj()->wisski_ARCAdapter_getStore();
  $contains = $store->query($q, 'raw');
  if ($store->getErrors()) {
    foreach ($store->getErrors() as $e) drupal_set_message("error asking TS: $e");
  }

  return $contains;
  
}


function _wisski_vocab_ctrl_get_triples($vid, $term_uri) {
  global $base_root, $base_path;

  if (!$vid || !$term_uri) return FALSE;
  
  if (!is_object($vid)) {
    $voc = wisski_vocab_ctrl_get($vid);
  } else {
    $voc = $vid;
  }
  if (!$voc) return array();
  
  if (_wisski_vocab_ctrl_is_imported($voc, $term_uri)) {
    // the triples were already stored!
    return array();
  }

  include_once(drupal_get_path('module', 'wisski_pathbuilder') . '/wisski_pathbuilder.inc');
  $group_path_array = _wisski_pathbuilder_calculate_group_samepart($voc->group_id);
  
  $triples = array(
    array(
      's' => $term_uri,
      's type' => 'uri',
      'p' => 'http://www.w3.org/1999/02/22-rdf-syntax-ns#type',
      'o' => $group_path_array['x0'],
      'o type' => 'uri',
    ),
  );

  foreach ($voc->fields as $fid => $mapping) {

    $ret = wisski_vocab_ctrl_lookup($fid, $voc, $term_uri, NULL, NULL, NULL, NULL, 'never');
    if (!isset($ret[$term_uri][$fid]) || !$ret[$term_uri][$fid]) continue;
  
    $field = wisski_vocab_ctrl_field_get($fid);
    $path_id = $mapping['cis_pid'];
    $path_data = wisski_pathbuilder_getPathData($path_id);
    $path_array = unserialize($path_data['path_array']);

    foreach ($ret[$term_uri][$fid] as $value) {

      $parent_uri = $term_uri;
      $step = 1;
      $max_step = floor(count($path_array) / 2) + 1;  // for target_type "literal"
      if ($field['target_type'] == 'same_class') {
        $s = $max_step;
        for (;$s > 0; $s--) {
          if ($path_array["x$s"] == $path_array["x0"]) {
            $max_step = $s;
            break;
          }
        }
        if ($s <= 0) {
          drupal_set_message("bad field mapping definition: path $path_id is not sameclass");
          continue; // no step contained same class, invalid definition
        }

      }
      while ($step < $max_step) {
        
        $uuid = wisski_get_uuid(4);
        $step_uri = $base_root . $base_path . 'content/' . $uuid;
        
        $triples[] = array(
            's' => $parent_uri,
            's_type' => 'uri',
            'p' => $path_array["y" . ($step - 1)],
            'o' => $step_uri,
            'o_type' => 'uri',
            );
        $triples[] = array(
            's' => $step_uri,
            's_type' => 'uri',
            'p' => 'http://www.w3.org/1999/02/22-rdf-syntax-ns#type',
            'o' => $path_array["x$step"],
            'o_type' => 'uri',
            );
        
        $parent_uri = $step_uri;
        $step++;
        
      }

      if ($field['target_type'] == 'literal') {

         $triples[] = array(
            's' => $parent_uri,
            's_type' => 'uri',
            'p' => $path_data["datatype_property"],
            'o' => $value['value'],
            'o_type' => 'literal',
            'o_datatype' => isset($value['datatype']) ? $value['datatype'] : NULL,
            'o_lang' => isset($value['lang']) ? $value['lang'] : NULL,
            );
      
      } elseif ($field['target_type'] == 'same_class') {

        $triples[] = array(
            's' => $parent_uri,
            's_type' => 'uri',
            'p' => $path_array["y" . ($step - 1)],
            'o' => $value['value'],
            'o_type' => 'uri',
            );

      }
 
    }

  }
  
  // create nodes for instances and save trilpes to standard instance graph
  foreach ($triples as $t) {
    $n = wisski_store_addNodeForTriple($t);
  }
  $store = wisski_store_getObj()->wisski_ARCAdapter_getStore();
  $a = $store->insert($triples, wisski_store_getObj()->wisski_ARCAdapter_graphName());
  
  return array();

}


function _wisski_vocab_ctrl_get_path_array($voc, $field) {
  
  if (!is_object($field)) $field = wisski_vocab_ctrl_field_get($field);
  if (!$field) return NULL;
  $fid = $field->fid;
  if (!isset($voc->fields['fields'][$fid])) return NULL;
  
  return _wisski_vocab_ctrl_adjust_path_array($voc->fields['fields'][$fid]['cis_path'], $field['target_type']);

}


function _wisski_vocab_ctrl_adjust_path_array($path_data, $field_type) {

  if (!is_array($path_data)) {
    include_once(drupal_get_path('module', 'wisski_pathbuilder') . '/wisski_pathbuilder.inc');
    $path_data = wisski_pathbuilder_getPathData($path_data);
  }
  
  // get the path array
  if ($path_data['is_group']) {
    if ($field['target_type'] == 'literal') return NULL;  // a group cannot have a literal at its end
    include_once(drupal_get_path('module', 'wisski_pathbuilder') . '/wisski_pathbuilder.inc');
    $path_array = _wisski_pathbuilder_calculate_group_samepart($path_data['id']);
  } else {
    $path_array = unserialize($path_data['path_array']);
  }
  
  // adjust path array according to field type
  if ($field_type == 'same_class') {

    // search for last element identical to start element
    $x0 = $path_array['x0'];
    $i = ((count($path_array) - 1) / 2);
    for (; $i > 0; $i--) {
      if ($path_array["x$i"] == $x0) break;
    }
    if ($i == 0) return NULL;  // we are through and didn't find any identical element!
    $path_array = array_slice($path_array, 0, $i * 2 + 1);

  } elseif ($field_type == 'literal') {

    // whole path + datatype property
    $path_array['dt_prop'] = $path_data['datatype_property'];

  }

  return $path_array;

}


function _wisski_vocab_ctrl_build_sparql_query_pattern($path_data, $target_type, $source, $target, $match_mode) {

  $path_array = _wisski_vocab_ctrl_adjust_path_array($path_data, $target_type);
  
  $sparql .= '  ?source <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <' . $path_array['x0'] . "> . \n";

  if ($target_type == 'literal') {
    
    $last = 0;
    for ($i = 1; isset($path_array["x$i"]); $i++, $last++) {
      $sparql .= ((!$last) ? '  ?source <' : "  ?x$last <") . $path_array["y$last"] . "> ?x$i . \n";
      $sparql .= "  ?x$i <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <" . $path_array["x$i"] . "> . \n";
    }
    $sparql .= (isset($path_array['x1']) ? "  ?x$last <" : '  ?source <') . $path_array['dt_prop'] . "> ?target . \n";
    
  } elseif ($target_type == 'same_class') {
    
    $last = 0;
    $i = 1;
    for (; isset($path_array["y$i"]); $i++, $last++) {
      $sparql .= ((!$last) ? '  ?source <' : "  ?x$last <") . $path_array["y$last"] . "> ?x$i . \n";
      $sparql .= "  ?x$i <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <" . $path_array["x$i"] . "> . \n";
    }
    $sparql .= ((!$last) ? '  ?source <' : "  ?x$last <") . $path_array["y$last"] . "> ?target . \n";
    $sparql .= "  ?target <http://www.w3.org/1999/02/22-rdf-syntax-ns#type> <" . $path_array["x$i"] . "> . \n";
    
  }
  
  return $sparql;

}



/*============================================ *
*  VOCABULARY ACCESS
* ============================================ */


function _wisski_vocab_ctrl_lookup($fids, $voc, $source, $target, $limit, $offset, $match_mode, $use_index) {
  
  $args = func_get_args();
  array_shift($args);

  $label_fields = array('label', 'alt_label');
  
  // pseudo field 'all' lists all defined fields plus pseudo field 'tree'
  if (!$fids || $fids == 'all') $fids = array_keys(wisski_vocab_ctrl_field_get()) + array('tree');
   // pseudo field 'labels' lists all defined label fields
  if ($fids == 'labels') $fids = $label_fields;
    
  if (is_array($fids)) {
    $result = array();
    foreach ($fids as $fid) {
      $args1 = $args;
      array_unshift($args1, $fid);
      $fid_result = call_user_func_array('_wisski_vocab_ctrl_lookup', $args1);
      $result = array_merge_recursive($result, $fid_result);
    }
    return $result;
  }
  
  $fid = $fids;
  
  // look for special implementation of a pseudo-field like 'tree' or 'labels'
  if (function_exists("_wisski_vocab_ctrl_lookup_field_$fid")) {
    return call_user_func_array("_wisski_vocab_ctrl_lookup_field_$fid", $args);
  }
  if (in_array($fid, $label_fields)) {
    return _wisski_vocab_ctrl_lookup_field_labels($fid, $voc, $source, $target, $limit, $offset, $match_mode, $use_index);
  }


// currently we only use indexes for labels. this is catched above!
  return _wisski_vocab_ctrl_lookup_direct($fid, $voc, $source, $target, $limit, $offset, $match_mode);
  
}
 

function _wisski_vocab_ctrl_lookup_direct($fid, $voc, $source, $target, $limit, $offset, $match_mode) {

  $field = wisski_vocab_ctrl_field_get($fid);
  
  if (!$field || !isset($voc->fields[$fid])) {
    return array();
  }
  
  if (isset($voc->fields[$fid]['trans_mid'])) {
    $mapping = wisski_vocab_ctrl_mapping_get($voc->fields[$fid]['trans_mid']);
    $regex = $mapping['regex'];
  } else {
    $mapping = NULL;
    $regex = NULL;
  }
  
  // build the sparql query
  if (!$source) {
    $sparql = "SELECT ?source ?target WHERE { \n";
  } else {
    $sparql = "SELECT ?target WHERE { \n";
  }
  
  // the graph pattern, either from path definition or from mapping pattern
  if (!$mapping) {  
    // no mapping, use normal pathbuilder paths
    
    if (isset($voc->fields[$fid]['trans_pid'])) {
      $pid = $voc->fields[$fid]['trans_pid'];
    } else {
      $pid = $voc->fields[$fid]['cis_pid'];
    }

    $sparql .= _wisski_vocab_ctrl_build_sparql_query_pattern($pid, $field['target_type'], $source, $target, $match_mode);

  } else {
    // use a special mapping
    
    $sparql .= ' ' . trim($mapping['pattern']);

  }

  // apply filters for source and target, if we can restrict the search space
  if ($source) {
    $sparql = str_replace(' ?source ', " <$source> ", $sparql);
  }
  if ($target) {
    if ($field['target_type'] == 'literal') {
      $target = wisski_store_escape_sparql_literal($target);
      switch($match_mode) {
        case 'exact':
          $sparql .= '    FILTER (str(?target) = "' . $target . '") ' . "\n";
          break;
        case 'contains':
          $target = ".*$target.*";
          $sparql .= '    FILTER (regex(?target, "' . $target . '", "i")) ' . "\n";
          break;
        case 'contains_word':
          $wb = "[-\\s,'\\/.]";
          $target = "^$target$|^$target$wb.*|.*$wb$target$wb.*|.*$wb$target$";
          $sparql .= '    FILTER (regex(?target, "' . $target . '", "i")) ' . "\n";
          break;
        case 'starts_word':
          $wb = "[-\\s,'\\/.]";
          $target = "^$target.*|.*$wb$target.*";
          $sparql .= '    FILTER (regex(?target, "' . $target . '", "i")) ' . "\n";
          break;
        case 'regex':
          $sparql .= '    FILTER (regex(?target, "' . $target . '", "i")) ' . "\n";
          break;
      }
    } elseif ($field['target_type'] == 'same_class') {
      $sparql .= "    FILTER (str(?target) = <$target>) \n";
    }
  }
  
  $sparql .= " \n}\n";

  // Limit the result list
  if (!$source && $target) {
    $sparql .= "ORDER BY ?source ?target \n";
  } elseif (!$source) {
    $sparql .= "ORDER BY ?source \n";
  } elseif ($target) {
    $sparql .= "ORDER BY ?target \n";
  }
  if ($limit) $sparql .= "LIMIT $limit \n";
  if ($offset) $sparql .= "OFFSET $offset \n";
  $rows = $voc->store->query($sparql, 'rows');
  if ($voc->store->getErrors()) {
    foreach ($voc->store->getErrors() as $e) {
      watchdog('wisski_vocab', check_plain($e), array(), WATCHDOG_WARNING);
    }
    return array();
  }
    
  if (!$rows) return array();
  
  $results = array();
  foreach($rows as $row) {
    if ($source) $row['source'] = $source;
    if ($regex) {
      preg_match_all("/$regex/", $row['target'], $matches);
      $matches = $matches[0];
    } else {
      $matches = array($row['target']);
    }
    foreach ($matches as $match) {
      $result = array('value' => trim($match));
      if ($row['target type']) $result['type'] = $row['target type'];
      if ($row['target lang']) $result['lang'] = $row['target lang'];
      if ($row['target datatype']) $result['datatype'] = $row['target datatype'];
      $results[$row['source']][$fid][] = $result;
    }
  }
 
  return $results;

}



function _wisski_vocab_ctrl_lookup_field_labels($fid, $voc, $source, $target, $limit = NULL, $offset = NULL, $match_mode = 'exact', $use_index = 'normal') {
  

  if ($use_index != 'never' && !$source && variable_get('wisski_vocab_ctrl_index_labels_' . $voc->vid, 'no') == 'indexed') {
    // the vocab is indexed

    $q = 'SELECT * FROM {wisski_vocab_ctrl_index} WHERE vid = ' . $voc->vid . " AND fid = '$fid' AND ";
    switch ($match_mode) {
      case 'contains':
        $target = db_escape_string($target);
        $target = strtr($target, array('\\' => '\\\\', '%' => '\\%', '_' => '\\_'));
        $q .= "value LIKE '%%$target%%'";
        break;
      case 'contains_word':
        $target = db_escape_string($target);
        $target = strtr($target, array('\\' => '\\\\', '%' => '\\%', '_' => '\\_'));
        $q .= "(value = '$target' OR value LIKE '%% $target' OR value LIKE '%% $target %%' OR value LIKE '$target %%')";
        break;
      case 'starts_word':
        $target = db_escape_string($target);
        $target = strtr($target, array('\\' => '\\\\', '%' => '\\%', '_' => '\\_'));
        $q .= "(value = '$target' OR value LIKE '$target%%')";
        break;
      default: // case 'exact':
        $target = db_escape_string($target);
        $q .= "value = '$target'";
        break;
    }
    $q .= ' ORDER BY value, uri';
    if ($limit) $q .= " LIMIT %d";
    if ($offset) $q .= ' OFFSET %d';

    $rows = db_query($q, $limit, $offset);
    $results = array();
    while ($row = db_fetch_array($rows)) {
      $result = array('value' => $row['value']);
      if ($row['lang']) $result['lang'] = $row['lang'];
      $results[$row['uri']][$fid][] = $result;
    }
    return $results;
    
  } elseif ($use_index != 'always') {
    // not indexed
    
    return _wisski_vocab_ctrl_lookup_direct($fid, $voc, $source, $target, $limit, $offset, $match_mode);
    
  } else {

    return array();

  }


  
}


/* Walk the term hierarchy up or down and return the tree.
*
* @parameter $source the starting instance
* @parameter $max_branches maximum branching factor per tree node.
*   e.g. set to 1 to obtain a list
* @parameter $direction_fid the id of the field that serves as relation
*   for walking
* @parameter $inverse_direction_fid the id of the field that is opposite to
*   $direction_fid. E.g. if the hierarchy shall be walked up, $diection_fid
*   should be sth. like 'broader' and $inverse_..._fid should be 'narrower'.
*
* @return an array with keys being all instances in the tree including $source
*   The values are arrays of the form
*   array('children' => array(<uri1>, <uri2>,...), 'label' => <label>).
*   There will always also be a key for <uri*>.
*   NOTE: children may be narrower or braoder depending on the direction.
*   The tree may be traversed by starting at root $source and querying its
*   children. For each child this can be repeated. If 'children' array is empty
*   the node is a leaf.
*   If $source has no children, an empty array is returned.
*
*/
function _wisski_vocab_ctrl_lookup_tree($voc, $source, $max_branches, $direction_fid, $inverse_direction_fid) {
  
  if (!$source) return NULL;
  
  $tree = array($source => array());
  $agenda = array($source);
  
  while ($agenda) {
    
    $uri = array_shift($agenda);
    
    // $tree[$uri] will always be defined
    $tree[$uri]['children'] = array();
    $nodes = array();
    if ($direction_fid) {
      $res = _wisski_vocab_ctrl_lookup($direction_fid, $voc, $uri, NULL, $max_branches, 0, NULL, 'never');
      if ($res) {
        foreach ($res[$uri][$direction_fid] as $u) {
          $nodes[$u['value']] = 1;
        }
      }
    }
    if ($inverse_direction_fid && (!$max_branches || count($nodes) < $max_branches)) {
      $res = _wisski_vocab_ctrl_lookup($inverse_direction_fid, $voc, NULL, $uri, (!$max_branches) ? NULL : $max_branches - count($nodes), 0, NULL, 'never');
      $nodes += $res;
    }
    foreach ($nodes as $nuri => $bla) {
      // detect loops! e.g. Getty top concepts refer to themselves!
      if ($uri != $nuri) {  // avoid self-references!
        $tree[$uri]['children'][] = $nuri; 
      }
      if (!isset($tree[$nuri])) { // avoid cycles!
        $agenda[] = $nuri;
        $label = _wisski_vocab_ctrl_lookup('label', $voc, $nuri, NULL, 1, 0, NULL, 'never');
        if ($label) $tree[$nuri] = array('label' => $label[$nuri]['label'][0]['value']);
      }
    }
    
  }

  if (count($tree) < 2) return array();  // defacto no tree (only the $source uri as root is set)
  else return $tree;
  
}



function _wisski_vocab_ctrl_lookup_field_broader_labels($voc, $source_orig, $target, $limit, $offset, $match_mode) {
  
  $source = $source_orig;
  // walk up the term hierarchy, don't consider branches (one term suffices)
  // return a list of broader terms with label info
  $ret = _wisski_vocab_ctrl_lookup_tree($voc, $source, 1, 'broader', 'narrower');
  
  $result = array();
  while ($source = isset($ret[$source]['children'][0]) ? $ret[$source]['children'][0] : NULL) {
    if ($ret[$source]['label']) $result[] = $ret[$source]['label'];
  }
  
  if (!$result) return array();
  return array($source_orig => array('broader_labels' => $result));
}


function _wisski_vocab_ctrl_lookup_field_broader_tree($voc, $source, $target, $limit, $offset, $match_mode) {
   return array($source => array('broader_tree' => _wisski_vocab_ctrl_lookup_tree($voc, $source, NULL, 'broader', 'narrower')));
}


function _wisski_vocab_ctrl_lookup_field_narrower_tree($voc, $source, $target, $limit, $offset, $match_mode) {
   return array($source => array('narrower_tree' => _wisski_vocab_ctrl_lookup_tree($voc, $source, NULL, 'narrower', 'broader')));
}



/** Update the index for vocab $voc
*
* As this can take some time, a $status_callback is supplied that
* can delegate status messages to some ajax or something.
*/
function wisski_vocab_ctrl_create_labels_index($voc, $fids, $status_callback = NULL, $initial_offset = 0, $offset_break = NULL, $limit = 2000) {
  
  if (!$voc || !$fids) {
    if ($status_callback) $status_callback(0, 'Missing vocabulary or field(s)');
    return NULL;
  }
  if (!is_object($voc)) {
    $voc = wisski_vocab_ctrl_get($voc);
  }
  $vid = $voc->vid;
  if (!is_array($fids)) $fids = array($fids);
  
  variable_set("wisski_vocab_ctrl_index_labels_$vid", 'rebuild');
  db_query("DELETE FROM {wisski_vocab_ctrl_index} WHERE vid = %d", $vid);

  $label_count = $initial_offset;

  // indexing may take some time... disable max exec time
  if (!ini_get('safe_mode')) {
    set_time_limit(0);
  }
  
  foreach ($fids as $fid) {
    
    if ($status_callback) {
      $status_callback($label_count, t("Starting to index %field.", array('%field' => wisski_vocab_ctrl_field_get($fid, 'name'))));
    }

    $offset = $initial_offset;

    while (TRUE) {
      
      if ($offset_break !== NULL && $offset >= $offset_break) return;

      $labels_per_uri = _wisski_vocab_ctrl_lookup_direct($fid, $voc, NULL, '', $limit, $offset, 'contains');
      $offset += $limit;

      if (!$labels_per_uri) break;
      
      $values = array();

      foreach ($labels_per_uri as $uri => $labels) {
        
        foreach ($labels[$fid] as $label) {
          $label['fid'] = $fid;
          $label['uri'] = $uri;
          $label['vid'] = $vid;
          $values[] = $label['fid'];
          $values[] = $label['value'];
          $values[] = $label['vid'];
          $values[] = $label['uri'];
//          drupal_write_record('wisski_vocab_ctrl_index', $label);
          $label_count++;
        }
        
      }
      
      $query = 'INSERT INTO {wisski_vocab_ctrl_index} (`fid`, `value`, `vid`, `uri`) VALUES ' . implode(', ', array_fill(0, count($values) / 4, "('%s', '%s', %d, '%s')")) . ";";
      db_query($query, $values);
      
      if ($status_callback) {
        $status_callback($label_count, NULL);
      }

    }
    
  }
  
  if ($status_callback) {
    $status_callback($label_count, t("Indexing finished successfully."));
  }

  variable_set("wisski_vocab_ctrl_index_labels_$vid", 'indexed');
  variable_set("wisski_vocab_ctrl_index_timestamp_labels_$vid", time());

}

