#include <config.h>

#include <gtk/gtk.h>

#include <gavl/gavl.h>

#include <gmerlin/pluginregistry.h>
#include <gmerlin/translation.h>
#include <gmerlin/utils.h>

#include <gmerlin/gui_gtk/fileselect.h>
#include <gmerlin/gui_gtk/gtkutils.h>

// #include <medialist.h>

#include <gui_gtk/mediabrowser.h>

struct bg_nle_media_browser_s
  {
  GtkWidget * treeview;
  GtkWidget * video_window;
  GtkWidget * box;
  GtkWidget * plug;
  bg_nle_media_list_t * list;

  GtkTreeViewColumn * col_duration;
  GtkTreeViewColumn * col_name;

  bg_gtk_filesel_t * filesel;
  };

enum
  {
    COLUMN_THUMBNAIL,
    COLUMN_NAME,
    COLUMN_DURATION,
    COLUMN_WEIGHT,
    NUM_COLUMNS
  };

static gboolean
tree_changed_foreach(GtkTreeModel *model,
                     GtkTreePath  *path,
                     GtkTreeIter  *iter,
                     gpointer      data)
  {
  gtk_tree_model_row_changed(model, path, iter);
  return FALSE;
  }

static void column_resize_callback(GtkTreeViewColumn * col,
                                   gint * width_val,
                                   gpointer data)
  {
  GtkTreeModel * model;
  int width_needed;
  int name_width;
  int width;
  
  bg_nle_media_browser_t * w = data;

  width = col->width;
  
  if(col == w->col_duration)
    {
    gtk_tree_view_column_cell_get_size(col,
                                       (GdkRectangle*)0,
                                       (gint *)0,
                                       (gint *)0,
                                       &width_needed,
                                       (gint *)0);
    name_width = gtk_tree_view_column_get_fixed_width (w->col_name);
    
    if(width > width_needed)
      {
      name_width += width - width_needed;
      gtk_tree_view_column_set_fixed_width (w->col_name, name_width);
      }
    else if(width < width_needed)
      {
      name_width -= width_needed - width;
      gtk_tree_view_column_set_fixed_width (w->col_name, name_width);
      }
    
    }
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(w->treeview));
  gtk_tree_model_foreach (GTK_TREE_MODEL(model),
                          tree_changed_foreach, NULL);   
  }

static void update_entry(bg_nle_media_browser_t * w,
                         bg_nle_file_t * file, GtkTreeIter * iter)
  {
  gavl_video_format_t thumbnail_format;
  gavl_video_frame_t * thumbnail_frame;
  
  char string_buffer[GAVL_TIME_STRING_LEN + 32];
  GtkTreeModel * model;
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(w->treeview));

  gtk_list_store_set(GTK_LIST_STORE(model),
                     iter,
                     COLUMN_WEIGHT,
                     PANGO_WEIGHT_NORMAL, -1);
  /* Thumbnail */

  if(file->num_video_streams)
    {
    if(bg_get_thumbnail(file->filename,
                        w->list->plugin_reg,
                        (char **)0,
                        &thumbnail_frame,
                        &thumbnail_format))
      {
      GdkPixbuf * pb;
      pb = bg_gtk_pixbuf_from_frame(&thumbnail_format,
                                    thumbnail_frame);

      if(pb)
        {
        gtk_list_store_set(GTK_LIST_STORE(model),
                           iter,
                           COLUMN_THUMBNAIL,
                           pb, -1);
        g_object_unref(pb);
        }
      }
    }
  else
    {
    
    }
  
  /* Name */
  gtk_list_store_set(GTK_LIST_STORE(model), iter, COLUMN_NAME, file->name, -1);
  
  /* Duration */
  gavl_time_prettyprint(file->duration, string_buffer);
  gtk_list_store_set(GTK_LIST_STORE(model), iter, COLUMN_DURATION, string_buffer, -1);
  
  }



