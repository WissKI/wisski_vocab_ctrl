<?php


/*============================================ *
*  VOCABULARIES 
* ============================================ */


/** Lists all vocabularies and gives basic info
 */
function wisski_vocab_ctrl_admin_list() {

  $vocs = wisski_vocab_ctrl_get();

  $vocs_list = array();
  foreach ($vocs as $voc) {
    $vid = $voc->vid;
    $item = array();
    $item[] = $voc->priority;
    $item[] = $voc->name;
    $item[] = wisski_accesspoint_get($voc->aid, 'name');
    $item[] = _wisski_vocab_ctrl_pretty_path_name($voc->group_id);
    $item[] = '<a href="' . url("admin/settings/wisski/vocab_ctrl/$vid/edit") . '">'. t('Edit') . '</a>' .
      ' <a href="' . url("admin/settings/wisski/vocab_ctrl/$vid/delete") . '">' . t('Delete') . '</a>';
    $vocs_list[] = $item;
  }

  $output = theme('table', array(t('Priority'), t('Name'), t('Access point'), t('Group'), t('Operations')), $vocs_list);
  return $output;

}



function _wisski_vocab_ctrl_pretty_path_name ($id) {
  include_once(drupal_get_path('module', 'wisski_pathbuilder') . '/wisski_pathbuilder.inc');
  $info = wisski_pathbuilder_getPathData($id);
  $name = ''; 
  do {
    $name = $info['name'] . ' -> ' . $name;
    $id = $info['group_id'];
    $info = wisski_pathbuilder_getPathData($id);
  } while ($info['group_id'] > 0);
  return substr($name, 0, -4);
}



/** The form for adding, editing and cloning vocabularies
 * If $vid is non-null, it is assumed to be an edit action, otherwise an add action
 */
