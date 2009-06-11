#include <stdlib.h>
#include <string.h>

#include <config.h>

#include <gmerlin/log.h>
#include <gmerlin/cfg_registry.h>
#include <gmerlin/utils.h>


#include <track.h>
#include <project.h>



#define LOG_DOMAIN "project_xml"

static const char * project_name   = "gmerlerra_project";
static const char * tracks_name    = "tracks";
static const char * selection_name = "selection";
static const char * visible_name   = "visible";

bg_nle_project_t * bg_nle_project_load(const char * filename)
  {
  xmlDocPtr xml_doc;
  xmlNodePtr node;
  xmlNodePtr child;
  char * tmp_string;
  bg_nle_track_t * track;
  bg_nle_project_t * ret;
  
  xml_doc = bg_xml_parse_file(filename);

  if(!xml_doc)
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN,
           "Couldn't open project file %s", filename);
    return (bg_nle_project_t*)0;
    }
  
  node = xml_doc->children;
  
  if(BG_XML_STRCMP(node->name, project_name))
    {
    bg_log(BG_LOG_ERROR, LOG_DOMAIN, "File %s contains no project", filename);
    xmlFreeDoc(xml_doc);
    return (bg_nle_project_t*)0;
    }
  ret = calloc(1, sizeof(*ret));

  node = node->children;

  while(node)
    {
    if(!node->name)
      {
      node = node->next;
      continue;
      }
    
    if(!BG_XML_STRCMP(node->name, visible_name))
      {
      tmp_string = (char*)xmlNodeListGetString(xml_doc, node->children, 1);
      sscanf(tmp_string, "%"PRId64" %"PRId64, &ret->start_visible, &ret->end_visible);
      free(tmp_string);
      }
    if(!BG_XML_STRCMP(node->name, selection_name))
      {
      tmp_string = (char*)xmlNodeListGetString(xml_doc, node->children, 1);
      sscanf(tmp_string, "%"PRId64" %"PRId64, &ret->start_selection, &ret->end_selection);
      free(tmp_string);
      }
    if(!BG_XML_STRCMP(node->name, tracks_name))
      {
      child = node->children;

      while(child)
        {
        if(!child->name)
          {
          child = child->next;
          continue;
          }

        if(!BG_XML_STRCMP(child->name, "track"))
          {
          track = bg_nle_track_load(xml_doc, child);
          bg_nle_project_append_track(ret, track);
          }
        
        child = child->next;
        }
      }
    node = node->next;
    }
  xmlFreeDoc(xml_doc);
  return ret;
  }

void bg_nle_project_save(bg_nle_project_t * p, const char * filename)
  {
  int i;
  char * tmp_string;
  xmlDocPtr  xml_doc;
  xmlNodePtr xml_project;
  xmlNodePtr node;
  
  xml_doc = xmlNewDoc((xmlChar*)"1.0");
  xml_project = xmlNewDocRawNode(xml_doc, NULL, (xmlChar*)project_name, NULL);
  
  xmlDocSetRootElement(xml_doc, xml_project);
  
  xmlAddChild(xml_project, BG_XML_NEW_TEXT("\n"));

  /* Global data */

  node = xmlNewTextChild(xml_project, (xmlNsPtr)0,
                         (xmlChar*)selection_name, NULL);
  tmp_string = bg_sprintf("%"PRId64" %"PRId64, p->start_selection, p->end_selection);
  xmlAddChild(node, BG_XML_NEW_TEXT(tmp_string));
  free(tmp_string);
  
  node = xmlNewTextChild(xml_project, (xmlNsPtr)0,
                         (xmlChar*)visible_name, NULL);
  tmp_string = bg_sprintf("%"PRId64" %"PRId64, p->start_visible, p->end_visible);
  xmlAddChild(node, BG_XML_NEW_TEXT(tmp_string));
  free(tmp_string);
  
  /* Add tracks */

  if(p->num_tracks)
    {
    node = xmlNewTextChild(xml_project, (xmlNsPtr)0,
                           (xmlChar*)tracks_name, NULL);

    tmp_string = bg_sprintf("%d", p->num_tracks);
    BG_XML_SET_PROP(node, "num", tmp_string);
    free(tmp_string);
    
    for(i = 0; i < p->num_tracks; i++)
      {
      bg_nle_track_save(p->tracks[i], node);
      }
    }
  
  /* Save */
  
  if(filename)
    {
    xmlSaveFile(filename, xml_doc);
    }
  }