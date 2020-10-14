/*
 * facereco_data_collection.cc
 *
 * This application is a simple application to grabe face picture for data
 * collection.
 *
 * Author: Vincent Abriou <vincent.abriou@st.com> for STMicroelectronics.
 *
 * Copyright (c) 2020 STMicroelectronics. All rights reserved.
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *
 *     http://www.opensource.org/licenses/BSD-3-Clause
 */

#include <cairo.h>
#include <condition_variable>
#include <dirent.h>
#include <filesystem>
#include <getopt.h>
#include <glib.h>
#include <gst/gst.h>
#include <gtk/gtk.h>
#include <iostream>
#include <iomanip>
#include <linux/input.h>
#include <opencv2/opencv.hpp>
#include <semaphore.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>

/* Application parameters */
std::string video_device_str  = "0";
std::string camera_fps_str    = "15";
std::string camera_width_str  = "640";
std::string camera_height_str = "480";
bool verbose = false;

/* Synchronization variables */
std::condition_variable cond_var;
std::mutex cond_var_m;
std::mutex mtx;

bool gtk_main_started = false;

#define DATASET_DIRECTORY    "/usr/local/demo-ai/computer-vision/tflite-face-recognition/dataset/"
#define CSS_DIRECTORY        "/usr/local/demo-ai/computer-vision/tflite-face-recognition/bin/"

#define EXIT_AREA_WIDTH   50
#define EXIT_AREA_HEIGHT  50
#define BRAIN_AREA_WIDTH  75
#define BRAIN_AREA_HEIGHT 80

typedef struct _Position {
	int x;
	int y;
	int width;
	int height;
} Position;

typedef enum _State {
	STATE_IDLE = 0,
	STATE_CONSENT,
	STATE_ENTER_NAME,
	STATE_READY_PICTURE_1,
	STATE_TAKE_PICTURE_1,
	STATE_READY_PICTURE_2,
	STATE_TAKE_PICTURE_2,
	STATE_READY_PICTURE_3,
	STATE_TAKE_PICTURE_3,
	STATE_READY_PICTURE_4,
	STATE_TAKE_PICTURE_4,
	STATE_READY_PICTURE_5,
	STATE_TAKE_PICTURE_5,
	STATE_READY_PICTURE_6,
	STATE_TAKE_PICTURE_6,
	STATE_READY_PICTURE_7,
	STATE_TAKE_PICTURE_7,
	STATE_READY_PICTURE_8,
	STATE_TAKE_PICTURE_8,
	STATE_FINISH
} State;

typedef struct _Key {
	int id;
	GtkWidget *button;
} Key;

/* Structure that contains all information to pass around */
typedef struct _CustomData {
	/* The gstreamer pipeline */
	GstElement *pipeline;
	/* The GTK window widget */
	GtkWidget *window;
	/* Cairo brain icon pointer */
	cairo_surface_t *brain_icon;
	/* Cairo exit icon pointer */
	cairo_surface_t *exit_icon;
	/* window resolution */
	int window_width;
	int window_height;
	/* Frame resolution (still picture or camera) */
	int frame_width;
	int frame_height;
	/* Frame position in fullscreen mode */
	Position frame_fullscreen_pos;
	int frame_fullscreen_pos_x;
	int frame_fullscreen_pos_y;
	/* Managing consent window */
	GtkWidget *consent;
	GtkWidget *consent_fixed;
	/* Managing virtual keyboard */
	GtkWidget *keyboard;
	GtkWidget *keyboard_entry;
	GtkWidget *keyboard_label;
	GtkWidget *keyboard_fixed;
	std::vector<Key> keys;
	/* face label */
	std::string face_label;
	/* State machine */
	State state = STATE_IDLE;
} CustomData;

/* Framerate statisitics */
gdouble display_avg_fps = 0;
gdouble processing_avg_fps = 0;

void change_state(CustomData *data)
{
	int next_state = (int)data->state;
	next_state++;
	if (next_state > STATE_FINISH)
		next_state = STATE_IDLE;

	data->state = (State)next_state;

	if (data->state == STATE_ENTER_NAME) {
		/* display the virtual keyboard to enter the face label */
		gtk_label_set_markup(GTK_LABEL(data->keyboard_label),
				     "<span font='15' color='#D3007AFF'>"
				     "<b>Please enter your name...</b></span>");
		gtk_widget_show_all(data->keyboard);
	}
	if (data->state == STATE_CONSENT) {
		/* display the consent box */
		gtk_widget_show_all(data->consent);
	}
}

/**
 * This function is called when a click on "return" key of the virtual
 * keyboard is registered.
 */