function wisski_vocab_ctrl_admin_edit(&$form_state, $vid = NULL, $aid = NULL, $group_id = NULL, $fields = NULL, $indices = NULL) {

  include_once(dirname(__FILE__) . '/wisski_vocab_ctrl.inc');

  // add or edit or clone? / init default values for form
  $edit = $vid != NULL;
  if ($form_state['values']) {
    // this will only hold on ahah calls
    $voc = _wisski_vocab_ctrl_admin_build_voc_from_form_values($form_state['values']);
    $voc->vid = $vid; // this will not be transmitted and is thus not in the formstate values
  } elseif ($edit) {
    // load existing data
    $voc = wisski_vocab_ctrl_get($vid);
  } else {
    // add vocabulary
    // default values
    $voc = array(
        'vid' => NULL,
        'name' => '',
        'aid' => '',
        'priority' => 0,
        'group_id' => '',
        'fields' => array(),
        'indices' => array('enabled' => FALSE),
        );
    $voc = (object) $voc;
  }

  if ($aid) $voc->aid = $aid;
//  if ($group_id) $voc->group_id = $group_id;
//  if ($fields) $voc->fields = $fields; 
//  if ($indices) $voc->indices = $indices; 
  
  drupal_load('module', 'wisski_pathbuilder');
  include_once(drupal_get_path('module', 'wisski_pathbuilder') . '/wisski_pathbuilder.inc');
  
  $fields = wisski_vocab_ctrl_field_get();

  // check accesspoint, group and paths/mappings
  $accesspoints = array();
  foreach (wisski_accesspoint_get() as $aid => $ap) {
    $accesspoints[$aid] = $ap->name;
  }
  if ($voc->aid && !isset($accesspoints[$voc->aid])) {
    drupal_set_message(t('Access point with ID %ap does not exist.', array('%ap' => $voc->aid)), 'error');
    $voc->aid = NULL;
  }
  if ($voc->group_id && wisski_pathbuilder_getPathData($voc->group_id) === FALSE) {
    drupal_set_message(t('Group with ID %group does not exist.', array('%group' => $voc->group_id)), 'error');
    $voc->group_id = '';
    $voc->fields['fields'] = array();
  }
  if ($voc->fields) {
    foreach ($voc->fields as $fid => $opts) {
      if (wisski_pathbuilder_getPathData($opts['cis_pid']) === FALSE) {
        drupal_set_message(t('Path with ID %path in field %field does not exist.', array('%path' => $opts['cis_pid'], '%field' => $fields[$fid]['name'])), 'error');
        unset($voc->fields[$fid]);
      }
      if (isset($opts['trans_mid'])) {
        if (!wisski_vocab_ctrl_mapping_get($opts['trans_mid'])) {
          drupal_set_message(t('Mapping with ID %mid in field %field does not exist.', array('%path' => $opts['trans_mid'], '%field' => $fields[$fid]['name'])), 'error');
          unset($voc->fields[$fid]);
        }
      }
    }
  }
    

  // for pb_group: display every top group of the import PB section
  $pb_groups = array('' => '<' . t('Select group') . '>');
  foreach (wisski_pathbuilder_getEnabledGroups(3) as $id) {
    $info = wisski_pathbuilder_getPathData($id);
    if ($info['group_id'] < 1) $pb_groups["$id"] = $info['name'];
  }
  
   
  // store vid 
  $form['vid'] = array('#type' => 'value', '#value' => $voc->vid);
 
  $form['vocabulary'] = array(
    '#type' => 'fieldset',
    '#title' => t('Vocabulary settings'),
  );
  $form['vocabulary']['name'] = array(
      '#type' => 'textfield',
      '#title' => t('Name'),
      '#required' => TRUE,
      '#size' => 30,
      '#default_value' => $voc->name,
      );
  $form['vocabulary']['priority'] = array(
      '#type' => 'weight',
      '#title' => t('Priority'),
      '#required' => TRUE,
      '#delta' => 20,
      '#default_value' => $voc->priority,
      );
  $form['vocabulary']['aid'] = array(
      '#type' => 'select',
      '#title' => t('Access point'),
      '#required' => TRUE,
      '#options' => $accesspoints,
      '#default_value' => $voc->aid,
      );
  $form['mapping'] = array(
      '#type' => 'fieldset',
      '#title' => t('Field mappings'),
      );
  $form['mapping']['group_id'] = array(
      '#type' => 'select',
      '#title' => t('Import group'),
      '#multiple' => FALSE /*TRUE*/,
      '#options' => $pb_groups,
      '#default_value' => $voc->group_id,
      '#ahah' => array(
        'wrapper' => 'mapping-field-table-wrapper',
//        'effect' => 'fade',
//        'method' => 'replace',
        'path' => 'wisski/vocab_ctrl/edit/js/mapping/field_table',
      ),
      '#required' => TRUE,
      );
  $form['mapping']['field_table'] = array(
      '#prefix' => '<div id="mapping-field-table-wrapper">',
      '#suffix' => '</div>',
      '#theme' => 'wisski_vocab_ctrl_table',
      );

  // build table with fields, only if a group is selected
  if ($voc->group_id) {

    $paths_per_field = array();
    $path_datas = array();
    foreach (wisski_pathbuilder_getAllMembers($voc->group_id, TRUE) as $pid) {
      $path_datas[$pid] = wisski_pathbuilder_getPathData($pid);
    }
    foreach ($fields as $fid => $field) {
      $paths_per_field[$fid][''] = ($fid == 'label') ? t('select') . ' ...' : ('<' . t('None') . '>');
      foreach ($path_datas as $pid => $path_data) {
        if (_wisski_vocab_ctrl_adjust_path_array($path_data, $field['target_type'])) {
          $paths_per_field[$fid][$pid] = $path_data['name'];
        }
      }
    }
    
    $mappings = array('' => '<' . t('None') . '>');
    foreach (wisski_vocab_ctrl_mapping_get() as $mid => $mapping) {
      $mappings[$mid] = $mapping['name'];
    }
    
    foreach ($fields as $fid => $field) {
      $form['mapping']['field_table']["row_$fid"]["fid_$fid"] = array(
        '#title' => t('Field'),
        '#value' => $field['name'],
        '#required' => $fid == 'label',
      );
      $form['mapping']['field_table']["row_$fid"]["cis_pid_$fid"] = array(
        '#type' => 'select',
        '#title' => t('Import path'),
        '#options' => $paths_per_field[$fid],
        '#default_value' => isset($voc->fields[$fid]) ? $voc->fields[$fid]['cis_pid'] : '',
        '#required' => $fid == 'label',
      );
      $form['mapping']['field_table']["row_$fid"]["trans_mid_$fid"] = array(
        '#type' => 'select',
        '#title' => t('Query pattern'),
        '#default_value' => isset($voc->fields[$fid]['trans_mid']) ? $voc->fields[$fid]['trans_mid'] : '',
        '#options' => $mappings,
      );
    }
  }
  
  // local vocabularies always build an index on saving triples
  $form['index'] = array(
      '#type' => 'fieldset',
      '#title' => t('Indexes'),
      '#prefix' => '<div id="index-wrapper">',
      '#suffix' => '</div>',
      );
  if (wisski_accesspoint_get($voc->aid)->type != 'local') {
    $form['index']['index_enabled'] = array(
        '#type' => 'checkbox',
        '#title' => t('Enable indexing'),
        '#default_value' => $voc->indices['enabled'],
        '#return_value' => TRUE,
        '#ahah' => array(
          'wrapper' => 'index-wrapper',
          'path' => 'wisski/vocab_ctrl/edit/js/index',
        ),
        );
    $form['index']['index_refresh_interval'] = array(
        '#type' => 'textfield',
        '#title' => t('Index refresh interval'),
        //    '#field_prefix' => t('every'),
        '#field_suffix' => t('minutes'),
        '#default_value' => isset($voc->indices['refresh_interval']) ? $voc->indices['refresh_interval'] : '0',
        '#description' => t('Must be positive. 0 disables automatic re-indexing. The intervals are only roughly respected.'),
        '#disabled' => !$voc->indices['enabled'],
        );
  }
  if ($edit) {
    $form['index']['reindex'] = array(
        '#type' => 'submit',
        '#value' => t('Re-index now'),
        '#submit' => array('wisski_vocab_ctrl_admin_reindex_submit'),
        '#disabled' => !$voc->indices['enabled'],
        );
  }

  $form['save'] = array(
      '#type' => 'submit',
      '#value' => t('Submit'),
      '#validate' => array('wisski_vocab_ctrl_admin_edit_validate'),
      '#submit' => array('wisski_vocab_ctrl_admin_edit_submit'),
      '#executes_submit_callback' => TRUE,
      );
  global $base_root, $base_path;
  $form['cancel'] = array(
      '#type' => 'markup',
      '#value' => t('Cancel'),
      '#prefix' => '<a href="' . $base_root . $base_path . 'admin/settings/wisski/vocab_ctrl/list">',
      '#suffix' => '</a>',
      );

  return $form;

}


