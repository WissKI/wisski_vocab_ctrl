/**
 *
 * @ author: Eugen Meissner, changes by Martin Scholz
 * @ contact: eugen.meissner@ymail.com
 * (c) WissKI Project http://wiss-ki.eu
 *
 */

(function() {
  
  // Set up functions and vars in global namespace
  if (typeof WissKI === 'undefined') WissKI = {};
  if (typeof WissKI.vocab_ctrl === 'undefined') WissKI.vocab_ctrl = {};
  if (typeof WissKI.vocab_ctrl.tooltips === 'undefined') WissKI.vocab_ctrl.tooltips = {};
  

  /** here we cache all visible tooltips so that they can be directly accessed and hidden
  */
  WissKI.vocab_ctrl.tooltips.shown = [];
      

  /** hides all currently visible tooltips
  */
  WissKI.vocab_ctrl.tooltips.hideAll = function() {
    for (var i in WissKI.vocab_ctrl.tooltips.shown) {
      $(WissKI.vocab_ctrl.tooltips.shown[i]).qtip("hide");
    }
  };

  /** hides all currently visible tooltips
  * this is for backward compatibility with wisski_textedit/editor
  */
  if (typeof WissKI === 'undefined') WissKI = {};
  if (typeof WissKI.editor === 'undefined') WissKI.editor = {};
  if (typeof WissKI.editor.tooltips === 'undefined') WissKI.editor.tooltips = {};
  WissKI.editor.tooltips.hideAll = function() {
    for (var i in WissKI.vocab_ctrl.tooltips.shown) {
      $(WissKI.vocab_ctrl.tooltips.shown[i]).qtip("hide");
    }
  };


  /** Init an infobox for one element, retrieve the metadata
  */
  WissKI.vocab_ctrl.tooltips.setElement = function(elem) {
  
    // either jQuery or the qTip tooltip plugin are not loaded
    // just quietly quit, so that we do not produce an error
    if (typeof jQuery === 'undefined' ||
        typeof jQuery.fn.qtip === 'undefined') return;

  
    var className = elem.className;

    /* position of tooltip must be calculated if an.vocab_ctrl.is available */
    var xPos = 0;
    var yPos = 0;
    var ifr = $('#edit-body-wrapper');
    //var divOffset = $('#wki-content') ? $('#wki-content').offset() : $(ifr).offset();
    var divOffset = $(ifr).offset();
    var target = false;

    if ($('.mceEditor').length != 0) target = $('.mceEditor');

    if(ifr.length!=0 && !className.match('mceText')){
      xPos = divOffset.left+15;
      yPos = divOffset.top+70;
    }

    if(className.match('mceText'))
      className = elem.parentNode.parentNode.className;

    /* set label which will be displayed under info */
    var classLabel = WissKI.vocab_ctrl.tooltips.getAnnoClassLabel(className);
    var uri = WissKI.vocab_ctrl.tooltips.getDecodedAnnoURI(className);

    $(elem).qtip({
      content : {
        title : '<div id="wisski_vocab_ctrl_infobox_tip_title"><h1>'+classLabel+'</h1><a href="' + uri + '">' + uri + '</a></div>',
        text  : 'Please wait...'
      },
      position : {
        target : target,
        corner : {
          tooltip : (target == false) ? 'leftMiddle' : 'leftTop',
          target  : (target == false) ? 'rightMiddle' : 'rightTop'
        },
        adjust : {
          x : 0, //xPos,
          y : 0, //yPos,
          resize: true
        }
      },
      style : {
        border : {
          width   : 3,
          radius  : 3
        },
        width:375,
        padding : 5,
        tip : true,
        name  : 'light', 
      },
      api : {
        onRender : function() {
          
          var span = this.elements.target[0];
          if (span == null) {
            this.updateContent('Please try again later...');
            return;
          }
          
          var className = span.className;
          
          if(className.match('mceText'))
            className = span.parentNode.parentNode.className;

          if(className.match('wisski_anno_new')){
            WissKI.vocab_ctrl.tooltips.getContentForNewElem(className,this);
          }else{
            var uri = WissKI.vocab_ctrl.tooltips.getDecodedAnnoURI(className);
            WissKI.vocab_ctrl.tooltips.doRequest(uri, this, 2);
            WissKI.vocab_ctrl.tooltips.fetchGroupInfo(uri, className, this);
          } 
        },
        // Martin:  problem: when annotation is selected in menu, tooltip stays forever as
        // the menu element is destroyed and no mouseout event is triggered
        // use this as workaround: keep a list of shown tooltips, and destroy them on time.
        onShow : function() {
          WissKI.vocab_ctrl.tooltips.shown.push(elem);
        },
        onHide : function() {
          for (var i in WissKI.vocab_ctrl.tooltips.shown) {
            if (WissKI.vocab_ctrl.tooltips.shown[i] == elem) {
              WissKI.vocab_ctrl.tooltips.shown.splice(i, 1);
              break;
            }
          }
        }
      },
      show : {
        solo : true,

      },
      hide : {
        when:'mouseout',
//        when:'',
        delay:200,
        fixed:true
      }
    });
  };
    
  /** Displays a text for newly created instances where no data is available
  */
  WissKI.vocab_ctrl.tooltips.getContentForNewElem = function(className,ttip) {
    var ttipContent =  'Create a new ' + WissKI.vocab_ctrl.tooltips.getAnnoClassLabel(className);
        ttip.updateContent(ttipContent);
  };
  
  /** Retrieves metadata/fields from server
  */
  WissKI.vocab_ctrl.tooltips.doRequest = function(uri, tooltip, images_col) {
    
    $.ajax({
      url : Drupal.settings.wisski.vocab_ctrl.about_url,
      content_type : "application/json",
      type : "POST",
      data : 'about=' + '{"uri":"' + uri + '","image_col":"' + images_col + '"}',
      timeout : 4000,
      success : function (response) {
        if (!response) {
          tooltip.updateContent('No information available at the moment.');
          return;
        }
//        var tooltipContent = WissKI.vocab_ctrl.tooltips.getContent(json);
        tooltip.updateContent('<div id="wisski_vocab_ctrl_infobox_tip_body">' + response + '</div>');
      }
    });
  };


  /** Retrieves metadata/fields from server
  */
  WissKI.vocab_ctrl.tooltips.fetchGroupInfo = function(uri, className, tooltip) {
    
    var group = WissKI.vocab_ctrl.tooltips.getAnnoClassLabel(className);
    
    // we only update the label if there is none
    if (!group) {

      $.ajax({
        url : Drupal.settings.basePath + "wisski/vocab_ctrl/ajax/group",
        content_type : "application/json",
        type : "POST",
        data : 'about=' + '{"uri":"' + uri + '","include":["id","name"]}',
        timeout : 4000,
        success : function (response) {
          var json = Drupal.parseJson(response);
          WissKI.vocab_ctrl.tooltips.setTitleGroupInfo(json.id, json.name);
        }

      });

    }

  };

  WissKI.vocab_ctrl.tooltips.setTitleGroupInfo = function(id, name) {  
  
    $("#wisski_vocab_ctrl_infobox_tip_title h1").html('<span class="wisski_anno_class_' + id + '"></span>' + name);
  
  };
  
  /**
   * This method gets a string and search for wisski_anno_class_ID, extracts the ID and looks up the label.
   * If label exisits label will be returned otherweise 'undefined'
   *
   * @author Eugen Meissner
   * @param class names
   **/
  WissKI.vocab_ctrl.tooltips.getAnnoClassLabel = function(className) {
    
    var annoClass = '';
    var annoClassRegEx = /wisski_anno_class_(\w*)/;
    var annoClassIDRaw = annoClassRegEx.exec(className);
    if (annoClassIDRaw != null && Drupal.settings.wisski.editor) {
      annoClassID = decodeURIComponent(annoClassIDRaw[1]);
      var ontology = Drupal.settings.wisski.editor.ontology; 
      for (var i in ontology.classes) {
        if (ontology.classes[i].id == annoClassID) {
          annoClass = '<span class="wisski_anno_class_' + annoClassIDRaw[1] + '"></span>' + ontology.classes[i].label;
          break;
        }
      }
    }
    return annoClass;
  };

  
  /*
   * Same as above, only for wisski_anno_uri_URI.
   *
   * @author Eugen Meissner, Martin Scholz
   * @return decoded URI
   **/
  WissKI.vocab_ctrl.tooltips.getDecodedAnnoURI = function(className) {
    
    var annoUriRegEx = /wisski_anno_uri_(\S*)/;
    var annoURIA = annoUriRegEx.exec(className);
    var annoURI = decodeURIComponent(annoURIA[1]);

    return annoURI;
  };



  /** Declare a new jquery function that attaches an infobox to one or more DOM elements
  */
  $.fn.ttip_set = function() {
    for(var i=0; i<this.length; i++){
      WissKI.vocab_ctrl.tooltips.setElement(this[i]);
    }
  };



  $().ready(function() {
    
    // augment links in the structured data form so that vocab info box can be displayed
    if (Drupal.settings.wisski.vocab_ctrl.showTooltipsForForm) {
      $('.wki-data-value a').each(function() {
        $(this).addClass('wisski_anno');
        $(this).addClass('wisski_anno_uri_' + encodeURIComponent($(this).attr('href')));
      });
    }

    // attach a infobox to each annotation in the DOM tree
    $('.wisski_anno').ttip_set();
  });

})();