static bool gui_start_grabbing_pictures(std::string label,
					CustomData *data)
{
	std::stringstream file_path_sstr;
	file_path_sstr << DATASET_DIRECTORY << label << "_1.png";

	/* Create the directory if it does not exist */
	DIR* dirp = opendir(DATASET_DIRECTORY);
	if (dirp == NULL)
		mkdir(DATASET_DIRECTORY,
		      S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	closedir(dirp);

	/* Check if the label already exists */
	if (access(file_path_sstr.str().c_str(), 0) == 0) {
		gtk_label_set_markup(GTK_LABEL(data->keyboard_label),
				     "<span font='15' color='#D3007AFF'>"
				     "<b>Name already exists! please enter a "
				     "new name...</b></span>");
		return false;
	}

	data->face_label = label;

	/* clear the widget entry and hide the keyboard */
	gtk_entry_set_text(GTK_ENTRY(data->keyboard_entry), "");
	gtk_widget_hide(data->keyboard);

	change_state(data);

	gtk_widget_queue_draw(data->window);

	return true;
}

/**
 * This function is an helper to get frame position on the display
 */
#define WESTON_PANEL_THICKNESS 32
static void gui_compute_frame_position(GdkWindow *window,
				       CustomData *data)
{
	int window_width = data->window_width;
	int window_height = data->window_height;
	int offset_x = 0;
	int offset_y = 0;

	window_height += WESTON_PANEL_THICKNESS;

	float width_ratio   = (float)window_width / (float)data->frame_width;
	float height_ratio  = (float)window_height / (float)data->frame_height;

	if (width_ratio > height_ratio) {
		data->frame_fullscreen_pos.width = (int)((float)data->frame_width * height_ratio);
		data->frame_fullscreen_pos.height = (int)((float)data->frame_height * height_ratio);
		data->frame_fullscreen_pos.x = (window_width - data->frame_fullscreen_pos.width) / 2;
		data->frame_fullscreen_pos.y = offset_y;
	} else {
		data->frame_fullscreen_pos.width = (int)((float)data->frame_width * width_ratio);
		data->frame_fullscreen_pos.height = (int)((float)data->frame_height * width_ratio);
		data->frame_fullscreen_pos.x = offset_x;
		data->frame_fullscreen_pos.y = (window_height - data->frame_fullscreen_pos.height) / 2;
	}
}

/**
 * This function is called each time the GTK UI is redrawn
 */
bool first_call = true;
static gboolean gui_draw_cb(GtkWidget *widget,
			    cairo_t *cr,
			    CustomData *data)
{
	GdkWindow *window = gtk_widget_get_window(widget);

	/* Set the transparent background */
	cairo_set_source_rgba (cr, 0.31, 0.32, 0.31, 0.0);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	cairo_paint(cr);

	if (first_call) {
		data->window_width = gdk_window_get_width(window);
		data->window_height = gdk_window_get_height(window);
		/* Calculating the position of the preview in fullscreen
		 * mode. This position is compute automaticaly by the
		 * waylandsink gstreamer element and is it is not
		 * exposed. Thus we need to calculate it by our own. */
		gui_compute_frame_position(window, data);

		/* set font size according to the display size */
		std::stringstream css_sstr;
		css_sstr << CSS_DIRECTORY;
		if (data->window_width > 800)
			css_sstr << "widgets_720p.css";
		else if (data->window_width > 480)
			css_sstr << "widgets_480p.css";
		else
			css_sstr << "widgets_default.css";

		GtkCssProvider *cssProvider = gtk_css_provider_new();
		gtk_css_provider_load_from_path (cssProvider,
						 css_sstr.str().c_str(),
						 NULL);
		gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
							  GTK_STYLE_PROVIDER(cssProvider),
							  GTK_STYLE_PROVIDER_PRIORITY_USER);

		/* Set the pipeline into playing state as face database has been
		 * initialized */
		gst_element_set_state(data->pipeline, GST_STATE_PLAYING);

		first_call = false;
	}

	std::stringstream info1_sstr;
	std::stringstream info2_sstr;
	std::stringstream info3_sstr;
	std::stringstream info4_sstr;
	std::stringstream info5_sstr;

	switch (data->state) {
	case STATE_IDLE:
		info1_sstr << "You can contribute to improve ST face recognition";
		info2_sstr << "neural network model by taking pictures of your face";
		info3_sstr << "(8 pictures with different head positions are needed).";
		info4_sstr << "It only takes few seconds to complete.";
		info5_sstr << "Ready? Touch anywhere to start taking pictures...";
		break;
	case STATE_CONSENT:
		break;
	case STATE_ENTER_NAME:
		info2_sstr << "Enter your name using the virtual keyboard.";
		break;
	case STATE_TAKE_PICTURE_1:
	case STATE_READY_PICTURE_1:
		info2_sstr << "-1-";
		info3_sstr << "Touch anywhere to take picture 1";
		break;
	case STATE_TAKE_PICTURE_2:
	case STATE_READY_PICTURE_2:
		info2_sstr << "-2-";
		info3_sstr << "Touch anywhere to take picture 2";
		info4_sstr << "Please change you head position";
		break;
	case STATE_TAKE_PICTURE_3:
	case STATE_READY_PICTURE_3:
		info2_sstr << "-3-";
		info3_sstr << "Touch anywhere to take picture 3";
		info4_sstr << "Please change you head position";
		break;
	case STATE_TAKE_PICTURE_4:
	case STATE_READY_PICTURE_4:
		info2_sstr << "-4-";
		info3_sstr << "Touch anywhere to take picture 4";
		info4_sstr << "Please change you head position";
		break;
	case STATE_TAKE_PICTURE_5:
	case STATE_READY_PICTURE_5:
		info2_sstr << "-5-";
		info3_sstr << "Touch anywhere to take picture 5";
		info4_sstr << "Please change you head position";
		break;
	case STATE_TAKE_PICTURE_6:
	case STATE_READY_PICTURE_6:
		info2_sstr << "-6-";
		info3_sstr << "Touch anywhere to take picture 6";
		info4_sstr << "Please change you head position";
		break;
	case STATE_TAKE_PICTURE_7:
	case STATE_READY_PICTURE_7:
		info2_sstr << "-7-";
		info3_sstr << "Touch anywhere to take picture 7";
		info4_sstr << "Please change you head position";
		break;
	case STATE_TAKE_PICTURE_8:
	case STATE_READY_PICTURE_8:
		info2_sstr << "-8-";
		info3_sstr << "Touch anywhere to take picture 8";
		info4_sstr << "Please change you head position";
		break;
	case STATE_FINISH:
		info2_sstr << "THANK YOU!";
		break;
	}

	/* Draw the brain icon at the top left corner */
	cairo_set_source_surface(cr, data->brain_icon, 5, 5);
	cairo_paint(cr);

	/* Draw the exit icon at the top right corner */
	cairo_set_source_surface(cr,
				 data->exit_icon,
				 data->window_width - EXIT_AREA_WIDTH, 0);
	cairo_paint(cr);
	cairo_save(cr);

	/* Translate to the preview position taking into account the
	 * brain icon to not overlap it. */
	if (data->frame_fullscreen_pos.x == 0)
		cairo_translate(cr,
				0,
				std::max(data->frame_fullscreen_pos.y,
					 BRAIN_AREA_HEIGHT));
	else
		cairo_translate(cr,
				std::max(data->frame_fullscreen_pos.x,
					 BRAIN_AREA_WIDTH),
				0);

	/* Display inference result */
	cairo_select_font_face (cr, "monospace",
				CAIRO_FONT_SLANT_NORMAL,
				CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size (cr, 30);
	cairo_set_source_rgb (cr, 0.83, 0.0, 0.48);

	/* Display the information to the user */
	cairo_text_extents_t extents;
	double x;

	cairo_text_extents(cr, info1_sstr.str().c_str(), &extents);
	x = (data->frame_fullscreen_pos.width / 2) - ((extents.width / 2) + extents.x_bearing);
	cairo_move_to(cr, x, 40);
	cairo_show_text(cr, info1_sstr.str().c_str());

	cairo_text_extents(cr, info2_sstr.str().c_str(), &extents);
	x = (data->frame_fullscreen_pos.width / 2) - ((extents.width / 2) + extents.x_bearing);
	cairo_move_to(cr, x, 80);
	cairo_show_text(cr, info2_sstr.str().c_str());

	cairo_text_extents(cr, info3_sstr.str().c_str(), &extents);
	x = (data->frame_fullscreen_pos.width / 2) - ((extents.width / 2) + extents.x_bearing);
	cairo_move_to(cr, x, 120);
	cairo_show_text(cr, info3_sstr.str().c_str());

	cairo_text_extents(cr, info4_sstr.str().c_str(), &extents);
	x = (data->frame_fullscreen_pos.width / 2) - ((extents.width / 2) + extents.x_bearing);
	cairo_move_to(cr, x, 160);
	cairo_show_text(cr, info4_sstr.str().c_str());

	cairo_text_extents(cr, info5_sstr.str().c_str(), &extents);
	x = (data->frame_fullscreen_pos.width / 2) - ((extents.width / 2) + extents.x_bearing);
	cairo_move_to(cr, x, 240);
	cairo_show_text(cr, info5_sstr.str().c_str());

	return FALSE;
}

/**
 * Following function are about the virtual keyboard management
 */
#define KEYBOARD_NB_ROWS 4
static const int nbOfKeysPerRow[KEYBOARD_NB_ROWS] = {11, 11, 11, 11};
static const std::vector<std::string> _keys =
{"1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "del",
 "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "return",
 "a", "s", "d", "f", "g", "h", "j", "k", "l", "#", "#",
 "#", "z", "x", "c", "v", "b", "n", "m", "#", "#", "#"};

static void gui_keyboard_clicked(GtkWidget *button,
				 CustomData *data)
{
	int button_index;
	std::stringstream entry_sstr;

	for (unsigned int i = 0 ; i < data->keys.size() ; i++) {
		if (data->keys[i].button == button) {
			button_index = data->keys[i].id;
			break;
		}
	}

	entry_sstr << gtk_entry_get_text(GTK_ENTRY(data->keyboard_entry));

	/* if return button is pressed then the face registration enter in the
	 * final step else if the del button is pressed, the last carater from
	 * the entry is removed else continue to enter the face label */
	if(_keys[button_index] == "return") {
		if(!entry_sstr.str().empty()) {
			/* get the entry content and finalize the face
			 * registration */
			if(!gui_start_grabbing_pictures(entry_sstr.str(), data)) {
				/* If label already exists ask user to enter a
				 * new label */
				return;
			}
		}
	} else if (_keys[button_index] == "del") {
		/* get the entry content and remove the last character */
		std::string s = entry_sstr.str();
		s.resize(s.size() - 1);
		gtk_entry_set_text(GTK_ENTRY(data->keyboard_entry), s.c_str());
	} else {
		entry_sstr << _keys[button_index];
		gtk_entry_set_text(GTK_ENTRY(data->keyboard_entry),
				   entry_sstr.str().c_str());
	}
}

void gui_get_keyboard_size(GtkWidget *widget,
			   GtkAllocation *allocation,
			   CustomData *data)
{
	/* Move the keyboard to be centered */
	int x = (data->window_width - allocation->width) / 2;
	int y = (data->window_height - allocation->height) / 2;
	gtk_fixed_move(GTK_FIXED(data->keyboard_fixed), widget, x, y);
}

void gui_button_OK_clicked(GtkWidget *widget,
			   CustomData *data)
{
	change_state(data);
	gtk_widget_hide(data->consent);
	gtk_widget_queue_draw(data->window);
}

void gui_button_KO_clicked(GtkWidget *widget,
			   CustomData *data)
{
	data->state = STATE_IDLE;
	gtk_widget_hide(data->consent);
	gtk_widget_queue_draw(data->window);
}

void gui_get_consent_box_size(GtkWidget *widget,
			   GtkAllocation *allocation,
			   CustomData *data)
{
	/* Move the consent box to be centered */
	int x = (data->window_width - allocation->width) / 2;
	int y = (data->window_height - allocation->height) / 2;
	gtk_fixed_move(GTK_FIXED(data->consent_fixed), widget, x, y);
}

/**
 * This function is called when a click or touch event is recieved
 */
static gboolean gui_press_event_cb(GtkWidget *widget,
				   GdkEventButton *event,
				   CustomData *data)
{
	gdouble draw_area_width  = (gdouble)gdk_window_get_width(event->window);

	if (event->button == GDK_BUTTON_PRIMARY) {
		/* exit if event occurs in the exit area */
		if ((event->x > (draw_area_width - EXIT_AREA_WIDTH)) &&
		    (event->y < EXIT_AREA_HEIGHT)) {
			gtk_main_quit();
			goto end;
		} else {
			if (data->state != STATE_FINISH) {
				change_state(data);
				gtk_widget_queue_draw(data->window);
			}
		}
	}

end:
	return TRUE;
}

/**
 * This function is creating GTK widget to create the window to display UI
 * information
 */
static void gui_create(CustomData *data)
{
	GtkWidget *drawing_area;

	data->brain_icon = cairo_image_surface_create_from_png("/usr/local/demo-ai/computer-vision/tflite-face-recognition/bin/resources/ST7079_AI_neural_pink_65x80.png");
	data->exit_icon = cairo_image_surface_create_from_png("/usr/local/demo-ai/computer-vision/tflite-face-recognition/bin/resources/close_50x50_pink.png");

	/* Create the window */
	data->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(data->window), "delete-event",
			 G_CALLBACK (gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(data->window), "destroy",
			 G_CALLBACK (gtk_main_quit), NULL);
	/* Remove title bar */
	gtk_window_set_decorated(GTK_WINDOW(data->window), FALSE);
	/* Tell GTK that we want to draw the background ourself */
	gtk_widget_set_app_paintable(data->window, TRUE);
	gtk_window_maximize(GTK_WINDOW(data->window));

	/* Create the drawing area to draw text on it using cairo */
	drawing_area = gtk_drawing_area_new();
	gtk_widget_add_events(drawing_area, GDK_BUTTON_PRESS_MASK);
	g_signal_connect(G_OBJECT(drawing_area), "draw",
			 G_CALLBACK(gui_draw_cb), data);
	g_signal_connect(G_OBJECT(drawing_area), "button-press-event",
			 G_CALLBACK(gui_press_event_cb), data);

	gtk_container_add (GTK_CONTAINER(data->window), drawing_area);

	gtk_widget_show_all(data->window);

	/* manage a virtual keyboard */
	data->keyboard = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(data->keyboard, "destroy",
			 G_CALLBACK(gtk_main_quit), NULL);
	/* Remove title bar */
	gtk_window_set_decorated(GTK_WINDOW(data->keyboard), FALSE);
	/* Tell GTK that we want to draw the background ourself */
	gtk_widget_set_app_paintable(data->keyboard, TRUE);
	gtk_window_maximize(GTK_WINDOW(data->keyboard));

	data->keyboard_fixed = gtk_fixed_new();
	gtk_container_add(GTK_CONTAINER(data->keyboard), data->keyboard_fixed);

	data->keyboard_entry = gtk_entry_new();

	data->keyboard_label = gtk_label_new(NULL);
	g_object_set(data->keyboard_label, "margin", 20, NULL);

	GtkWidget *grid1 = gtk_grid_new();
	int index = 0;
	for(int i = 0 ; i < KEYBOARD_NB_ROWS ; i++)
	{
		for(int j = 0 ; j < nbOfKeysPerRow[i] ; j++)
		{
			/* do not display the button with # */
			if (_keys[index].compare("#") == 0) {
				index++;
				continue;
			}

			Key new_key;
			new_key.id = index;
			new_key.button = gtk_button_new_with_label(_keys[index].c_str());

			if (_keys[index].compare("return") == 0) {
				gtk_grid_attach(GTK_GRID(grid1),
						new_key.button,
						j, i, 1, 3);
			} else {
				gtk_grid_attach(GTK_GRID(grid1),
						new_key.button,
						j, i, 1, 1);
			}

			g_signal_connect(new_key.button,
					 "clicked",
					 G_CALLBACK(gui_keyboard_clicked),
					 data);

			data->keys.push_back(new_key);
			index++;
		}
	}

	GtkWidget *grid2=gtk_grid_new();
	gtk_grid_attach(GTK_GRID(grid2), grid1, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid2), data->keyboard_label, 0, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid2), data->keyboard_entry, 0, 2, 1, 1);


	gtk_fixed_put(GTK_FIXED(data->keyboard_fixed), grid2, 200, 200);

	g_signal_connect(grid2, "size-allocate",
			 G_CALLBACK(gui_get_keyboard_size), data);

	/* manage consent information */
	data->consent = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(data->consent, "destroy",
			 G_CALLBACK(gtk_main_quit), NULL);
	/* Remove title bar */
	gtk_window_set_decorated(GTK_WINDOW(data->consent), FALSE);
	/* Tell GTK that we want to draw the background ourself */
	gtk_widget_set_app_paintable(data->consent, TRUE);
	gtk_window_maximize(GTK_WINDOW(data->consent));

	data->consent_fixed = gtk_fixed_new();
	gtk_container_add(GTK_CONTAINER(data->consent), data->consent_fixed);

	GtkWidget *consent_label = gtk_label_new(NULL);
	g_object_set(consent_label, "margin", 20, NULL);
	gtk_label_set_markup(GTK_LABEL(consent_label),
			     "<span font='15' color='#D3007AFF'>"
			     "<b>"
			     "By clicking \"I agree\" I give my consent to the AIS team to use my\n"
			     "face pictures in order to train and validate the ST face recognition\n"
			     "neural network model. My pictures will be kept internal to ST and will\n"
			     "be stored for a maximum time of 13 months.</b></span>");

	GtkWidget *grid_consent = gtk_grid_new();
	GtkWidget *button_OK = gtk_button_new_with_label("I agree");
	gtk_widget_set_hexpand (button_OK, TRUE);
	gtk_widget_set_halign (button_OK, GTK_ALIGN_START);
	GtkWidget *button_KO = gtk_button_new_with_label("I do not agree");
	gtk_widget_set_hexpand (button_KO, TRUE);
	gtk_widget_set_halign (button_KO, GTK_ALIGN_END);
	g_signal_connect(button_OK, "clicked",
			 G_CALLBACK(gui_button_OK_clicked), data);
	g_signal_connect(button_KO, "clicked",
			 G_CALLBACK(gui_button_KO_clicked), data);

	gtk_grid_attach(GTK_GRID(grid_consent), consent_label, 0, 0, 2, 1);
	gtk_grid_attach(GTK_GRID(grid_consent), button_OK, 0, 1, 1, 1);
	gtk_grid_attach_next_to (GTK_GRID (grid_consent), button_KO, button_OK, GTK_POS_RIGHT, 1, 1);
	//gtk_grid_attach(GTK_GRID(grid_consent), button_KO, 0, 1, 1, 1);

	gtk_fixed_put(GTK_FIXED(data->consent_fixed), grid_consent, 300, 300);

	g_signal_connect(grid_consent, "size-allocate",
			 G_CALLBACK(gui_get_consent_box_size), data);
}