function wisski_vocab_ctrl_admin_edit_js($part, $subpart = NULL) {

  $form_state = array('storage' => NULL, 'submitted' => FALSE, 'rebuild' => TRUE);
  $form_build_id = $_POST['form_build_id'];
  $form = form_get_cache($form_build_id, $form_state);

  $args = $form['#parameters']; 
  $form_id = array_shift($args);  
  $form_state['values'] = $form_state['post'] = $form['#post'] = $_POST;  
  $form['#programmed'] = $form['#redirect'] = FALSE;
  if (!isset($args[1])) $args[1] = NULL;
  if (!isset($args[2])) $args[2] = NULL;

// we don't want to submit anything! don't call: drupal_process_form($form_id, $form, $form_state);
  $form = drupal_rebuild_form($form_id, $form_state, $args, $form_build_id);

  $form_part = $form[$part];
  if ($subpart) $form_part = $form_part[$subpart];
  unset($form_part['#prefix'], $form_part['#suffix']);

  $output = drupal_render($form_part);

  print drupal_json(array('status' => true, 'data' => $output));
  exit();


}



function wisski_vocab_ctrl_admin_check_reindex() {
  
  if (variable_get('wisski_vocab_ctrl_needs_reindex', FALSE)) {
    variable_set('wisski_vocab_ctrl_last_reindex', time());
    variable_set('wisski_vocab_ctrl_needs_reindex', FALSE);
  } else {
    return;
  }

  $vocs = wisski_vocab_ctrl_get();
  
  foreach ($vocs as $voc) {

    if (!$voc->indices['enabled']) {
      continue;
    }
    
    $interval = $voc->indices['refresh_interval'];
    if ($interval <= 0) {
      continue;
    }

    $last = variable_get('wisski_vocab_ctrl_index_timestamp_labels_' . $voc->vid, 0);
    if ($last + $interval > time())  {
      continue; 
    }

    $fids = array('label');
    if (isset($voc->fields['alt_label'])) $fids[] = 'alt_label';
    wisski_vocab_ctrl_create_labels_index($voc, $fids);
    
    $n = time() + $voc->indices['refresh_interval'];
    if ($n < $next) $next = $n;
    
  }
  
 
}



function wisski_vocab_ctrl_admin_reindex_submit($form, &$form_state) {

  include_once(dirname(__FILE__) . '/wisski_vocab_ctrl.inc');

  $voc = wisski_vocab_ctrl_get($form_state['values']['vid']);
  $fids = array('label');
  if (isset($voc->fields['alt_label'])) $fids[] = 'alt_label';
  
  wisski_vocab_ctrl_create_labels_index($voc, $fids, function($p, $m) { drupal_set_message(($m) ? "At $p labels: $m" : "$p labels indexed"); });

  $form_state['redirect'] = 'admin/settings/wisski/vocab_ctrl/list';

}