bg_nle_media_browser_t *
bg_nle_media_browser_create(bg_nle_media_list_t * list)
  {
  bg_nle_media_browser_t * ret;
  GtkListStore *store;
  GtkTreeViewColumn * col;
  GtkCellRenderer *renderer;
  GtkTreeSelection * selection;
  GtkWidget * scrolledwin;

  ret = calloc(1, sizeof(*ret));
  ret->list = list;
  
  /* Create list */

  store = gtk_list_store_new(NUM_COLUMNS,
                             GDK_TYPE_PIXBUF, // Thumbnail
                             G_TYPE_STRING,   // Name
                             G_TYPE_STRING,   // Duration
                             G_TYPE_INT);     // Foreground

  ret->treeview = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
  gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(ret->treeview), TRUE);
  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(ret->treeview), 0);

  /* Thumbnail */

  renderer = gtk_cell_renderer_pixbuf_new();
  col = gtk_tree_view_column_new ();
  gtk_tree_view_column_pack_start(col, renderer, FALSE);
  gtk_tree_view_column_add_attribute(col,
                                     renderer,
                                     "pixbuf", COLUMN_THUMBNAIL);
  gtk_tree_view_column_set_sizing(col,
                                  GTK_TREE_VIEW_COLUMN_GROW_ONLY);
  gtk_tree_view_append_column (GTK_TREE_VIEW(ret->treeview),
                               col);
  /* Name */

  renderer = gtk_cell_renderer_text_new();
  g_object_set(G_OBJECT(renderer), "xalign", 0.0, NULL);

  col = gtk_tree_view_column_new ();

  gtk_tree_view_column_pack_start(col, renderer, TRUE);
  gtk_tree_view_column_add_attribute(col, renderer,
                                     "text", COLUMN_NAME);
  gtk_tree_view_column_add_attribute(col,
                                     renderer,
                                     "weight", COLUMN_WEIGHT);
  gtk_tree_view_column_set_sizing(col,
                                  GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_append_column (GTK_TREE_VIEW(ret->treeview),
                               col);
  ret->col_name = col;
  
  /* Duration */

  renderer = gtk_cell_renderer_text_new();
  g_object_set(G_OBJECT(renderer), "xalign", 1.0, NULL);

  col = gtk_tree_view_column_new ();

  g_signal_connect(G_OBJECT(col),
                   "notify::width", G_CALLBACK(column_resize_callback),
                   (gpointer)ret);
  
  gtk_tree_view_column_set_title(col, "T");
  gtk_tree_view_column_pack_start(col, renderer, FALSE);
  gtk_tree_view_column_add_attribute(col,
                                     renderer,
                                     "text", COLUMN_DURATION);
  gtk_tree_view_column_add_attribute(col,
                                     renderer,
                                     "weight", COLUMN_WEIGHT);
  
  gtk_tree_view_column_set_sizing(col,
                                  GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_append_column (GTK_TREE_VIEW(ret->treeview),
                               col);
  ret->col_duration = col;

  /* Done with columns */
    
  gtk_widget_show(ret->treeview);
  
  scrolledwin =
    gtk_scrolled_window_new(gtk_tree_view_get_hadjustment(GTK_TREE_VIEW(ret->treeview)),
                            gtk_tree_view_get_vadjustment(GTK_TREE_VIEW(ret->treeview)));

  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwin),
                                 GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);
  gtk_container_add(GTK_CONTAINER(scrolledwin), ret->treeview);

  gtk_widget_show(scrolledwin);

  /* Pack everything */
  ret->box = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(ret->box), scrolledwin, TRUE, TRUE, 0);

  gtk_widget_show(ret->box);
  
  return ret;
  }

void bg_nle_media_browser_destroy(bg_nle_media_browser_t * b)
  {
  
  }

GtkWidget *
bg_nle_media_browser_get_widget(bg_nle_media_browser_t * b)
  {
  return b->box;
  }

static void add_file_callback(char ** files, const char * plugin,
                              void * data)
  {
  int i;
  bg_nle_media_browser_t * b = data;
  bg_nle_file_t * new_file;
  GtkTreeModel * model;
  GtkTreeIter iter;
  
  model = gtk_tree_view_get_model(GTK_TREE_VIEW(b->treeview));
  
  b->list->open_path = bg_strdup(b->list->open_path,
                                 bg_gtk_filesel_get_directory(b->filesel));
  
  i = 0;
  
  while(files[i])
    {
    new_file = bg_nle_media_list_load_file(b->list, files[i], plugin);

    if(new_file)
      {
      gtk_list_store_append(GTK_LIST_STORE(model), &iter);
      update_entry(b, new_file, &iter);
      }
    
    i++;
    }
  }

static void filesel_close_callback(bg_gtk_filesel_t * filesel,
                                    void * data)
  {
  bg_nle_media_browser_t * b = data;
  b->filesel = NULL;
  }
                        

void bg_nle_media_browser_load_files(bg_nle_media_browser_t * b)
  {
  if(b->filesel)
    return;

  b->filesel = bg_gtk_filesel_create(TRS("Load files"),
                                     add_file_callback,
                                     filesel_close_callback,
                                     b,
                                     b->box,
                                     b->list->plugin_reg,
                                     BG_PLUGIN_INPUT,
                                     BG_PLUGIN_FILE);

  if(b->list->open_path)
    bg_gtk_filesel_set_directory(b->filesel,
                                 b->list->open_path);
  else
    bg_gtk_filesel_set_directory(b->filesel,
                                 ".");
  bg_gtk_filesel_run(b->filesel, 0);
  }