/**
 * This function is called when appsink Gstreamer element receives a buffer
 */
int wait = 0;
bool first_frame = true;
static GstFlowReturn gst_new_sample_cb(GstElement *sink, CustomData *data)
{
	GstSample *sample;
	GstBuffer *app_buffer, *buffer;
	GstMapInfo info;

	if (first_frame) {
		/*  when the first frame is received, do not process anything
		 *  and only unlock the synchronization variable to start
		 *  creating the GTK main window.
		 *  This is mandatory to ensure that waylandsink is take into
		 *  account before GTK window to be sure that the GTK
		 *  transparent window will be on top of the camera preview. */
		g_print("Recieve the first frame\n");
		std::lock_guard<std::mutex> lk(cond_var_m);
		cond_var.notify_all();
		first_frame = false;

		return GST_FLOW_OK;
	}

	/* Retrieve the buffer */
	g_signal_emit_by_name (sink, "pull-sample", &sample);
	if (sample) {
		buffer = gst_sample_get_buffer (sample);

		/* Make a copy */
		app_buffer = gst_buffer_ref (buffer);

		gst_buffer_map(app_buffer, &info, GST_MAP_READ);

		std::stringstream file_path_sstr;

		if (data->state == STATE_TAKE_PICTURE_1) {
			file_path_sstr << DATASET_DIRECTORY << data->face_label;
			file_path_sstr << "_1.png";
		}
		if (data->state == STATE_TAKE_PICTURE_2) {
			file_path_sstr << DATASET_DIRECTORY << data->face_label;
			file_path_sstr << "_2.png";
		}
		if (data->state == STATE_TAKE_PICTURE_3) {
			file_path_sstr << DATASET_DIRECTORY << data->face_label;
			file_path_sstr << "_3.png";
		}
		if (data->state == STATE_TAKE_PICTURE_4) {
			file_path_sstr << DATASET_DIRECTORY << data->face_label;
			file_path_sstr << "_4.png";
		}
		if (data->state == STATE_TAKE_PICTURE_5) {
			file_path_sstr << DATASET_DIRECTORY << data->face_label;
			file_path_sstr << "_5.png";
		}
		if (data->state == STATE_TAKE_PICTURE_6) {
			file_path_sstr << DATASET_DIRECTORY << data->face_label;
			file_path_sstr << "_6.png";
		}
		if (data->state == STATE_TAKE_PICTURE_7) {
			file_path_sstr << DATASET_DIRECTORY << data->face_label;
			file_path_sstr << "_7.png";
		}
		if (data->state == STATE_TAKE_PICTURE_8) {
			file_path_sstr << DATASET_DIRECTORY << data->face_label;
			file_path_sstr << "_8.png";
		}

		/* If a file path is set, the frame coming from the camera is
		 * saved as a png.file */
		if (!file_path_sstr.str().empty()) {
			cv::Mat img_rgba;
			cv::Mat img_bgr(cv::Size(data->frame_width,
						 data->frame_height), CV_8UC3, info.data,
					cv::Mat::AUTO_STEP);
			cv::cvtColor(img_bgr, img_rgba, cv::COLOR_BGR2RGBA);
			cv::imwrite(file_path_sstr.str().c_str(), img_rgba);
			/* Call application callback only in playing state */
			gst_element_post_message(sink,
						 gst_message_new_application(GST_OBJECT(sink),
									     gst_structure_new_empty("cb-done")));
		}

		if (data->state == STATE_FINISH) {
			/* Wait 60 consecutive frame before changing the state
			 * machine */
			wait++;
			if (wait > 60) {
				wait = 0;
				gst_element_post_message(sink,
							 gst_message_new_application(GST_OBJECT(sink),
										     gst_structure_new_empty("cb-done")));
			}
		}

		gst_buffer_unmap(app_buffer, &info);
		gst_buffer_unref (app_buffer);

		/* We don't need the appsink sample anymore */
		gst_sample_unref (sample);

		return GST_FLOW_OK;
	}

	return GST_FLOW_ERROR;
}

