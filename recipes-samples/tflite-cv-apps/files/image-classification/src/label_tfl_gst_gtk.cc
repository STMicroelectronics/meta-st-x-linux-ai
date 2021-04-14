/*
 * label_tfl_gst_gtk.cc
 *
 * This application demonstrate a computer vision use case for image
 * classification where frames are grabbed from a camera input (/dev/videox) and
 * analyzed by a neural network model interpreted by TensorFlow Lite framework.
 *
 * Gstreamer pipeline is used to stream camera frames (using v4l2src), to
 * display a preview (using waylandsink) and to execute neural network inference
 * (using appsink).
 *
 * The result of the inference is displayed on the preview. The overlay is done
 * using GTK widget with cairo.
 *
 * This combination is quite simple and efficient in term of CPU consumption.
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
#include <numeric>
#include <opencv2/opencv.hpp>
#include <semaphore.h>
#include <sys/types.h>
#include <thread>

#include "wrapper_tfl.hpp"

/* Application parameters */
std::vector<std::string> dir_files;
std::string model_file_str;
std::string labels_file_str;
std::string image_dir_str;
std::string video_device_str  = "0";
std::string camera_fps_str    = "15";
std::string camera_width_str  = "640";
std::string camera_height_str = "480";
bool verbose = false;
bool validation = false;
float input_mean = 127.5f;
float input_std = 127.5f;
bool crop = false;
cv::Rect cropRect(0, 0, 0, 0);

/* TensorFlow Lite variables */
wrapper_tfl::Tfl_Wrapper tfl_wrapper;

struct wrapper_tfl::Config config;
struct wrapper_tfl::Label_Results results;
std::vector<std::string> labels;

/* Synchronization variables */
std::condition_variable cond_var;
std::mutex cond_var_m;

bool gtk_main_started = false;

typedef struct _FramePosition {
	int x;
	int y;
	int width;
	int height;
} FramePosition;

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
	/* Preview camera enable (else still picture use case) */
	bool preview_enabled;
	/* NN input size */
	int nn_input_width;
	int nn_input_height;
	/* Frame resolution (still picture or camera) */
	int frame_width;
	int frame_height;
	/* Frame position in fullscreen mode */
	FramePosition frame_fullscreen_pos;
	int frame_fullscreen_pos_x;
	int frame_fullscreen_pos_y;
	/* For validation purpose */
	int valid_timeout_id;
	int valid_draw_count;
	std::vector<double> valid_inference_time;

} CustomData;

/* Framerate statisitics */
gdouble display_avg_fps = 0;
gdouble nn_avg_fps = 0;

/**
 * In validation mode this function is called when timeout occurs
 * Exit application if this function is called
 */
gint valid_timeout_callback(gpointer data)
{
	/* If timeout occurs that means that camera preview and the gtk is not
	 * behaving as expected */
	g_print("Timeout: application is not behaving has expected\n");
	exit(1);
}

/**
 * This function execute an NN inference
 */
static void nn_inference(uint8_t *img)
{
	tfl_wrapper.RunInference(img, &results);
}

/**
 * This function is called when a click or touch event is recieved
 */
#define EXIT_AREA_WIDTH   50
#define EXIT_AREA_HEIGHT  50
#define BRAIN_AREA_WIDTH  75
#define BRAIN_AREA_HEIGHT 80
static gboolean gui_press_event_cb(GtkWidget *widget,
				   GdkEventButton *event,
				   CustomData *data)
{
	gdouble draw_area_width  = (gdouble)gdk_window_get_width(event->window);

	if (event->button == GDK_BUTTON_PRIMARY) {
		/* exit if event occurs in the exit area (tof right corner) */
		if ((event->x > (draw_area_width - EXIT_AREA_WIDTH)) &&
		    (event->y < EXIT_AREA_HEIGHT))
			gtk_main_quit();
	} if (event->button == GDK_BUTTON_PRIMARY) {
		/* clasify a new picture */
		if ((event->x < BRAIN_AREA_WIDTH) &&
		    (event->y < BRAIN_AREA_HEIGHT))
			gtk_widget_queue_draw(data->window);
	}

	return TRUE;
}

/**
 * This function is an helper to get frame position on the display
 */