function wisski_vocab_ctrl_admin_edit_validate($form, &$form_state) {

  include_once(dirname(__FILE__) . '/wisski_vocab_ctrl.inc');
  $vocs = wisski_vocab_ctrl_get();

  $voc = _wisski_vocab_ctrl_admin_build_voc_from_form_values($form_state['values']);

  foreach ($vocs as $v) {
    if ($v->name == $voc->name && $v->vid != $voc->vid) form_set_error('name', 'There already exists a vocabulary with that name. Please choose a different one.');
  }

  if (!preg_match('/^\\d*$/', trim($voc->index_refresh_interval))) form_set_error('index_refresh_interval', t('Index refresh rate must be a positive number.'));

}


function wisski_vocab_ctrl_admin_edit_submit($form, &$form_state) {
  
  $voc = _wisski_vocab_ctrl_admin_build_voc_from_form_values($form_state['values']);

  $form_state['redirect'] = 'admin/settings/wisski/vocab_ctrl/list';
  
  wisski_vocab_ctrl_save($voc);

}


function _wisski_vocab_ctrl_admin_build_voc_from_form_values($v) {
  
  $voc = (object) $v;
  
  $voc->fields = array();
  foreach (wisski_vocab_ctrl_field_get() as $fid => $field) {
    $cis_pid = "cis_pid_$fid";
    $trans_mid = "trans_mid_$fid";
    if ($voc->$cis_pid) {
      $voc->fields[$fid]['cis_pid'] = $voc->$cis_pid;
      if ($voc->$trans_mid) $voc->fields[$fid]['trans_mid'] = $voc->$trans_mid;
    }
  }
  
  if (wisski_accesspoint_get($voc->aid, "type") != 'local') {
    $voc->indices = array('enabled' => $voc->index_enabled, 'refresh_interval' => $voc->index_refresh_interval * 1);
  } else {
    $voc->indices = array('enabled' => TRUE, 'refresh_interval' => 1);
  }

  variable_set('wisski_vocab_ctrl_next_index_cron', 0);
  
  return $voc;

}




/**Pseudo-form for deletion of vocabs: Prompt for confirmation
 */
function wisski_vocab_ctrl_admin_delete(&$form_state, $vid) {

  include_once(dirname(__FILE__) . '/wisski_vocab_ctrl.inc');

  $data = wisski_vocab_ctrl_get($vid);

  $form = array(
      'vid' => array('#type' => 'value', '#value' => $vid),
      'query' => array(
        '#type' => 'markup',
        '#value' => t('Do you really want to delete "@voc"? This action cannot be undone.', array('@voc' => $data->name)),
        '#prefix' => '<div>',
        '#suffix' => '</div>',
        ),
      'submit' => array(
        '#type' => 'submit',
        '#value' => t('Delete'),
        '#prefix' => '<div>',
        '#suffix' => ' <a href="' . url('/admin/settings/wisski/vocab_ctrl/list') . '">' . t('Cancel'). '</a></div>',
        ),
      );

  return $form;

}


function wisski_vocab_ctrl_admin_delete_submit($form, &$form_state) {

  include_once(dirname(__FILE__) . '/wisski_vocab_ctrl.inc');

  wisski_vocab_ctrl_delete($form_state['values']['vid']);

  $form_state['redirect'] = 'admin/settings/wisski/vocab_ctrl';

}





/*============================================ *
*  FIELDS
* ============================================ */