/**
 * This function is called by Gstreamer fpsdisplaysink to get fps measurment
 */
void gst_fps_measure_display_cb(GstElement *fpsdisplaysink,
				gdouble fps,
				gdouble droprate,
				gdouble avgfps,
				gpointer data)
{
	display_avg_fps = avgfps;
}

/**
 * This function is called by Gstreamer fpsdisplaysink to get fps measurment
 */
void gst_fps_measure_processing_cb(GstElement *fpsdisplaysink,
				   gdouble fps,
				   gdouble droprate,
				   gdouble avgfps,
				   gpointer data)
{
	processing_avg_fps = avgfps;
}

/**
 * This function is called when a Gstreamer error message is posted on the bus.
 */
static void gst_error_cb (GstBus *bus, GstMessage *msg, CustomData *data) {
	GError *err;
	gchar *debug_info;

	/* Print error details on the screen */
	gst_message_parse_error (msg, &err, &debug_info);
	g_printerr("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
	g_printerr("Debugging information: %s\n", debug_info ? debug_info : "none");
	g_clear_error(&err);
	g_free(debug_info);

	/* Set the pipeline to READY (which stops camera streaming) */
	gst_element_set_state(data->pipeline, GST_STATE_READY);
}

/**
 * This function is called when a Gstreamer End-Of-Stream message is posted on
 * the bus. We just set the pipeline to READY (which stops camera streaming).
 */
static void gst_eos_cb (GstBus *bus, GstMessage *msg, CustomData *data) {
	g_print("End-Of-Stream reached.\n");
	gst_element_set_state(data->pipeline, GST_STATE_READY);
}

/**
 * This function is called when a Gstreamer "application" message is posted on
 * the bus.
 * Here we retrieve the message posted by the appsink gst_new_sample_cb callback.
 */
static void gst_application_cb (GstBus *bus, GstMessage *msg, CustomData *data) {
	if (g_strcmp0(gst_structure_get_name(gst_message_get_structure(msg)), "cb-done") == 0) {
		/* If the message is the "cb-done", update the GTK UI */
		change_state(data);
		gtk_widget_queue_draw(data->window);
	}
}

/**
 * Construct the Gstreamer pipeline used to stream camera frame and to run
 * TensorFlow Lite inference engine using appsink.
 */
static int gst_pipeline_camera_creation(CustomData *data)
{
	GstStateChangeReturn ret;
	GstElement *pipeline, *source, *dispsink, *tee, *scale, *framerate;
	GstElement *queue1, *queue2, *convert, *appsink;
	GstElement *fpsmeasure1, *fpsmeasure2;
	GstBus *bus;

	/* Create the pipeline */
	pipeline  = gst_pipeline_new("Image classification live camera");
	data->pipeline = pipeline;

	/* Create gstreamer elements */
	source      = gst_element_factory_make("v4l2src",        "camera-source");
	tee         = gst_element_factory_make("tee",            "frame-tee");
	queue1      = gst_element_factory_make("queue",          "queue-1");
	queue2      = gst_element_factory_make("queue",          "queue-2");
	convert     = gst_element_factory_make("videoconvert",   "video-convert");
	scale       = gst_element_factory_make("videoscale",     "videoscale");
	dispsink    = gst_element_factory_make("waylandsink",    "display-sink");
	appsink     = gst_element_factory_make("appsink",        "app-sink");
	framerate   = gst_element_factory_make("videorate",      "video-rate");
	fpsmeasure1 = gst_element_factory_make("fpsdisplaysink", "fps-measure1");
	fpsmeasure2 = gst_element_factory_make("fpsdisplaysink", "fps-measure2");

	if (!pipeline || !source || !tee || !queue1 || !queue2 || !convert ||
	    !scale || !dispsink || !appsink || !framerate || !fpsmeasure1 ||
	    !fpsmeasure2) {
		g_printerr("One element could not be created. Exiting.\n");
		return -1;
	}

	/* Configure the source element */
	std::stringstream device_sstr;
	device_sstr << "/dev/video" << video_device_str;
	g_object_set(G_OBJECT(source), "device", device_sstr.str().c_str(), NULL);

	/* Configure the display sink element */
	g_object_set(G_OBJECT(dispsink), "fullscreen", true, NULL);

	/* Configure the queue elements */
	g_object_set(G_OBJECT(queue1), "max-size-buffers", 1, "leaky", 2 /* downstream */, NULL);
	g_object_set(G_OBJECT(queue2), "max-size-buffers", 1, "leaky", 2 /* downstream */, NULL);

	/* Create caps based on application parameters */
	std::stringstream sourceCaps_sstr;
	sourceCaps_sstr << "video/x-raw,width=" << camera_width_str << ",height=" << camera_height_str << ",framerate=" << camera_fps_str << "/1";
	GstCaps *sourceCaps = gst_caps_from_string(sourceCaps_sstr.str().c_str());
	std::stringstream scaleCaps_sstr;
	scaleCaps_sstr << "video/x-raw,format=RGB";
	GstCaps *scaleCaps  = gst_caps_from_string(scaleCaps_sstr.str().c_str());

	/* Configure fspdisplaysink */
	g_object_set(fpsmeasure1, "signal-fps-measurements", TRUE,
		     "fps-update-interval", 2000, "text-overlay", FALSE,
		     "video-sink", dispsink, NULL);
	g_signal_connect(fpsmeasure1, "fps-measurements", G_CALLBACK(gst_fps_measure_display_cb), NULL);

	g_object_set(fpsmeasure2, "signal-fps-measurements", TRUE,
		     "fps-update-interval", 2000, "text-overlay", FALSE,
		     "video-sink", appsink, NULL);
	g_signal_connect(fpsmeasure2, "fps-measurements", G_CALLBACK(gst_fps_measure_processing_cb), NULL);

	/* Configure appsink */
	g_object_set(appsink, "emit-signals", TRUE, "sync", FALSE,
		     "max-buffers", 1, "drop", TRUE, "caps", scaleCaps, NULL);
	g_signal_connect(appsink, "new-sample", G_CALLBACK(gst_new_sample_cb), data);

	/* Add all elements into the pipeline */
	gst_bin_add_many(GST_BIN(pipeline),
			 source, framerate, tee, convert, queue1, queue2, scale,
			 fpsmeasure1, fpsmeasure2, NULL);

	/* Link the elements together */
	if (!gst_element_link_many(source, framerate, NULL)) {
		g_error("Failed to link elements (1)");
		return -2;
	}
	if (!gst_element_link_filtered(framerate, tee, sourceCaps)) {
		g_error("Failed to link elements (2)");
		return -2;
	}
	if (!gst_element_link_many(tee, queue2, convert, scale, NULL)) {
		g_error("Failed to link elements (3)");
		return -2;
	}
	if (!gst_element_link_filtered(scale, fpsmeasure2, scaleCaps)) {
		g_error("Failed to link elements (4)");
		return -2;
	}
	if (!gst_element_link_many(tee, queue1, fpsmeasure1, NULL)) {
		g_error("Failed to link elements (5)");
		return -2;
	}

	gst_caps_unref(sourceCaps);
	gst_caps_unref(scaleCaps);

	/* Instruct the bus to emit signals for each received message, and
	 * connect to the interesting signals */
	bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	gst_bus_add_signal_watch(bus);
	g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)gst_error_cb, data);
	g_signal_connect(G_OBJECT(bus), "message::eos", (GCallback)gst_eos_cb, data);
	g_signal_connect(G_OBJECT(bus), "message::application", (GCallback)gst_application_cb, data);
	gst_object_unref(bus);

	/* Set the pipeline to playing state */
	ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr("Unable to set the pipeline to the playing state.\n");
		gst_object_unref(GST_OBJECT(data->pipeline));
		return -1;
	}
	g_print("Gst pipeline now playing\n");

	return 0;
}