#define WESTON_PANEL_THICKNESS 32
static void gui_compute_frame_position(GdkWindow *window,
				       CustomData *data)
{
	int window_width;
	int window_height;
	int offset_x = 0;
	int offset_y = 0;

	window_width  = gdk_window_get_width(window);
	if (data->preview_enabled) {
		window_height = gdk_window_get_height(window) + WESTON_PANEL_THICKNESS;
	} else {
		window_height = gdk_window_get_height(window) - BRAIN_AREA_HEIGHT;
		offset_y = BRAIN_AREA_HEIGHT;
	}

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
 * This function get a file randomly in a directory
 */
static std::string gui_get_files_in_directory_randomly(std::string directory)
{
	std::stringstream file_path_sstr;

	/* Get the list of all the file ins the directory if the list of the
	 * file is empty */
	if (dir_files.size() == 0) {
		DIR* dirp = opendir(directory.c_str());
		struct dirent * dp;
		while ((dp = readdir(dirp)) != NULL) {
			if ((strcmp(dp->d_name, ".") !=0) &&
			    (strcmp(dp->d_name, "..") != 0))
				dir_files.push_back(dp->d_name);
		}
		closedir(dirp);
	}

	/* Check if directory is empty */
	if (dir_files.size() == 0)
		return "";

	int random = rand() % dir_files.size();
	file_path_sstr << directory << dir_files[random];
	dir_files.erase(dir_files.begin() + random);

	return file_path_sstr.str();
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
	int window_width  = gdk_window_get_width(window);
	int offset = 0;
	std::string file;

	if (data->preview_enabled) {
		/* Camera preview use case */

		/* Set the transparent background */
		cairo_set_source_rgba (cr, 0.31, 0.32, 0.31, 0.0);
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		cairo_paint(cr);

		if (first_call) {
			/* Calculating the position of the preview in fullscreen
			 * mode. This position is compute automaticaly by the
			 * waylandsink gstreamer element and is it is not
			 * exposed. Thus we need to calculate it by our own. */
			gui_compute_frame_position(window, data);
			first_call = false;
		}
	} else {
		/* Still picture use case */

		/* As this function is called twice at the initialisation,
		 * return immediatly the first time it is called in a still
		 * picture context */
		if (first_call) {
			first_call = false;
			return FALSE;
		}

		/* Set the black background */
		cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		cairo_paint(cr);

		/* Select a picture in the directory */
		file = gui_get_files_in_directory_randomly(image_dir_str);

		/* Read and format the pciture */
		cairo_surface_t *picture;
		cv::Mat img_bgr, img_bgra, img_bgra_resized, img_nn;

		img_bgr = cv::imread(file);
		cv::cvtColor(img_bgr, img_bgra, cv::COLOR_BGR2BGRA);
		data->frame_width =  img_bgra.size().width;
		data->frame_height = img_bgra.size().height;

		/* Get final frame position and dimension and resize it */
		gui_compute_frame_position(window, data);
		cv::Size size(data->frame_fullscreen_pos.width, data->frame_fullscreen_pos.height);
		cv::resize(img_bgra, img_bgra_resized, size);

		/* Generate the cairo surface */
		int stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24,
							   data->frame_fullscreen_pos.width);

		picture = cairo_image_surface_create_for_data(img_bgra_resized.data,
							      CAIRO_FORMAT_RGB24,
							      data->frame_fullscreen_pos.width,
							      data->frame_fullscreen_pos.height,
							      stride);

		/*  Display the resized picture */
		cairo_set_source_surface(cr,
					 picture,
					 data->frame_fullscreen_pos.x,
					 data->frame_fullscreen_pos.y);
		cairo_paint(cr);
		cairo_surface_destroy(picture);

		/* Run the NN inference */
		cv::Size size_nn(data->nn_input_width, data->nn_input_height);
		cv::resize(img_bgr, img_nn, size_nn);
		cv::cvtColor(img_nn, img_nn, cv::COLOR_BGR2RGB);

		nn_inference(img_nn.data);
	}

	std::stringstream information_sstr;
	if (tfl_wrapper.IsModelQuantized())
		information_sstr << std::left  << "quant model ";
	else
		information_sstr << std::left  << "float model ";
	information_sstr << config.model_name.substr(config.model_name.find_last_of("/\\") + 1);

	std::stringstream display_fps_sstr;
	display_fps_sstr   << std::left  << std::setw(11) << "disp. fps:";
	display_fps_sstr   << std::right << std::setw(5) << std::fixed << std::setprecision(1) << display_avg_fps;
	display_fps_sstr   << std::right << std::setw(3) << "fps";

	std::stringstream inference_fps_sstr;
	inference_fps_sstr << std::left  << std::setw(11) << "inf. fps:";
	inference_fps_sstr << std::right << std::setw(5) << std::fixed << std::setprecision(1) << nn_avg_fps;
	inference_fps_sstr << std::right << std::setw(3) << "fps";

	std::stringstream inference_time_sstr;
	inference_time_sstr << std::left  << std::setw(11) << "inf. time:";
	inference_time_sstr << std::right << std::setw(5) << std::fixed << std::setprecision(1) << results.inference_time;
	inference_time_sstr << std::right << std::setw(2) << "ms";

	std::stringstream accuracy_sstr;
	accuracy_sstr       << std::left  << std::setw(11) << "accuracy:";
	accuracy_sstr       << std::right << std::setw(5) << std::fixed << std::setprecision(1) << results.accuracy[0] * 100;
	accuracy_sstr       << std::right  << std::setw(1) << "%";

	std::stringstream label_sstr;
	label_sstr          <<  labels[results.index[0]];

	/* Draw the brain icon at the top left corner */
	cairo_set_source_surface(cr, data->brain_icon, 5, 5);
	cairo_paint(cr);

	/* Draw the exit icon at the top right corner */
	cairo_set_source_surface(cr, data->exit_icon, window_width - EXIT_AREA_WIDTH, 0);
	cairo_paint(cr);

	if (data->preview_enabled) {
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

		if (crop) {
			/* draw the crop window on the preview to center image
			 * to classify */
			float ratio;
			cv::Rect cropDisplay;

			float ratio_width = (float)data->frame_fullscreen_pos.width / (float)cropRect.width;
			float ratio_height = (float)data->frame_fullscreen_pos.height / (float)cropRect.height;

			/* Adapt the cropRect to the display size */
			if (ratio_width > ratio_height)
				ratio = ratio_height;
			else
				ratio = ratio_width;

			cropDisplay.width = cropRect.width * ratio;
			cropDisplay.height = cropRect.height * ratio;
			cropDisplay.x = cropRect.x * ratio;
			cropDisplay.y = cropRect.y * ratio;

			cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);
			cairo_rectangle(cr, int(cropDisplay.x), int(cropDisplay.y), int(cropDisplay.width), int(cropDisplay.height));
			cairo_stroke(cr);
		}
	}

	/* Display inference result */
	cairo_select_font_face (cr, "monospace",
				CAIRO_FONT_SLANT_NORMAL,
				CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size (cr, 20);
	cairo_set_source_rgb (cr, 0.83, 0.0, 0.48);

	if (data->preview_enabled) {
		/* Camera preview use case */
		cairo_move_to(cr, 2, 20);
		cairo_show_text(cr, information_sstr.str().c_str());
		cairo_move_to(cr, 2, 40);
		cairo_show_text(cr, display_fps_sstr.str().c_str());
		cairo_move_to(cr, 2, 60);
		cairo_show_text(cr, inference_fps_sstr.str().c_str());
		cairo_move_to(cr, 2, 80);
		cairo_show_text(cr, inference_time_sstr.str().c_str());
		cairo_move_to(cr, 2, 100);
		cairo_show_text(cr, accuracy_sstr.str().c_str());

		/* Set the font size to display the label */
		cairo_set_font_size (cr, 55);
		offset = WESTON_PANEL_THICKNESS;
	} else {
		/* Still picture use case */
		cairo_move_to(cr, BRAIN_AREA_WIDTH + 2, 20);
		cairo_show_text(cr, information_sstr.str().c_str());
		cairo_move_to(cr, BRAIN_AREA_WIDTH + 2, 40);
		cairo_show_text(cr, inference_time_sstr.str().c_str());
		cairo_move_to(cr, BRAIN_AREA_WIDTH + 2, 60);
		cairo_show_text(cr, accuracy_sstr.str().c_str());

		/* Translate coordinate to the preview area */
		cairo_translate(cr,
				data->frame_fullscreen_pos.x,
				data->frame_fullscreen_pos.y);
		/* Set the font size to display the label */
		cairo_set_font_size (cr, 40);
		offset = 0;
	}

	/* Display the label centered in the preview area */
	cairo_text_extents_t extents;
	double x, y;

	cairo_text_extents(cr, label_sstr.str().c_str(), &extents);
	x = (data->frame_fullscreen_pos.width / 2) - ((extents.width / 2) + extents.x_bearing);
	y = data->frame_fullscreen_pos.height + (extents.y_bearing / 2) - offset;
	cairo_move_to(cr, x, y);
	cairo_show_text(cr, label_sstr.str().c_str());

	if (validation) {
		data->valid_inference_time.push_back(results.inference_time);

		/* Reload the timeout */
		g_source_remove(data->valid_timeout_id);
		data->valid_timeout_id = g_timeout_add(5000,
						      valid_timeout_callback,
						      NULL);

		if (data->preview_enabled) {
			data->valid_draw_count++;

			if (data->valid_draw_count > 100) {
				auto avg_inf_time = std::accumulate(data->valid_inference_time.begin(),
								  data->valid_inference_time.end(), 0.0) /
					data->valid_inference_time.size();
				/* Stop the timeout and properly exit the
				 * application */
				std::cout << "avg display fps= " << display_avg_fps << std::endl;
				std::cout << "avg inference fps= " << nn_avg_fps << std::endl;
				std::cout << "avg inference time= " << avg_inf_time << std::endl;
				g_source_remove(data->valid_timeout_id);
				gtk_main_quit();
			}
		} else {
			/* Get the name of the file without extension and underscore */
			std::string file_name = std::filesystem::path(file).stem();
			file_name = file_name.substr(0, file_name.find('_'));

			/* Verify that the inference output result matches the file
			 * name. Else exit the application: validation is failing */
			std::cout << "name extract from the picture file: "
				<< std::left  << std::setw(32) << file_name
				<<  "label: " << label_sstr.str() << std::endl;
			if (file_name.compare(label_sstr.str()) != 0) {
				std::cout << "Inference result mismatch the file name\n";
				exit(1);
			}

			/* Continue over all files */
			if (dir_files.size() == 0) {
				auto avg_inf_time = std::accumulate(data->valid_inference_time.begin(),
								  data->valid_inference_time.end(), 0.0) /
					data->valid_inference_time.size();
				std::cout << "avg inference time= " << avg_inf_time << " ms\n";
				g_source_remove(data->valid_timeout_id);
				gtk_main_quit();
			} else {
				gtk_widget_queue_draw(data->window);
			}
		}
	}

	return FALSE;
}