function wisski_vocab_ctrl_admin_field_settings() {
  
  global $base_root, $base_path;
  
  $target_types = array('literal' => t('literal'), 'same_class' => t('same class as source'));

  $form['info'] = array(
//    '#value' => t('Add fields for more data to be imported from vocabularies.'),
  );
  
  $form['table'] = array(
    '#prefix' => '<div>',
    '#theme' => 'wisski_vocab_ctrl_field_settings_table',
    '#suffix' => '</div>',
  );
  foreach (wisski_vocab_ctrl_field_get() as $id => $field) {
    $form['table']["row_$id"] = array(
      "name_$id" => array(
        '#type' => 'textfield',
        '#title' => t('Name'),
        '#default_value' => $field['name'],
        '#size' => 20,
        '#disabled' => ($field['origin'] != 'user'),
      ),
      "fid_$id" => array(
        '#value' => $id,
      ),
      "target_type_$id" => array(
        '#type' => 'select',
        '#title' => t('Target type'),
        '#options' => $target_types,
        '#default_value' => $field['target_type'],
        '#disabled' => ($field['origin'] != 'user'),
      ),
      "delete_$id" => array(
        '#type' => 'checkbox',
        '#title' => 'Delete',
        '#default_value' => FALSE,
        '#return_value' => TRUE,
        '#disabled' => ($field['origin'] != 'user'),
      ),
      "description_$id" => array(
        '#type' => 'textarea',
        '#default_value' => $field['description'],
        '#cols' => 100,
        '#rows' => 3,
        '#disabled' => ($field['origin'] != 'user'),
      ),
    );
  }
  
  $form['table']['row_']['name_'] = array(
    '#type' => 'textfield',
    '#maxlength' => '50',
    '#size' => 20,
    '#prefix' => '<span title="' . t('When saving, a field will be created if the name field is non-empty.') . '">',
    '#suffix' => '</span>',
  );
  $form['table']['row_']['fid_'] = array(
    '#prefix' => '<span title="' . t('When saving, a field will be created if the name field is non-empty.') . '">',
    '#value' => t('Add field'),
    '#suffix' => '</span>',
  );
  $form['table']['row_']['delete_'] = array(
    '#value' => '',
  );
  $form['table']['row_']['target_type_'] = array(
    '#type' => 'select',
    '#options' => $target_types,
    '#default_value' => 'literal',
    '#prefix' => '<span title="' . t('When saving, a field will be created if the name field is non-empty.') . '">',
    '#suffix' => '</span>',
  );
  $form['table']['row_']['description_'] = array(
    '#type' => 'textarea',
    '#cols' => 100,
    '#rows' => 3,
    '#prefix' => '<span title="' . t('When saving, a field will be created if the name field is non-empty.') . '">',
    '#suffix' => '</span>',
  );
  
  $form['submit_cancel_field'] = array(
    '#prefix' => '<div><hr/>',
    '#suffix' => '</div>',
  );
  $form['submit_cancel_field']['submit'] = array(
    '#type' => 'submit',
    '#value' => 'Save',
  );
  $form['submit_cancel_field']['cancel'] = array(
    '#type' => 'markup',
    '#prefix' => '<a href="' . $base_root . $base_path . 'admin/settings/wisski/vocab_ctrl">',
    '#value' => t('Cancel'),
    '#suffix' => '</a>',
  );

  return $form;

}


function theme_wisski_vocab_ctrl_field_settings_table($element) {
  
  $header = array(t('Delete') , t('ID'), t('Name'), t('Target type'), t('Description'));

  foreach (element_children($element) as $key) {
    $row = array();
    $id = substr($key, 4);
    unset($element[$key]["delete_$id"]['#title']);
    $row[] = drupal_render($element[$key]["delete_$id"]);
    unset($element[$key]["fid_$id"]['#title']);
    $row[] = drupal_render($element[$key]["fid_$id"]);
    unset($element[$key]["name_$id"]['#title']);
    $row[] = drupal_render($element[$key]["name_$id"]);
    unset($element[$key]["target_type_$id"]['#title']);
    $row[] = drupal_render($element[$key]["target_type_$id"]);
    unset($element[$key]["description_$id"]['#title']);
    $row[] = drupal_render($element[$key]["description_$id"]);
    $rows[] = $row;
  }
      
  return theme('table', $header, $rows);
  
}


function wisski_vocab_ctrl_admin_field_settings_submit($form, $form_state) {
  
  $v = $form_state['values'];
  $fields = wisski_vocab_ctrl_field_get();
  
  // update existing fields
  foreach($fields as $id => $field) {
    if ($field['origin'] != 'user') continue; // only user defined fields may be changed

    if ($v["delete_$id"]) {

      wisski_vocab_ctrl_field_delete($id);
  
    } elseif (   $v["name_$id"] != $field['name']
              || $v["target_type_$id"] != $field['target_type']
              || $v["description_$id"] != $field['description']) {
        
      wisski_vocab_ctrl_field_set(array(
        'fid' => $id, 
        'name' => $v["name_$id"],
        'target_type' => $v["target_type_$id"],
        'description' => $v["description_$id"],
      ));
      
    }

  } 

  // add new field
  $name = trim($v['name_']);
  if ($name) {

    // determine new id
    $id = strtolower(preg_replace('/[^a-zA-Z0-9]+/', '_', $name));
    if (isset($fields[$id])) {
      $i = 0;
      while (isset($fields["${id}_$i"])) $i++;
      $id .= "_$id";
    }

    wisski_vocab_ctrl_field_set(array(
      'fid' => $id,
      'name' => $name,
      'origin' => 'user',
      'target_type' => $v['target_type_'],
      'description' => $v['description_'],
    ));

  }
  
}



/*============================================ *
*  MAPPINGS
* ============================================ */