/**
 * This function display the help when -h or --help is passed as parameter.
 */
static void print_help(int argc, char** argv)
{
	std::cout <<
		"Usage: " << argv[0] << " <option>\n"
		"\n"
		"-v --video_device <n>:                video device (default /dev/video0)\n"
		"-w --frame_width  <val>:              width of the camera frame (default is 640)\n"
		"-h --frame_height <val>:              height of the camera frame (default is 480)\n"
		"--framerate <val>:                    framerate of the camera (default is 15fps)\n"
		"--verbose:                            enable verbose mode\n"
		"--help:                               show this help\n";
	exit(1);
}

/**
 * This function parse the parameters of the application
 */
#define OPT_FRAMERATE           1000
#define OPT_VERBOSE             1001
#define OPT_HELP                1002
void process_args(int argc, char** argv)
{
	const char* const short_opts = "v:w:h";
	const option long_opts[] = {
		{"video_device",  required_argument, nullptr, 'v'},
		{"frame_width",   required_argument, nullptr, 'w'},
		{"frame_height",  required_argument, nullptr, 'h'},
		{"framerate",     required_argument, nullptr, OPT_FRAMERATE},
		{"verbose",       no_argument,       nullptr, OPT_VERBOSE},
		{"help",          no_argument,       nullptr, OPT_HELP},
		{nullptr,         no_argument,       nullptr, 0}
	};

	while (true)
	{
		const auto opt = getopt_long(argc, argv, short_opts, long_opts, nullptr);

		if (-1 == opt)
			break;

		switch (opt)
		{
		case 'v':
			video_device_str = std::string(optarg);
			std::cout << "camera device set to: /dev/video"
				<< video_device_str << std::endl;
			break;
		case 'w':
			camera_width_str = std::string(optarg);
			std::cout << "camera frame width set to: "
				<< camera_width_str << std::endl;
			break;
		case 'h':
			camera_height_str = std::string(optarg);
			std::cout << "camera frame heightset to: "
				<< camera_height_str << std::endl;
			break;
		case OPT_FRAMERATE:
			camera_fps_str = std::string(optarg);
			std::cout << "camera framerate set to: "
				<< camera_fps_str << std::endl;
			break;
		case OPT_VERBOSE:
			verbose = true;
			std::cout << "verbose mode enabled" << std::endl;
			break;
		case OPT_HELP: // -h or --help
		case '?': // Unrecognized option
		default:
			print_help(argc, argv);
			break;
		}
	}
}