/**
 * This function is creating GTK widget to create the window to display UI
 * information
 */
static void gui_create(CustomData *data)
{
	GtkWidget *drawing_area;

	if (data->preview_enabled)
		data->brain_icon = cairo_image_surface_create_from_png("/usr/local/demo-ai/computer-vision/tflite-image-classification/bin/resources/ST7079_AI_neural_pink_65x80.png");
	else
		data->brain_icon = cairo_image_surface_create_from_png("/usr/local/demo-ai/computer-vision/tflite-image-classification/bin/resources/ST7079_AI_neural_pink_65x80_next_inference.png");
	data->exit_icon = cairo_image_surface_create_from_png("/usr/local/demo-ai/computer-vision/tflite-image-classification/bin/resources/close_50x50_pink.png");

	/* Create the window */
	data->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(data->window), "delete-event", G_CALLBACK (gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(data->window), "destroy", G_CALLBACK (gtk_main_quit), NULL);
	/* Remove title bar */
	gtk_window_set_decorated(GTK_WINDOW (data->window), FALSE);
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
}

/**
 * This function is called when appsink Gstreamer element receives a buffer
 */
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
		 *  This is mandatory to ensure that wayandsink is take into
		 *  account before GTK window to be sure that the GTK
		 *  transparent window will be on top of the camera preview. */
		g_print("Receive the first frame\n");
		std::lock_guard<std::mutex> lk(cond_var_m);
		cond_var.notify_all();
		first_frame = false;
	}

	/* Retrieve the buffer */
	g_signal_emit_by_name (sink, "pull-sample", &sample);
	if (sample) {
		buffer = gst_sample_get_buffer (sample);

		/* Make a copy */
		app_buffer = gst_buffer_ref (buffer);

		gst_buffer_map(app_buffer, &info, GST_MAP_READ);

		/* Execute the inference */
		nn_inference(info.data);

		gst_buffer_unmap(app_buffer, &info);
		gst_buffer_unref (app_buffer);

		/* We don't need the appsink sample anymore */
		gst_sample_unref (sample);

		/* Call application callback only in playing state */
		gst_element_post_message(sink,
					 gst_message_new_application(GST_OBJECT(sink),
								     gst_structure_new_empty("inference-done")));

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
void gst_fps_measure_nn_cb(GstElement *fpsdisplaysink,
			   gdouble fps,
			   gdouble droprate,
			   gdouble avgfps,
			   gpointer data)
{
	nn_avg_fps = avgfps;
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
	if (g_strcmp0(gst_structure_get_name(gst_message_get_structure(msg)), "inference-done") == 0) {
		/* If the message is the "inference-done", update the GTK UI */
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
	GstElement *queue1, *queue2, *convert, *appsink, *framecrop;
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
	framecrop   = gst_element_factory_make("videocrop",      "videocrop");
	scale       = gst_element_factory_make("videoscale",     "videoscale");
	dispsink    = gst_element_factory_make("waylandsink",    "display-sink");
	appsink     = gst_element_factory_make("appsink",        "app-sink");
	framerate   = gst_element_factory_make("videorate",      "video-rate");
	fpsmeasure1 = gst_element_factory_make("fpsdisplaysink", "fps-measure1");
	fpsmeasure2 = gst_element_factory_make("fpsdisplaysink", "fps-measure2");


	if (!pipeline || !source || !tee || !queue1 || !queue2 || !convert ||
	    !scale || !dispsink || !appsink || !framerate || !fpsmeasure1 ||
	    !fpsmeasure2 || !framecrop) {
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
	scaleCaps_sstr << "video/x-raw,format=RGB,width=" << data->nn_input_width << ",height=" << data->nn_input_height;
	GstCaps *scaleCaps  = gst_caps_from_string(scaleCaps_sstr.str().c_str());

	/* Configure fspdisplaysink */
	g_object_set(fpsmeasure1, "signal-fps-measurements", TRUE,
		     "fps-update-interval", 2000, "text-overlay", FALSE,
		     "video-sink", dispsink, NULL);
	g_signal_connect(fpsmeasure1, "fps-measurements", G_CALLBACK(gst_fps_measure_display_cb), NULL);

	g_object_set(fpsmeasure2, "signal-fps-measurements", TRUE,
		     "fps-update-interval", 2000, "text-overlay", FALSE,
		     "video-sink", appsink, NULL);
	g_signal_connect(fpsmeasure2, "fps-measurements", G_CALLBACK(gst_fps_measure_nn_cb), NULL);

	/* Configure the videocrop */
	if (crop) {
		/* Crop requested */
		int left   = cropRect.x;
		int right  = data->frame_width - left - cropRect.width;
		int top    = cropRect.y;
		int bottom = data->frame_height - top - cropRect.height;
		g_object_set(framecrop, "left", left, "right", right,
			     "top", top, "bottom", bottom, NULL);
	} else {
		/* No crop requested */
		g_object_set(framecrop, "left", 0, "right", 0,
			     "top", 0, "bottom", 0, NULL);
	}

	/* Configure appsink */
	g_object_set(appsink, "emit-signals", TRUE, "sync", FALSE,
		     "max-buffers", 1, "drop", TRUE, "caps", scaleCaps, NULL);
	g_signal_connect(appsink, "new-sample", G_CALLBACK(gst_new_sample_cb), data);

	/* Add all elements into the pipeline */
	gst_bin_add_many(GST_BIN(pipeline),
			 source, framerate, tee, convert, queue1, queue2, scale,
			 framecrop, fpsmeasure1, fpsmeasure2, NULL);

	/* Link the elements together */
	if (!gst_element_link_many(source, framerate, NULL)) {
		g_error("Failed to link elements (1)");
		return -2;
	}
	if (!gst_element_link_filtered(framerate, tee, sourceCaps)) {
		g_error("Failed to link elements (2)");
		return -2;
	}
	if (!gst_element_link_many(tee, queue2, convert, framecrop, scale, NULL)) {
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
		"Usage: " << argv[0] << " -m <model .tflite> -l <label .txt file>\n"
		"\n"
		"-m --model_file <.tflite file path>:  .tflite model to be executed\n"
		"-l --label_file <label file path>:    name of file containing labels\n"
		"-i --image <directory path>:          image directory with image to be classified\n"
		"-v --video_device <n>:                video device (default /dev/video0)\n"
		"--crop:                               if set, the nn input image is cropped (with the expected nn aspect ratio) before being resized,\n"
		"                                      else the nn imput image is only resized to the nn input size (could cause picture deformation).\n"
		"--frame_width  <val>:                 width of the camera frame (default is 640)\n"
		"--frame_height <val>:                 height of the camera frame (default is 480)\n"
		"--framerate <val>:                    framerate of the camera (default is 15fps)\n"
		"--input_mean <val>:                   model input mean (default is 127.5)\n"
		"--input_std  <val>:                   model input standard deviation (default is 127.5)\n"
		"--verbose:                            enable verbose mode\n"
		"--validation:                         enable the validation mode\n"
		"--help:                               show this help\n";
	exit(1);
}

/**
 * This function parse the parameters of the application -m and -l ar
 * mandatory.
 */
#define OPT_FRAME_WIDTH  1000
#define OPT_FRAME_HEIGHT 1001
#define OPT_FRAMERATE    1002
#define OPT_INPUT_MEAN   1003
#define OPT_INPUT_STD    1004
#define OPT_VERBOSE      1005
#define OPT_VALIDATION   1006
void process_args(int argc, char** argv)
{
	const char* const short_opts = "m:l:i:v:c:h";
	const option long_opts[] = {
		{"model_file",   required_argument, nullptr, 'm'},
		{"label_file",   required_argument, nullptr, 'l'},
		{"image",        required_argument, nullptr, 'i'},
		{"video_device", required_argument, nullptr, 'v'},
		{"crop",         no_argument,       nullptr, 'c'},
		{"frame_width",  required_argument, nullptr, OPT_FRAME_WIDTH},
		{"frame_height", required_argument, nullptr, OPT_FRAME_HEIGHT},
		{"framerate",    required_argument, nullptr, OPT_FRAMERATE},
		{"input_mean",   required_argument, nullptr, OPT_INPUT_MEAN},
		{"input_std",    required_argument, nullptr, OPT_INPUT_STD},
		{"verbose",      no_argument,       nullptr, OPT_VERBOSE},
		{"validation",   no_argument,       nullptr, OPT_VALIDATION},
		{"help",         no_argument,       nullptr, 'h'},
		{nullptr,        no_argument,       nullptr, 0}
	};

	while (true)
	{
		const auto opt = getopt_long(argc, argv, short_opts, long_opts, nullptr);

		if (-1 == opt)
			break;

		switch (opt)
		{
		case 'm':
			model_file_str = std::string(optarg);
			std::cout << "model file set to: " << model_file_str << std::endl;
			break;
		case 'l':
			labels_file_str = std::string(optarg);
			std::cout << "label file set to: " << labels_file_str << std::endl;
			break;
		case 'i':
			image_dir_str = std::string(optarg);
			std::cout << "image directory set to: " << image_dir_str << std::endl;
			break;
		case 'v':
			video_device_str = std::string(optarg);
			std::cout << "camera device set to: /dev/video" << video_device_str << std::endl;
			break;
		case 'c':
			crop = true;
			std::cout << "nn input image will be cropped then resized" << std::endl;
			break;
		case OPT_FRAME_WIDTH:
			camera_width_str = std::string(optarg);
			std::cout << "camera frame width set to: " << camera_width_str << std::endl;
			break;
		case OPT_FRAME_HEIGHT:
			camera_height_str = std::string(optarg);
			std::cout << "camera frame heightset to: " << camera_height_str << std::endl;
			break;
		case OPT_FRAMERATE:
			camera_fps_str = std::string(optarg);
			std::cout << "camera framerate set to: " << camera_fps_str << std::endl;
			break;
		case OPT_INPUT_MEAN:
			input_mean = std::stof(optarg);
			std::cout << "input mean to: " << input_mean << std::endl;
			break;
		case OPT_INPUT_STD:
			input_std = std::stof(optarg);
			std::cout << "input standard deviation set to: " << input_std << std::endl;
			break;
		case OPT_VERBOSE:
			verbose = true;
			std::cout << "verbose mode enabled" << std::endl;
			break;
		case OPT_VALIDATION:
			validation = true;
			std::cout << "application started in validation mode" << std::endl;
			break;
		case 'h': // -h or --help
		case '?': // Unrecognized option
		default:
			print_help(argc, argv);
			break;
		}
	}

	if (model_file_str.empty() || labels_file_str.empty())
		print_help(argc, argv);
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
	size_t label_count;

	/* Catch CTRL + C signal */
	signal(SIGINT, int_handler);
	/* Catch kill signal */
	signal(SIGTERM, int_handler);

	process_args(argc, argv);

	/* Initialize our data structure */
	data.pipeline = NULL;
	data.window = NULL;

	/* If image_dir is set by the user, test data picture are used instead
	 * of camera frames */
	if (image_dir_str.empty()) {
		data.frame_width  = std::stoi(camera_width_str);
		data.frame_height = std::stoi(camera_height_str);
		data.preview_enabled = true;

		/* Check if the video device is present */
		std::stringstream device_sstr;
		device_sstr << "/dev/video" << video_device_str;
		if (access(device_sstr.str().c_str(), F_OK) == -1) {
			g_printerr("ERROR: No camera connected, %s does not exist.\n", device_sstr.str().c_str());
			exit(1);
		}
	} else {
		data.preview_enabled = false;

		/* Check if directory is empty */
		std::string file = gui_get_files_in_directory_randomly(image_dir_str);
		if (file.empty()) {
			g_printerr("ERROR: Image directory %s is empty\n", image_dir_str.c_str());
			exit(1);
		} else {
			/* Reset the dir_file variable */
			dir_files.erase(dir_files.begin(), dir_files.end());
		}
	}

	/* TensorFlow Lite wrapper initialization */
	config.verbose = verbose;
	config.model_name = model_file_str;
	config.labels_file_name = labels_file_str;
	config.input_mean = input_mean;
	config.input_std = input_std;
	config.number_of_threads = 2;
	config.number_of_results = 5;

	tfl_wrapper.Initialize(&config);

	if (tfl_wrapper.ReadLabelsFile(config.labels_file_name, &labels, &label_count) != kTfLiteOk)
		exit(1);

	/* Get model input size */
	data.nn_input_width = tfl_wrapper.GetInputWidth();
	data.nn_input_height = tfl_wrapper.GetInputHeight();

	if (data.preview_enabled) {
		if (crop) {
			int frame_width = data.frame_width;
			int frame_height = data.frame_height;
			int nn_input_width = data.nn_input_width;
			int nn_input_height = data.nn_input_height;

			float nn_input_aspect_ratio = (float)nn_input_width / (float)nn_input_height;

			/* Setup a rectangle to define the crop region */
			if (nn_input_aspect_ratio >  1) {
				cropRect.width = frame_width;
				cropRect.height = int((float)frame_width / (float)nn_input_aspect_ratio);
				cropRect.x = 0;
				cropRect.y = (int)((frame_height - cropRect.height) / 2);
			} else {
				cropRect.width = int((float)frame_height * (float)nn_input_aspect_ratio);
				cropRect.height = frame_height;
				cropRect.x = (int)((frame_width - cropRect.width) / 2);
				cropRect.y = 0;
			}
		}
	}

	/* Initialize GTK */
	gtk_init(&argc, &argv);

	if (data.preview_enabled) {
		/* Initialize GStreamer for camera preview use case*/
		gst_init(&argc, &argv);

		/* Create the GStreamer pipeline for camera use case */
		ret = gst_pipeline_camera_creation(&data);
		if(ret)
			exit(1);

		auto now = std::chrono::system_clock::now();
		auto delay = std::chrono::milliseconds(5000);
		std::unique_lock<std::mutex> lk(cond_var_m);
		cond_var.wait_until(lk, now + delay);
	}

	/* Start a timeout timer in validation process to close application if
	 * timeout occurs */
	if (validation) {
		data.valid_draw_count = 0;
		data.valid_timeout_id = g_timeout_add(10000,
						      valid_timeout_callback,
						      NULL);
	}

	/* Create the GUI */
	g_print("Start Creating GTK window\n");
	gui_create(&data);

	/* Start the GTK main loop.
	 * We will not regain control until gtk_main_quit is called. */
	gtk_main_started = true;
	gtk_main();
	gtk_main_started = false;

	/* Out of the main loop, clean up nicely */
	if (data.preview_enabled) {
		/* Camera preview use case */
		g_print("Returned, stopping Gst pipeline\n");
		gst_element_set_state(data.pipeline, GST_STATE_NULL);

		g_print("Deleting Gst pipeline\n");
		gst_object_unref(data.pipeline);
	}

	/* Clean cairo surfaces */
	cairo_surface_destroy(data.brain_icon);
	cairo_surface_destroy(data.exit_icon);

	return 0;
}