function wisski_vocab_ctrl_admin_mapping_settings() {
  
  global $base_root, $base_path;

  $form['info'] = array(
//    '#value' => t('Add fields for more data to be imported from vocabularies.'),
  );
  
  $form['table'] = array(
    '#prefix' => '<div>',
    '#theme' => 'wisski_vocab_ctrl_table',
    '#suffix' => '</div>',
  );
  foreach (wisski_vocab_ctrl_mapping_get() as $id => $mapping) {
    $form['table']["row_$id"] = array(
      "delete_$id" => array(
        '#type' => 'checkbox',
        '#title' => 'Delete',
        '#default_value' => FALSE,
        '#return_value' => TRUE,
      ),
      "mid_$id" => array(
        '#title' => t('ID'),
        '#value' => $id,
      ),
      "name_$id" => array(
        '#type' => 'textfield',
        '#title' => t('Name'),
        '#default_value' => $mapping['name'],
        '#size' => 20,
      ),
      "pattern_$id" => array(
        '#type' => 'textarea',
        '#title' => t('Pattern'),
        '#default_value' => $mapping['pattern'],
        '#cols' => 100,
        '#rows' => 3,
      ),
      "regex_$id" => array(
        '#type' => 'textfield',
        '#title' => t('Regex'),
        '#default_value' => $mapping['regex'],
        '#size' => 15,
        '#field_prefix' => '/',
        '#field_suffix' => '/',
      ),
      "description_$id" => array(
        '#type' => 'textarea',
        '#title' => t('Description'),
        '#default_value' => $field['description'],
        '#cols' => 100,
        '#rows' => 3,
      ),
    );
  }
  
  $form['table']['row_']['delete_'] = array(
    '#title' => t('Delete'),
    '#value' => '',
    '#description' => t('When saving, the mapping will be created if the name field is non-empty.'),
  );
  $form['table']['row_']['mid_'] = array(
    '#title' => t('ID'),
    '#value' => t('Add field'),
    '#description' => t('When saving, the mapping will be created if the name field is non-empty.'),
  );
  $form['table']['row_']['name_'] = array(
    '#type' => 'textfield',
    '#title' => t('Name'),
    '#maxlength' => '50',
    '#size' => 20,
    '#description' => t('When saving, the mapping will be created if the name field is non-empty.'),
  );
  $form['table']['row_']['pattern_'] = array(
    '#type' => 'textarea',
    '#title' => t('Pattern'),
    '#default_value' => '',
    '#cols' => 100,
    '#rows' => 3,
    '#description' => t('When saving, the mapping will be created if the name field is non-empty.'),
  );
  $form['table']['row_']['regex_'] = array(
    '#type' => 'textfield',
    '#title' => t('Regex'),
    '#default_value' => '',
    '#size' => 15,
    '#field_prefix' => '/',
    '#field_suffix' => '/',
    '#description' => t('When saving, the mapping will be created if the name field is non-empty.'),
  );
  $form['table']['row_']['description_'] = array(
    '#title' => t('Description'),
    '#type' => 'textarea',
    '#cols' => 100,
    '#rows' => 3,
    '#description' => t('When saving, the mapping will be created if the name field is non-empty.'),
  );
  
  $form['submit_cancel_field'] = array(
    '#prefix' => '<div><p></p>',
    '#suffix' => '</div>',
  );
  $form['submit_cancel_field']['submit'] = array(
    '#type' => 'submit',
    '#value' => 'Save',
  );
  $form['submit_cancel_field']['cancel'] = array(
    '#type' => 'markup',
    '#prefix' => '<a href="' . $base_root . $base_path . 'admin/settings/wisski/vocab_ctrl">',
    '#value' => t('Cancel'),
    '#suffix' => '</a>',
  );

  return $form;

}


function theme_wisski_vocab_ctrl_table($element) {
  
  $header = array();

  $rows = array();
  $first = TRUE;
  foreach (element_children($element) as $row_name) {
    $row = array();
    foreach (element_children($element[$row_name]) as $col_name) {
      if ($first) $header[] = $element[$row_name][$col_name]['#title'];
      unset($element[$row_name][$col_name]['#title']);
      if (isset($element[$row_name][$col_name]['#description'])) {
        $prefix = '<span title="' . check_plain($element[$row_name][$col_name]['#description']) . '">';
        $suffix = '</span>';
        unset($element[$row_name][$col_name]['#description']);
      } else {
        $prefix = $suffix = '';
      }
      $row[] = $prefix . drupal_render($element[$row_name][$col_name]) . $suffix;
    }
    $rows[] = $row;
    $first = FALSE;
  }

  return theme('table', $header, $rows);
  
}