/**
 * Function called when CTRL + C or Kill signal is destected
 */
static void int_handler(int dummy)
{
	if (gtk_main_started)
		gtk_main_quit();
	else
		exit(0);
}

/**
 * Main function
 */
int main(int argc, char *argv[])
{
	CustomData data;
	int ret;

	/* Catch CTRL + C signal */
	signal(SIGINT, int_handler);
	/* Catch kill signal */
	signal(SIGTERM, int_handler);

	process_args(argc, argv);

	/* Initialize our data structure */
	data.pipeline = NULL;
	data.window = NULL;
	data.frame_width  = std::stoi(camera_width_str);
	data.frame_height = std::stoi(camera_height_str);

	/* Initialize the state machine */
	data.state = STATE_IDLE;

	/* Check if the video device is present */
	std::stringstream device_sstr;
	device_sstr << "/dev/video" << video_device_str;
	if (access(device_sstr.str().c_str(), F_OK) == -1) {
		g_printerr("ERROR: No camera connected, %s does not exist.\n", device_sstr.str().c_str());
		return 0;
	}

	/* Initialize GTK */
	gtk_init(&argc, &argv);

	/* Initialize GStreamer for camera preview use case*/
	gst_init(&argc, &argv);

	/* Create the GStreamer pipeline for camera use case */
	ret = gst_pipeline_camera_creation(&data);
	if(ret)
		return -1;

	std::unique_lock<std::mutex> lk(cond_var_m);
	cond_var.wait(lk);

	/* Set the pipeline into pause state and restart it once the
	 * database will be initialized */
	gst_element_set_state(data.pipeline, GST_STATE_PAUSED);

	/* Create the GUI */
	g_print("Start Creating GTK window\n");
	gui_create(&data);

	/* Start the GTK main loop.
	 * We will not regain control until gtk_main_quit is called. */
	gtk_main_started = true;
	gtk_main();
	gtk_main_started = false;

	/* Out of the main loop, clean up nicely */
	/* Camera preview use case */
	g_print("Returned, stopping Gst pipeline\n");
	gst_element_set_state(data.pipeline, GST_STATE_NULL);

	g_print("Deleting Gst pipeline\n");
	gst_object_unref(data.pipeline);

	/* Clean cairo surfaces */
	cairo_surface_destroy(data.brain_icon);
	cairo_surface_destroy(data.exit_icon);

	return 0;
}