function wisski_vocab_ctrl_admin_mapping_settings_submit($form, $form_state) {
  
  $v = $form_state['values'];
  $mappings = wisski_vocab_ctrl_mapping_get();
  
  // update existing fields
  foreach($mappings as $id => $mapping) {

    if ($v["delete_$id"]) {

      wisski_vocab_ctrl_mapping_delete($id);
  
    } elseif (   $v["name_$id"] != $field['name']
              || $v["pattern_$id"] != $field['pattern']
              || $v["regex_$id"] != $field['regex']
              || $v["description_$id"] != $field['description']) {
        
      wisski_vocab_ctrl_mapping_set(array(
        'mid' => $id, 
        'name' => $v["name_$id"],
        'pattern' => $v["pattern_$id"],
        'regex' => $v["regex_$id"],
        'description' => $v["description_$id"],
      ));
      
    }

  } 

  // add new field
  $name = trim($v['name_']);
  if ($name) {

    // determine new id
    $id = strtolower(preg_replace('/[^a-zA-Z0-9]+/', '_', $name));
    if (isset($mappings[$id])) {
      $i = 0;
      while (isset($mappings["${id}_$i"])) $i++;
      $id .= "_$i";
    }

    wisski_vocab_ctrl_mapping_set(array(
      'mid' => $id,
      'name' => $name,
      'pattern' => $v['pattern_'],
      'regex' => $v['regex_'],
      'description' => $v['description_'],
    ));

  }
  
}


/*============================================ *
*  MAPPINGS
* ============================================ */


function wisski_vocab_ctrl_admin_cron_indexing() {
  
  $vocs = wisski_vocab_ctrl_get();
  $fields = wisski_vocab_ctrl_field_get();
  
  // vocs are only indexable with cron if they have indexing enabled
  // and they do not automatically refresh (avoid interference with
  // normal indexing routine (TODO this should be possible eventually)
  // note: all local vocs do refresh, so they cannot be indexed with cron
  $indexable_vocs = array();
  foreach ($vocs as $voc) {
    if ($voc->indices['enabled'] && $voc->indices['refresh_interval'] == 0) {
      $indexable_vocs[$voc->vid] = $voc->name;
    }
  }
  
  // section for adding a new task
  $form = array();
  $form['adv_idx_info'] = array(
    '#type' => 'markup',
    '#value' => t('The normal indexing methods can lead to complications when indexing big vocabularies. Use advanced indexing to better control the indexing process.'),
    '#prefix' => '<p>',
    '#suffix' => '<p>',
    '#weight' => 0,
  );
  $form['add_task'] = array(
    '#type' => 'fieldset',
    '#title' => t('Add indexing task'),
    '#collapsible' => TRUE,
    '#collapsed' => TRUE,
    '#weight' => 10,
    '#description' => t(
        'An indexing task indexes a vocabulary in subsequent steps that are triggered by cron. You can also !run.',
        array('!run' => l(
            t('run cron manually'),
            'admin/reports/status/run-cron',
            array('query' => array('destination' => 'admin/settings/wisski/vocab_ctrl/cron_indexing')))
        )
    ),
  );
  $form['add_task']['vid'] = array(
    '#type' => 'select',
    '#title' => t('Vocabulary'),
    '#options' => $indexable_vocs,
    '#weight' => 10,
  );
  $form['add_task']['limit'] = array(
    '#type' => 'textfield',
    '#title' => t('Limit per run'),
    '#description' => t('The maximum number of instances that get indexed per step/cron run. Depending on the endpoint\'s performance and server timeout settings this value may be increased or decreased.'),
    '#default_value' => 2000,
    '#weight' => 15,
  );
  $form['add_task']['immediately'] = array(
    '#type' => 'textfield',
    '#title' => t('Run immediately'),
    '#description' => t('Number of times the task is run directly after creation. Can be used if you don\'t want to wait for cron to run.'),
    '#default_value' => '1',
    '#field_suffix' => t('times'),
    '#weight' => 30,
  );
  $form['add_task']['disable_timeout'] = array(
    '#type' => 'checkbox',
    '#title' => t('Disable timeout'),
    '#description' => t('Disable the PHP execution timeout. Note that depending on your server\'s configuration, changeing the timeout may not be allowed.'),
    '#default_value' => '1',
    '#weight' => 40,
  );
  $form['add_task']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Add indexing task'),
    '#weight' => 50,
  );
  
  // rpint status msgs

  $form['status'] = array(
    '#type' => 'fieldset',
    '#title' => t('Status'),
    '#weight' => 50,
  );
  $form['status']['active'] = array(
    '#type' => 'fieldset',
    '#title' => t('Active tasks'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    '#weight' => 10,
  );
  $form['status']['terminated'] = array(
    '#type' => 'fieldset',
    '#title' => t('Terminated tasks'),
    '#collapsible' => TRUE,
    '#collapsed' => FALSE,
    '#weight' => 50,
  );

  // compile stats for tasks
  // this could actually be a table
  $rebuilds = variable_get('wisski_vocab_ctrl_index_rebuilds', array());
  $vid_reb = array();
  foreach ($rebuilds as $k => $task) $vid_reb[$task['vid']] = $k;

  foreach ($vocs as $voc) {
    $stat = NULL;
    $task = NULL;
    $weight = 0;
    $active = FALSE;

    // determine whether we have completed or active stats for this voc
    // each voc has max one task! either a completed task or an active one
    if (isset($vid_reb[$voc->vid])) {
      //active
      $task = $rebuilds[$vid_reb[$voc->vid]];
      $stat = $task['stat'];
      $weight = $vid_reb[$voc->vid];
      $active = TRUE;
    } else {
      $stat = variable_get('wisski_vocab_ctrl_index_stat_' . $voc->vid, NULL);
      $weight = $voc->priority;
    }

    if (empty($stat)) continue; // voc has no tasks whatsoever

    $stat_str = '<ul>';
    $stat_str .= '<li>' . t('Started at %t', array('%t' => date(DATE_ISO8601, $stat['init_time']))) . '</li>';
    $stat_str .= '<li>' . t('%t runs', array('%t' => $stat['run_count'])) . '</li>';
    // timout only for active tasks
    if (!empty($task)) $stat_str .= '<li>' . t('Timeout: %t', array('%t' => $task['timeout'] ? t('enabled') : t('disabled'))) . '</li>';
    // fields progress
    foreach ($stat['counts'] as $fid => $field_count) {
      if (!isset($fields[$fid])) {
        $stat_str .= t('<li>Field %f: unknown field ID</li>', array('%f' => $fid));
      } else {
        $stat_str .= '<li>' . t('Field %f: %i instances, %l labels %c', array(
            '%f' => $fields[$fid]['name'],
            '%i' => $field_count['instance'],
            '%l' => $field_count['label'],
            // in case of an active task, specify whether field is being indexed now
            '%c' => empty($task) ? '' : ((in_array($fid, $task['fids'])) ? t('(ongoing)') : t('(completed)')),
            )) . '</li>';
      }
    }
    if (!empty($task)) {
      // only active stats
      if (!isset($fields[$fid])) {
        $stat_str .= t('<li>Field %f: unknown field ID</li>', array('%f' => $fid));
      } else {
        foreach(array_slice($task['fids'], 1) as $fid) {
          $stat_str .= '<li>' . t('Field %f: yet to be indexed', array('%f' => $fields[$fid]['name'])) . '</li>';
        }
      }
      $stat_str .= '<li>' . l(t('Stop task'), 'admin/settings/wisski/vocab_ctrl/stop_cron_indexing/' . $voc->vid) . '</li>';
    } else {
      // only terminated stats
      $stat_str .= '<li>' . t('Finished at %t', array('%t' => date(DATE_ISO8601, $stat['finish_time']))) . '</li>';
      $stat_str .= '<li>' . t('Terminating status: %s', array('%s' => htmlentities($stat['term_status']))) . '</li>';
    }
    $stat_str .= '</ul>';

    $form['status'][$active ? 'active' : 'terminated']['fs_vid' . $voc->vid] = array(
      '#type' => 'fieldset',
      '#title' => htmlentities($voc->name),
      '#collapsible' => TRUE,
    );
    $form['status'][$active ? 'active' : 'terminated']['fs_vid' . $voc->vid]['mu_vid' . $voc->vid] = array(
      '#type' => 'markup',
      '#value' => $stat_str,
    );

  }
  
  return $form;

}



function wisski_vocab_ctrl_admin_cron_indexing_validate($form, $form_state) {
  
  if (!preg_match('/^\d+$/', $form_state['values']['limit'])) {
    form_set_error('limit', t('Only integer values allowed'));
  }
  if (!preg_match('/^\d+$/', $form_state['values']['immediately'])) {
    form_set_error('immediately', t('Only integer values allowed'));
  }

}


function wisski_vocab_ctrl_admin_cron_indexing_submit($form, $form_state) {
  
  $v = $form_state['values'];
  include_once('wisski_vocab_ctrl.inc');
  _wisski_vocab_ctrl_index_rebuilds_init_task($v['vid'], $v['limit'], NULL, $v['immediately'], !$v['disable_timeout']);

}


function wisski_vocab_ctrl_admin_cron_indexing_stop($vid) {
  
  include_once('wisski_vocab_ctrl.inc');
  _wisski_vocab_ctrl_index_rebuilds_stop($vid);

  drupal_goto('admin/settings/wisski/vocab_ctrl/cron_indexing');

}
