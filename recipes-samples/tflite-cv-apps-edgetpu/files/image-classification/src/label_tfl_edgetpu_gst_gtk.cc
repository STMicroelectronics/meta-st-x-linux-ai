/*
 * label_tfl_edgetpu_gst_gtk.cc
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

#include <filesystem>
#include <getopt.h>
#include <glib.h>
#include <gtk/gtk.h>
#include <numeric>
#include <opencv2/opencv.hpp>
#include <sys/types.h>
#include <thread>
#include <gst/video/videooverlay.h>
#include <wayland/wayland.h>
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#else
#error "Wayland is not supported in GTK+"
#endif

#include "wrapper_tfl_edgetpu.hpp"

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
struct wrapper_tfl::Tfl_WrapperEdgeTPU tfl_wrapper_tpu;
struct wrapper_tfl::Config config;
struct wrapper_tfl::Label_Results results;
std::vector<std::string> labels;

bool gtk_main_started = false;
bool exit_application = false;

/* Ressource directory on board */
#define RESOURCES_DIRECTORY "/usr/local/demo-ai/computer-vision/tflite-image-classification-edgetpu/bin/resources/"

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
	GtkWidget *window_main;
	GtkWidget *window_overlay;

	/* The drawing area where the camera stream is shown */
	GtkWidget *video;
	GstVideoOverlay *video_overlay;
	GtkAllocation video_allocation;

	/* Text widget to display info about
	 * - either the display framerate, and the inference time
	 * - in fps or ms and the accuracy of the model
	 */
	GtkWidget *info_disp_fps;
	GtkWidget *info_inf_fps;
	GtkWidget *info_inf_time;
	GtkWidget *info_accuracy;
	GtkWidget *info_box_main;
	GtkWidget *info_box_ov;
	GtkWidget *exit_box_main;
	GtkWidget *exit_box_ov;
	std::stringstream label_sstr;

	/* window resolution */
	int window_width;
	int window_height;

	/* UI parameters (values will depends on display size) */
	int ui_cairo_font_size_label;
	int ui_cairo_font_size;
	double ui_box_line_width;
	int ui_weston_panel_thickness;

	/* ST icon widget */
	GtkWidget *st_icon_main;
	GtkWidget *st_icon_ov;

	/* exit icon widget */
	GtkWidget *exit_icon_main;
	GtkWidget *exit_icon_ov;

	/* Preview camera enable (else still picture use case) */
	bool preview_enabled;

	/* NN input size */
	int nn_input_width;
	int nn_input_height;

	/* Frame resolution (still picture or camera) */
	int frame_width;
	int frame_height;

	/* Frame resolution and scaling factor */
	float x_scale;
	float y_scale;

	/* drawing area resolution */
	int widget_draw_width;
	int widget_draw_height;

	/* For validation purpose */
	int valid_timeout_id;
	int valid_draw_count;
	std::vector<double> valid_inference_time;
	std::string file;

	/* aspect ratio */
	FramePosition frame_disp_pos;

	/*  Still picture variables */
	bool new_inference;
	cv::Mat img_to_display;

} CustomData;

/* Framerate statistics */
gdouble display_avg_fps = 0;
gdouble app_avg_fps = 0;

/**
* This function is called when we need to setup the dcmipp camera
*/
static void setup_dcmipp()
{
	std::stringstream config_cam_sstr;
	std::stringstream config_dcmipp_parallel_sstr;
	std::stringstream config_dcmipp_dump_postproc0_sstr;
	std::stringstream config_dcmipp_dump_postproc1_sstr;
	std::stringstream config_dcmipp_dump_postproc_crop_sstr;

	config_cam_sstr << "media-ctl -d /dev/media0 --set-v4l2 \"\'ov5640 1-003c\':0[fmt:RGB565_2X8_LE/" << camera_width_str << "x" << camera_height_str << "@1/" << camera_fps_str << " field:none]\"";
	system(config_cam_sstr.str().c_str());

	config_dcmipp_parallel_sstr << "media-ctl -d /dev/media0 --set-v4l2 \"\'dcmipp_parallel\':0[fmt:RGB565_2X8_LE/" << camera_width_str << "x" << camera_height_str <<"]\"";
	system(config_dcmipp_parallel_sstr.str().c_str());

	config_dcmipp_dump_postproc0_sstr << "media-ctl -d /dev/media0 --set-v4l2 \"\'dcmipp_dump_postproc\':0[fmt:RGB565_2X8_LE/" << camera_width_str << "x" << camera_height_str <<"]\"";
	system(config_dcmipp_dump_postproc0_sstr.str().c_str());

	config_dcmipp_dump_postproc1_sstr << "media-ctl -d /dev/media0 --set-v4l2 \"\'dcmipp_dump_postproc\':1[fmt:RGB565_2X8_LE/" << camera_width_str << "x" << camera_height_str <<"]\"";
	system(config_dcmipp_dump_postproc1_sstr.str().c_str());

	config_dcmipp_dump_postproc_crop_sstr << "media-ctl -d /dev/media0 --set-v4l2 \"\'dcmipp_dump_postproc\':1[crop:(0,0)/" << camera_width_str << "x" << camera_height_str << "]\"";
	system(config_dcmipp_dump_postproc_crop_sstr.str().c_str());

g_print("dcmipp congiguration passed \n");
}

/**
* This function is called when we need to pass a shell cmd and
* recover the output
* */
std::string shell_cmd_exec(const char* cmd) {
    std::array<char, 128> char_buff;
    std::string cmd_output;
    std::unique_ptr<FILE, decltype(&pclose)> cmd_pipe(popen(cmd, "r"), pclose);
    if (!cmd_pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(char_buff.data(), char_buff.size(), cmd_pipe.get()) != nullptr) {
        cmd_output += char_buff.data();
    }
    return cmd_output;
}

/**
 * This function is called when the ST icon image is clicked
 */
static gboolean st_icon_cb(GtkWidget *event_box,
			   GdkEventButton *event,
			   CustomData *data)
{
	if (!data->preview_enabled){
		g_print("ST icon clicked: new inference\n");
		data->new_inference = true;
	}
	return TRUE;
}

/**
 * This function is called when the exit icon image is clicked
 */

static gboolean exit_icon_cb(GtkWidget *event_box,
			     GdkEventButton *event,
			     CustomData *data)
{
	g_print("Exit icon clicked: exit\n");
	gtk_main_quit();
	return TRUE;
}

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
	tfl_wrapper_tpu.RunInference(img, &results);
}

/**
 * This function is an helper to get frame position on the display
 */
static void compute_frame_position(CustomData *data)
{
	int window_width = data->widget_draw_width;
	int window_height = data->widget_draw_height;
	int offset_x = 0;
	int offset_y = 0;

	float width_ratio   = (float)window_width / (float)data->frame_width;
	float height_ratio  = (float)window_height / (float)data->frame_height;

	if (width_ratio > height_ratio) {
		data->frame_disp_pos.width = (int)((float)data->frame_width * height_ratio);
		data->frame_disp_pos.height = (int)((float)data->frame_height * height_ratio);
		data->frame_disp_pos.x = (window_width - data->frame_disp_pos.width) / 2;
		data->frame_disp_pos.y = offset_y;
	} else {
		data->frame_disp_pos.width = (int)((float)data->frame_width * width_ratio);
		data->frame_disp_pos.height = (int)((float)data->frame_height * width_ratio);
		data->frame_disp_pos.x = offset_x;
		data->frame_disp_pos.y = (window_height - data->frame_disp_pos.height) / 2;
	}
}

/**
 * This function get a file randomly in a directory
 */
static std::string get_files_in_directory_randomly(std::string directory)
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
 * This function is an idle function waiting for a new picture
 * to be infered
 */
static gboolean infer_new_picture(CustomData *data)
{
	if (exit_application) {
		gtk_main_quit();
		return FALSE;
	}

	if (data->new_inference){
		/* Select a picture in the directory */
		data->file = get_files_in_directory_randomly(image_dir_str);
		/* Read and format the picture */
		cv::Mat img_bgr, img_bgra, img_nn;

		img_bgr = cv::imread(data->file);
		cv::cvtColor(img_bgr, img_bgra, cv::COLOR_BGR2BGRA);
		data->frame_width =  img_bgra.size().width;
		data->frame_height = img_bgra.size().height;
		compute_frame_position(data);

		/* Get final frame position and dimension and resize it */
		cv::Size size(data->frame_disp_pos.width,data->frame_disp_pos.height);
		cv::resize(img_bgra, data->img_to_display, size);

		/* prepare the inference */
		cv::Size size_nn(data->nn_input_width, data->nn_input_height);
		cv::resize(img_bgr, img_nn, size_nn);
		cv::cvtColor(img_nn, img_nn, cv::COLOR_BGR2RGB);

		nn_inference(img_nn.data);

		std::stringstream inference_time_sstr;
		inference_time_sstr << std::right << std::setw(5) << std::fixed << std::setprecision(1) << results.inference_time;
		inference_time_sstr << std::right << std::setw(2) << " ms ";
		gtk_label_set_text(GTK_LABEL(data->info_inf_time),inference_time_sstr.str().c_str());

		std::stringstream label_sstr;
		label_sstr << labels[results.index[0]];

		if (validation) {
			/* Still picture use case */
			data->valid_inference_time.push_back(results.inference_time);
			/* Reload the timeout */
			g_source_remove(data->valid_timeout_id);
			data->valid_timeout_id = g_timeout_add(5000,
							       valid_timeout_callback,
							       NULL);
			/* Get the name of the file without extension and underscore */
			std::string file_name = std::filesystem::path(data->file).stem();
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
				std::cout << "\navg inference time= " << avg_inf_time << " ms\n";
				g_source_remove(data->valid_timeout_id);
				gtk_widget_queue_draw(data->window_main);
				gtk_widget_queue_draw(data->window_overlay);
				exit_application = true;
			} else {
				data->new_inference = true;
			}
		} else {
			data->new_inference = false;
		}
		/*  Refresh main window and overlay */
		gtk_widget_queue_draw(data->window_overlay);
		gtk_widget_queue_draw(data->window_main);
	}
	return TRUE;
}

void gui_display_outlined_text(cairo_t *cr,
			       const char* text)
{
	cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
	cairo_text_path(cr, text);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_set_line_width(cr, 0.5);
	cairo_stroke(cr);
}

bool first_load = true;
static void gui_gtk_style(CustomData *data)
{
	std::stringstream css_sstr;
	css_sstr << RESOURCES_DIRECTORY;
	if (first_load){
		css_sstr << "Default.css";
		first_load = false;
	} else {
		if (data->window_width > 800)
			css_sstr << "widgets_720p.css";
		else if (data->window_width > 480)
			css_sstr << "widgets_480p.css";
		else
			css_sstr << "widgets_272p.css";
	}
	GtkCssProvider *cssProvider = gtk_css_provider_new();
	gtk_css_provider_load_from_path (cssProvider,
					 css_sstr.str().c_str(),
					 NULL);
	gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
						  GTK_STYLE_PROVIDER(cssProvider),
						  GTK_STYLE_PROVIDER_PRIORITY_USER);
}

/**
 * This function is an helper to set UI variable according to the display size
 */
static void gui_set_ui_parameters(CustomData *data)
{
	int window_height = data->window_height;
	/* Default UI parameter to target 720p display */
	int ui_icon_exit_width = 50;
	int ui_icon_exit_height = 50;
	int ui_icon_st_width = 130;
	int ui_icon_st_height = 160;
	data->ui_cairo_font_size = 25;
	data->ui_cairo_font_size_label = 40;

	if (window_height <= 272) {
		/* Display 480x272 */
		std::cout << "Adjust UI param to 272p display" <<  std::endl;
		data->ui_cairo_font_size_label = 20;
		data->ui_cairo_font_size = 12;
		ui_icon_exit_width = 25;
		ui_icon_exit_height = 25;
		ui_icon_st_width = 42;
		ui_icon_st_height = 52;
	} else if (window_height <= 480) {
		/* Display 800x480 */
		std::cout << "Adjust UI param to 480p display" <<  std::endl;
		data->ui_cairo_font_size_label = 30;
		data->ui_cairo_font_size = 25;
		ui_icon_exit_width = 50;
		ui_icon_exit_height = 50;
		ui_icon_st_width = 65;
		ui_icon_st_height = 80;
	}

	gui_gtk_style(data);

	/* set the icons */
	std::stringstream st_icon_sstr;
	st_icon_sstr << RESOURCES_DIRECTORY << "st_icon_";
	if (!data->preview_enabled)
		st_icon_sstr << "next_inference_";
	st_icon_sstr << ui_icon_st_width << "x" << ui_icon_st_height << ".png";
	gtk_image_set_from_file(GTK_IMAGE(data->st_icon_main), st_icon_sstr.str().c_str());
	gtk_image_set_from_file(GTK_IMAGE(data->st_icon_ov), st_icon_sstr.str().c_str());

	std::stringstream exit_icon_sstr;
	exit_icon_sstr << RESOURCES_DIRECTORY << "exit_";
	exit_icon_sstr << ui_icon_exit_width << "x" << ui_icon_exit_height << ".png";
	gtk_image_set_from_file(GTK_IMAGE(data->exit_icon_main), exit_icon_sstr.str().c_str());
	gtk_image_set_from_file(GTK_IMAGE(data->exit_icon_ov), exit_icon_sstr.str().c_str());
}

/**
 * This function is called each time the GTK UI is redrawn
 */
bool first_call_overlay = true;
static gboolean gui_draw_overlay_cb(GtkWidget *widget,
				    cairo_t *cr,
				    CustomData *data)
{
	/* Get drawing area informations */
	int draw_width = gtk_widget_get_allocated_width(widget);
	int draw_height= gtk_widget_get_allocated_height(widget);

	/* Updating the information with the new inference results */
	std::stringstream display_fps_sstr;
	display_fps_sstr   << std::right << std::setw(5) << std::fixed << std::setprecision(1) << display_avg_fps;
	display_fps_sstr   << std::right << std::setw(3) << " fps ";

	std::stringstream inference_fps_sstr;
	inference_fps_sstr << std::right << std::setw(5) << std::fixed << std::setprecision(1) << app_avg_fps;
	inference_fps_sstr << std::right << std::setw(3) << " fps ";

	std::stringstream inference_time_sstr;
	inference_time_sstr << std::right << std::setw(5) << std::fixed << std::setprecision(1) << results.inference_time;
	inference_time_sstr << std::right << std::setw(2) << " ms ";

	std::stringstream accuracy_sstr;
	accuracy_sstr << std::right << std::setw(5) << std::fixed << std::setprecision(1) << results.accuracy[0] * 100;
	accuracy_sstr << std::right  << std::setw(1) << " % ";

	/* set the font and the size for the label*/
	cairo_select_font_face (cr, "monospace",
				CAIRO_FONT_SLANT_NORMAL,
				CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size (cr, data->ui_cairo_font_size_label);

	/*  Update the gtk labels with latest information */
	if (data->preview_enabled) {
		/* Camera preview use case */
		if (first_call_overlay){
			first_call_overlay = false;
			return FALSE;
		} else {
			gtk_label_set_text(GTK_LABEL(data->info_disp_fps),display_fps_sstr.str().c_str());
			gtk_label_set_text(GTK_LABEL(data->info_inf_fps),inference_fps_sstr.str().c_str());
			gtk_label_set_text(GTK_LABEL(data->info_inf_time),inference_time_sstr.str().c_str());
			gtk_label_set_text(GTK_LABEL(data->info_accuracy),accuracy_sstr.str().c_str());
		}
		if(exit_application)
			gtk_main_quit();
	} else {
		/* Still picture use case */
		if (first_call_overlay){
			first_call_overlay = false;
			/* add the inference function in idlle after the initialization of
			 * the GUI */
			g_idle_add((GSourceFunc)infer_new_picture,data);
			data->new_inference = true;
			return FALSE;
		} else {
			gtk_label_set_text(GTK_LABEL(data->info_accuracy),accuracy_sstr.str().c_str());
		}
	}

	/* Display the label centered in the preview area */
	std::stringstream label_sstr;
	cairo_text_extents_t extents;
	double x, y;
	label_sstr << labels[results.index[0]];
	if (!data->preview_enabled) {
		draw_width = data->widget_draw_width;
		draw_height=  data->widget_draw_height;
	}
	cairo_text_extents(cr, label_sstr.str().c_str(), &extents);
	x = (draw_width/ 2) - ((extents.width / 2) + extents.x_bearing);
	y = draw_height + (extents.y_bearing / 2);
	cairo_move_to(cr, x, y);
	gui_display_outlined_text(cr, label_sstr.str().c_str());
	cairo_fill (cr);

	/* Validation mode */
	if (validation) {
		if (data->preview_enabled) {
			data->valid_inference_time.push_back(results.inference_time);
			/* Reload the timeout */
			g_source_remove(data->valid_timeout_id);
			data->valid_timeout_id = g_timeout_add(5000,
							       valid_timeout_callback,
							       NULL);
			data->valid_draw_count++;
			if (data->valid_draw_count > 100) {
				auto avg_inf_time = std::accumulate(data->valid_inference_time.begin(),
								    data->valid_inference_time.end(), 0.0) /
								    data->valid_inference_time.size();
				/* Stop the timeout and properly exit the
				 * application */
				std::cout << "avg display fps= " << display_avg_fps << std::endl;
				std::cout << "avg inference fps= " << app_avg_fps << std::endl;
				std::cout << "avg inference time= " << avg_inf_time << std::endl;
				g_source_remove(data->valid_timeout_id);
				gtk_main_quit();
			}
		}
	}
	return TRUE;
}

/**
 * This function is creating GTK widget to create the window to overlay UI
 * information
 */
static void gui_create_overlay(CustomData *data)
{
	/* HBox to hold all other boxes */
	GtkWidget *main_box;
	/* Hbox to hold the drawing area where the boxes are displayed */
	GtkWidget *drawing_box;
	/* Drawing area to handle labels */
	GtkWidget *drawing_area;
	GtkWidget *still_pict_draw;
	/* Event box for ST icon */
	GtkWidget *st_icon_event_box;
	/* Event box for exit icon */
	GtkWidget *exit_icon_event_box;
	/* Labels for information */
	GtkWidget *title_disp_fps;
	GtkWidget *title_inf_fps;
	GtkWidget *title_inf_time;
	GtkWidget *title_inf_accuracy;

	/* Create the window */
	data->window_overlay = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_app_paintable(data->window_overlay, TRUE);
	g_signal_connect(G_OBJECT(data->window_overlay), "delete-event",
			 G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(data->window_overlay), "destroy",
			 G_CALLBACK(gtk_main_quit), NULL);

	/* Remove title bar */
	gtk_window_set_decorated(GTK_WINDOW(data->window_overlay), FALSE);

	/* Maximize the window size */
	gtk_window_maximize(GTK_WINDOW(data->window_overlay));

	/* Create the ST icon widget */
	data->st_icon_ov = gtk_image_new();
	st_icon_event_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(st_icon_event_box), data->st_icon_ov);
	g_signal_connect(G_OBJECT(st_icon_event_box),"button_press_event",
			 G_CALLBACK(st_icon_cb), data);

	/* Create the exit icon widget */
	data->exit_icon_ov = gtk_image_new();
	exit_icon_event_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(exit_icon_event_box), data->exit_icon_ov);
	g_signal_connect(G_OBJECT(exit_icon_event_box),"button_press_event",
			 G_CALLBACK(exit_icon_cb), data);

	if (data->preview_enabled) {
		/*  Camera preview use case */
		/* Create the drawing area to draw text on it using cairo */
		drawing_area = gtk_drawing_area_new();
		gtk_widget_set_app_paintable(drawing_area, TRUE);
		g_signal_connect(G_OBJECT(drawing_area), "draw",
				 G_CALLBACK(gui_draw_overlay_cb), data);

		/* Create the gtk labels to display nn inference information*/
		title_disp_fps = gtk_label_new("disp. fps:   ");
		gtk_widget_set_halign(title_disp_fps,GTK_ALIGN_START);
		title_inf_fps = gtk_label_new("inf. fps:    ");
		gtk_widget_set_halign(title_inf_fps,GTK_ALIGN_START);
		title_inf_time = gtk_label_new("inf. time:   ");
		gtk_widget_set_halign(title_inf_time,GTK_ALIGN_START);
		title_inf_accuracy = gtk_label_new("accuracy:    ");
		gtk_widget_set_halign(title_inf_accuracy,GTK_ALIGN_START);

		data->info_disp_fps = gtk_label_new(NULL);
		gtk_label_set_lines (GTK_LABEL(data->info_disp_fps), 2);
		gtk_label_set_line_wrap (GTK_LABEL(data->info_disp_fps), TRUE);
		gtk_widget_set_halign(data->info_disp_fps,GTK_ALIGN_END);

		data->info_inf_fps = gtk_label_new(NULL);
		gtk_label_set_lines (GTK_LABEL(data->info_inf_fps), 2);
		gtk_label_set_line_wrap (GTK_LABEL(data->info_inf_fps), TRUE);
		gtk_widget_set_halign(data->info_inf_fps,GTK_ALIGN_END);

		data->info_inf_time = gtk_label_new(NULL);
		gtk_label_set_lines (GTK_LABEL(data->info_inf_time), 2);
		gtk_label_set_line_wrap (GTK_LABEL(data->info_inf_time), TRUE);
		gtk_widget_set_halign(data->info_inf_time,GTK_ALIGN_END);

		data->info_accuracy = gtk_label_new(NULL);
		gtk_label_set_lines (GTK_LABEL(data->info_accuracy), 2);
		gtk_label_set_line_wrap (GTK_LABEL(data->info_accuracy), TRUE);
		gtk_widget_set_halign(data->info_accuracy,GTK_ALIGN_END);

	} else {
		/*  Still picture use case */
		/* Create the video widget */
		still_pict_draw = gtk_drawing_area_new();
		gtk_widget_set_app_paintable(still_pict_draw, TRUE);
		g_signal_connect(still_pict_draw, "draw",
				 G_CALLBACK(gui_draw_overlay_cb), data);

		title_inf_time = gtk_label_new("inf. time:   ");
		gtk_widget_set_halign(title_inf_time,GTK_ALIGN_START);
		title_inf_accuracy = gtk_label_new("accuracy:    ");
		gtk_widget_set_halign(title_inf_accuracy,GTK_ALIGN_START);

		data->info_inf_time = gtk_label_new(NULL);
		gtk_label_set_lines (GTK_LABEL(data->info_inf_time), 2);
		gtk_label_set_line_wrap (GTK_LABEL(data->info_inf_time), TRUE);
		gtk_widget_set_halign(data->info_inf_time,GTK_ALIGN_END);

		data->info_accuracy = gtk_label_new(NULL);
		gtk_label_set_lines (GTK_LABEL(data->info_accuracy), 2);
		gtk_label_set_line_wrap (GTK_LABEL(data->info_accuracy), TRUE);
		gtk_widget_set_halign(data->info_accuracy,GTK_ALIGN_END);
	}

	/* Set the boxes */
	data->info_box_ov = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_name(data->info_box_ov , "gui_overlay_stbox");
	if (data->preview_enabled){
		/* Camera preview use case */
		gtk_box_pack_start(GTK_BOX(data->info_box_ov ), st_icon_event_box, FALSE, FALSE, 3);
		gtk_box_pack_start(GTK_BOX(data->info_box_ov ), title_disp_fps, FALSE, FALSE, 3);
		gtk_box_pack_start(GTK_BOX(data->info_box_ov ), data->info_disp_fps, FALSE, FALSE, 3);
		gtk_box_pack_start(GTK_BOX(data->info_box_ov ), title_inf_fps, FALSE, FALSE, 3);
		gtk_box_pack_start(GTK_BOX(data->info_box_ov ), data->info_inf_fps, FALSE, FALSE, 3);
		gtk_box_pack_start(GTK_BOX(data->info_box_ov ), title_inf_time, FALSE, FALSE, 3);
		gtk_box_pack_start(GTK_BOX(data->info_box_ov ), data->info_inf_time, FALSE, FALSE, 3);
		gtk_box_pack_start(GTK_BOX(data->info_box_ov ), title_inf_accuracy, FALSE, FALSE, 3);
		gtk_box_pack_start(GTK_BOX(data->info_box_ov ), data->info_accuracy, FALSE, FALSE, 3);
	} else {
		/*  Still picture use case */
		gtk_box_pack_start(GTK_BOX(data->info_box_ov ), st_icon_event_box, FALSE, FALSE, 4);
		gtk_box_pack_start(GTK_BOX(data->info_box_ov ), title_inf_time, FALSE, FALSE, 4);
		gtk_box_pack_start(GTK_BOX(data->info_box_ov ), data->info_inf_time, FALSE, FALSE, 4);
		gtk_box_pack_start(GTK_BOX(data->info_box_ov ), title_inf_accuracy, FALSE, FALSE, 4);
		gtk_box_pack_start(GTK_BOX(data->info_box_ov ), data->info_accuracy, FALSE, FALSE, 4);
	}

	drawing_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_name(drawing_box, "gui_overlay_draw");
	if (data->preview_enabled){
		/* Camera preview use case */
		gtk_box_pack_start(GTK_BOX(drawing_box), drawing_area, TRUE, TRUE, 0);
	} else {
		/*  Still picture use case */
		gtk_box_pack_start(GTK_BOX(drawing_box), still_pict_draw, TRUE, TRUE, 0);
	}

	data->exit_box_ov = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_name(data->exit_box_ov , "gui_overlay_exit");
	gtk_box_pack_start(GTK_BOX(data->exit_box_ov ), exit_icon_event_box, FALSE, FALSE, 2);

	main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_name(main_box, "gui_overlay");
	gtk_box_pack_start(GTK_BOX(main_box), data->info_box_ov, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(main_box), drawing_box, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(main_box), data->exit_box_ov , FALSE, FALSE, 2);

	/* Push the UI into the window_overlay */
	gtk_container_add(GTK_CONTAINER(data->window_overlay), main_box);
}

bool first_call_main = true;
static gboolean gui_draw_main_cb(GtkWidget *widget,
				 cairo_t *cr,
				 CustomData *data)
{
	int offset = 0;
	/* Get display information and set UI variable accordingly */
	if (first_call_main) {
		GdkWindow *window = gtk_widget_get_window(data->window_main);
		data->window_width = gdk_window_get_width(window);
		data->window_height = gdk_window_get_height(window) + data->ui_weston_panel_thickness;
		g_print("GTK window: width %d, height %d\n",
			data->window_width,
			data->window_height);
		gui_set_ui_parameters(data);
		/*  after the initialization of the main UI, initialize the
		 *  overlay UI */
		gtk_widget_show_all(data->window_overlay);
	}
	if (data->preview_enabled) {
		/* Camera preview use case */
		data->widget_draw_width = gtk_widget_get_allocated_width(widget);
		data->widget_draw_height= gtk_widget_get_allocated_height(widget);

		/* if first call skip the first frame */
		if (first_call_main){
			/*  Set pipeline in playing state when the ui is ready
			 *  to handle the video */
			int ret = gst_element_set_state(data->pipeline, GST_STATE_PLAYING);
			if (ret == GST_STATE_CHANGE_FAILURE) {
				g_printerr("Unable to set the pipeline to the playing state.\n");
				gtk_main_quit();
				return FALSE;
			}
			g_print("Gst pipeline now playing \n");
			first_call_main = false;
			return FALSE;
		}
	} else {
		/* Still picture use case */
		data->widget_draw_width = gtk_widget_get_allocated_width(widget);
		data->widget_draw_height = gtk_widget_get_allocated_height(widget);
		offset = (data->widget_draw_width - data->frame_disp_pos.width)/2;

		/*  Don't display the first picture loaded to avoid UI issues */
		if (first_call_main){
			first_call_main = false;
			return FALSE;
		}

		if(data->img_to_display.data){
			/*  Display the resized picture */
			/* Generate the cairo surface */
			int stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24,
								   data->frame_disp_pos.width);

			cairo_surface_t *picture = cairo_image_surface_create_for_data(data->img_to_display.data,
									    CAIRO_FORMAT_RGB24,
									    data->frame_disp_pos.width,
									    data->frame_disp_pos.height,
									    stride);
			cairo_set_source_surface(cr,picture,offset,0);
			cairo_paint(cr);
			cairo_surface_destroy(picture);
		} else {
			/* Set the black background */
			cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);
			cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
			cairo_paint(cr);
		}
	}
	return FALSE;
}

/**
 * This function is creating GTK widget to create the main window use to render
 * the preview.
 * Title label are also used in the main window in order to create boxes with
 * sizes that match the label width.
 */
static void gui_create_main(CustomData *data)
{
	/* HBox to hold all other boxes */
	GtkWidget *main_box;
	/* VBox to hold the video */
	GtkWidget *video_box;
	/* Event box for ST icon */
	GtkWidget *st_icon_event_box;
	/* Event box for exit icon */
	GtkWidget *exit_icon_event_box;
	/* Widget to handle picture in Still picture use case */
	GtkWidget *still_pict_draw;
	/* Labels title for information */
	GtkWidget *title_disp_fps;
	GtkWidget *title_inf_fps;
	GtkWidget *title_inf_time;
	GtkWidget *title_inf_accuracy;

	/* Create the window */
	data->window_main = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(G_OBJECT(data->window_main), "delete-event",
			 G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(data->window_main), "destroy",
			 G_CALLBACK(gtk_main_quit), NULL);

	/* Remove title bar */
	gtk_window_set_decorated(GTK_WINDOW(data->window_main), FALSE);

	/* Maximize the window size */
	gtk_window_maximize(GTK_WINDOW(data->window_main));

	/* Create the ST icon widget */
	data->st_icon_main = gtk_image_new();
	st_icon_event_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(st_icon_event_box), data->st_icon_main);
	g_signal_connect(G_OBJECT(st_icon_event_box),"button_press_event",
			 G_CALLBACK(st_icon_cb), data);

	/* Create the exit icon widget */
	data->exit_icon_main = gtk_image_new();
	exit_icon_event_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(exit_icon_event_box), data->exit_icon_main);
	g_signal_connect(G_OBJECT(exit_icon_event_box),"button_press_event",
			 G_CALLBACK(exit_icon_cb), data);

	if (data->preview_enabled) {
		/* Camera preview use case */
		/* Create the video widget */
		data->video = gtk_drawing_area_new();
		gtk_widget_set_app_paintable(data->video, TRUE);
		g_signal_connect(data->video, "draw",
				 G_CALLBACK(gui_draw_main_cb), data);

		/* Create the gtk labels to display nn inference information*/
		title_disp_fps = gtk_label_new("disp. fps:   ");
		gtk_widget_set_halign(title_disp_fps,GTK_ALIGN_START);
		title_inf_fps = gtk_label_new("inf. fps:    ");
		gtk_widget_set_halign(title_inf_fps,GTK_ALIGN_START);
		title_inf_time = gtk_label_new("inf. time:   ");
		gtk_widget_set_halign(title_inf_time,GTK_ALIGN_START);
		title_inf_accuracy = gtk_label_new("accuracy:    ");
		gtk_widget_set_halign(title_inf_accuracy,GTK_ALIGN_START);

		data->info_disp_fps = gtk_label_new(NULL);
		gtk_label_set_lines (GTK_LABEL(data->info_disp_fps), 2);
		gtk_label_set_line_wrap (GTK_LABEL(data->info_disp_fps), TRUE);
		gtk_widget_set_halign(data->info_disp_fps,GTK_ALIGN_END);

		data->info_inf_fps = gtk_label_new(NULL);
		gtk_label_set_lines (GTK_LABEL(data->info_inf_fps), 2);
		gtk_label_set_line_wrap (GTK_LABEL(data->info_inf_fps), TRUE);
		gtk_widget_set_halign(data->info_inf_fps,GTK_ALIGN_END);

		data->info_inf_time = gtk_label_new(NULL);
		gtk_label_set_lines (GTK_LABEL(data->info_inf_time), 2);
		gtk_label_set_line_wrap (GTK_LABEL(data->info_inf_time), TRUE);
		gtk_widget_set_halign(data->info_inf_time,GTK_ALIGN_END);

		data->info_accuracy = gtk_label_new(NULL);
		gtk_label_set_lines (GTK_LABEL(data->info_accuracy), 2);
		gtk_label_set_line_wrap (GTK_LABEL(data->info_accuracy), TRUE);
		gtk_widget_set_halign(data->info_accuracy,GTK_ALIGN_END);

	} else {
		/*  Still picture preview use case */
		/* Create the drawing widget */
		still_pict_draw = gtk_drawing_area_new();
		gtk_widget_set_app_paintable(still_pict_draw, TRUE);
		g_signal_connect(still_pict_draw, "draw",
				 G_CALLBACK(gui_draw_main_cb), data);

		title_inf_time = gtk_label_new("inf. time:   ");
		gtk_widget_set_halign(title_inf_time,GTK_ALIGN_START);
		title_inf_accuracy = gtk_label_new("accuracy:    ");
		gtk_widget_set_halign(title_inf_accuracy,GTK_ALIGN_START);

		data->info_inf_time = gtk_label_new(NULL);
		gtk_label_set_lines (GTK_LABEL(data->info_inf_time), 2);
		gtk_label_set_line_wrap (GTK_LABEL(data->info_inf_time), TRUE);
		gtk_widget_set_halign(data->info_inf_time,GTK_ALIGN_END);

		data->info_accuracy = gtk_label_new(NULL);
		gtk_label_set_lines (GTK_LABEL(data->info_accuracy), 2);
		gtk_label_set_line_wrap (GTK_LABEL(data->info_accuracy), TRUE);
		gtk_widget_set_halign(data->info_accuracy,GTK_ALIGN_END);
	}

	/* Set the boxes */
	data->info_box_main = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_name(data->info_box_main, "gui_main_stbox");
	if (data->preview_enabled){
		/*  Camera preview use case */
		gtk_box_pack_start(GTK_BOX(data->info_box_main), st_icon_event_box, FALSE, FALSE, 3);
		gtk_box_pack_start(GTK_BOX(data->info_box_main), title_disp_fps, FALSE, FALSE, 3);
		gtk_box_pack_start(GTK_BOX(data->info_box_main), data->info_disp_fps, FALSE, FALSE, 3);
		gtk_box_pack_start(GTK_BOX(data->info_box_main), title_inf_fps, FALSE, FALSE, 3);
		gtk_box_pack_start(GTK_BOX(data->info_box_main), data->info_inf_fps, FALSE, FALSE, 3);
		gtk_box_pack_start(GTK_BOX(data->info_box_main), title_inf_time, FALSE, FALSE, 3);
		gtk_box_pack_start(GTK_BOX(data->info_box_main), data->info_inf_time, FALSE, FALSE, 3);
		gtk_box_pack_start(GTK_BOX(data->info_box_main), title_inf_accuracy, FALSE, FALSE, 3);
		gtk_box_pack_start(GTK_BOX(data->info_box_main), data->info_accuracy, FALSE, FALSE, 3);
	} else {
		/*  Still picture use case */
		gtk_box_pack_start(GTK_BOX(data->info_box_main), st_icon_event_box, FALSE, FALSE, 4);
		gtk_box_pack_start(GTK_BOX(data->info_box_main), title_inf_time, FALSE, FALSE, 4);
		gtk_box_pack_start(GTK_BOX(data->info_box_main), data->info_inf_time, FALSE, FALSE, 4);
		gtk_box_pack_start(GTK_BOX(data->info_box_main), title_inf_accuracy, FALSE, FALSE, 4);
		gtk_box_pack_start(GTK_BOX(data->info_box_main), data->info_accuracy, FALSE, FALSE, 4);
	}

	video_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_name(video_box, "gui_main_video");
	if (data->preview_enabled){
		/*  Camera preview use case  */
		gtk_box_pack_start(GTK_BOX(video_box), data->video, TRUE, TRUE, 0);
	} else {
		/*  Still picture use case */
		gtk_box_pack_start(GTK_BOX(video_box), still_pict_draw, TRUE, TRUE, 0);
	}

	data->exit_box_main = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_name(data->exit_box_main, "gui_main_exit");
	gtk_box_pack_start(GTK_BOX(data->exit_box_main), exit_icon_event_box, FALSE, FALSE, 2);

	main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_name(main_box, "gui_main");
	gtk_box_pack_start(GTK_BOX(main_box), data->info_box_main, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(main_box), video_box, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(main_box), data->exit_box_main, FALSE, FALSE, 2);

	gtk_container_add(GTK_CONTAINER(data->window_main), main_box);
	gtk_widget_show_all(data->window_main);
}

/**
 * This function is called when appsink Gstreamer element receives a buffer
 */
static GstFlowReturn gst_new_sample_cb(GstElement *sink, CustomData *data)
{
	GstSample *sample;
	GstBuffer *app_buffer, *buffer;
	GstMapInfo info;

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
 * of display
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
 * of nn inference
 */
void gst_fps_measure_nn_cb(GstElement *fpsdisplaysink,
			   gdouble fps,
			   gdouble droprate,
			   gdouble avgfps,
			   gpointer data)
{
	app_avg_fps = avgfps;
}

/**
 * This function is called when a Gstreamer error message is posted on the bus.
 */
static void gst_error_cb (GstBus *bus, GstMessage *msg, CustomData *data)
{
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
static void gst_eos_cb (GstBus *bus, GstMessage *msg, CustomData *data)
{
	g_print("End-Of-Stream reached.\n");
	gst_element_set_state(data->pipeline, GST_STATE_READY);
}

/**
 * This function is called when a Gstreamer "application" message is posted on
 * the bus.
 * Here we retrieve the message posted by the appsink gst_new_sample_cb callback.
 */
static void gst_application_cb (GstBus *bus, GstMessage *msg, CustomData *data)
{
	if (g_strcmp0(gst_structure_get_name(gst_message_get_structure(msg)), "inference-done") == 0) {
		/* If the message is the "inference-done", update the GTK UI */
		gtk_widget_queue_draw(data->window_overlay);
	}
}

/**
 * This function is called every time a new message is posted on the bus. This
 * function is also used to assign the video to a video overlay which can be
 * display in a gtk UI
 */
static GstBusSyncReply gst_bus_sync_handler(GstBus * bus, GstMessage * message, gpointer user_data)
{
	CustomData *data = (CustomData *)user_data;

	if (gst_is_wayland_display_handle_need_context_message(message)) {
		GstContext *context;
		GdkDisplay *display;
		struct wl_display *display_handle;

		display = gtk_widget_get_display(data->video);
		display_handle = gdk_wayland_display_get_wl_display(display);
		context = gst_wayland_display_handle_context_new(display_handle);
		gst_element_set_context(GST_ELEMENT(GST_MESSAGE_SRC(message)), context);
		gst_context_unref(context);

		goto drop;
	} else if (gst_is_video_overlay_prepare_window_handle_message(message)) {
		GdkWindow *window;
		struct wl_surface *window_handle;

		/* GST_MESSAGE_SRC (message) will be the overlay object that we have to
		 * use. This may be waylandsink, but it may also be playbin. In the latter
		 * case, we must make sure to use playbin instead of waylandsink, because
		 * playbin resets the window handle and render_rectangle after restarting
		 * playback and the actual window size is lost */
		data->video_overlay = GST_VIDEO_OVERLAY(GST_MESSAGE_SRC(message));

		gtk_widget_get_allocation(data->video, &data->video_allocation);
		window = gtk_widget_get_window(data->video);
		window_handle = gdk_wayland_window_get_wl_surface(window);

		g_print("Setting video window handle, position (%d - %d) and size (%d x %d)\n",
			data->video_allocation.x, data->video_allocation.y,
			data->video_allocation.width, data->video_allocation.height);

		data->x_scale = (float)data->video_allocation.width / (float)data->frame_width;
		data->y_scale = (float)data->video_allocation.height / (float)data->frame_height;

		g_print("Scaling factor x %.3f y %.3f\n", data->x_scale, data->y_scale);

		gst_video_overlay_set_window_handle(data->video_overlay, (guintptr) window_handle);
		gst_video_overlay_set_render_rectangle(data->video_overlay,
						       data->video_allocation.x, data->video_allocation.y,
						       data->video_allocation.width, data->video_allocation.height);
		gst_video_overlay_expose(data->video_overlay);

		goto drop;
	}

	return GST_BUS_PASS;

drop:
	gst_message_unref(message);
	return GST_BUS_DROP;
}

/**
 * Construct the Gstreamer pipeline used to stream camera frame and to run
 * TensorFlow Lite inference engine using appsink.
 */
static int gst_pipeline_camera_creation(CustomData *data)
{
	GstStateChangeReturn ret;
	GstElement *pipeline, *source, *dispsink, *tee, *scale, *framerate;
	GstElement *queue1, *queue2, *convert, *convert2, *appsink, *framecrop;
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
	convert2    = gst_element_factory_make("videoconvert",   "video-convert-wayland");
	framecrop   = gst_element_factory_make("videocrop",      "videocrop");
	scale       = gst_element_factory_make("videoscale",     "videoscale");
	dispsink    = gst_element_factory_make("waylandsink",    "display-sink");
	appsink     = gst_element_factory_make("appsink",        "app-sink");
	framerate   = gst_element_factory_make("videorate",      "video-rate");
	fpsmeasure1 = gst_element_factory_make("fpsdisplaysink", "fps-measure1");
	fpsmeasure2 = gst_element_factory_make("fpsdisplaysink", "fps-measure2");

	if (!pipeline || !source || !tee || !queue1 || !queue2 || !convert || !convert2 ||
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
			 source, framerate, tee, convert, convert2, queue1, queue2, scale,
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
	if (!gst_element_link_many(tee, queue1, convert2, fpsmeasure1, NULL)) {
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
	gst_bus_set_sync_handler(bus, gst_bus_sync_handler, data, NULL);
	gst_object_unref(bus);

	/* Set the pipeline to ready state */
	ret = gst_element_set_state(pipeline, GST_STATE_READY);
	if (ret == GST_STATE_CHANGE_FAILURE) {
		g_printerr("Unable to set the pipeline to the ready state.\n");
		gst_object_unref(GST_OBJECT(data->pipeline));
		return -1;
	}
	g_print("Gst pipeline now ready \n");

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
 * Function called when CTRL + C or Kill signal is detected
 */
static void int_handler(int dummy)
{
	if (gtk_main_started){
		exit_application = true;
	} else {
		exit(0);
	}
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

	/* Process the application parameters */
	process_args(argc, argv);

	/* Initialize our data structure */
	data.pipeline = NULL;
	data.window_main = NULL;
	data.window_overlay = NULL;
	data.new_inference = false;
	data.frame_width  = std::stoi(camera_width_str);
	data.frame_height = std::stoi(camera_height_str);
	std::cout << data.frame_width << " data.frame_width\n";
	std::cout << data.frame_height << " data.frame_height\n";
	data.ui_cairo_font_size = 25;
	data.ui_cairo_font_size_label = 40;
	data.ui_box_line_width = 2.0;
	data.ui_weston_panel_thickness = 32;

	/*  Check if the Edge TPU is connected */
	int status = system("lsusb -d 1a6e:");
	status &= system("lsusb -d 18d1:");
	if (status) {
		g_printerr("ERROR: Edge TPU not connected.\n");
		return 0;
	}

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
		} else {
			/* Check the camera type to configure it if necessary */
			std::stringstream cmd;
			cmd << "cat /sys/class/video4linux/video" << video_device_str << "/name";
			std::string camera_type = shell_cmd_exec(cmd.str().c_str());
			std::cout << "camera type : " << camera_type << std::endl;
			std::string dcmipp = "dcmipp_dump_capture";
			std::size_t found = camera_type.find(dcmipp);
			if (found!=std::string::npos) {
				/* dcmipp camera found */
				setup_dcmipp();
			}
		}
	} else {
		data.preview_enabled = false;

		/* Check if directory is empty */
		std::string file = get_files_in_directory_randomly(image_dir_str);
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
	config.number_of_results = 5;

	tfl_wrapper_tpu.Initialize(&config);

	if (tfl_wrapper_tpu.ReadLabelsFile(config.labels_file_name, &labels, &label_count) != kTfLiteOk)
		exit(1);

	/* Get model input size */
	data.nn_input_width  = tfl_wrapper_tpu.GetInputWidth();
	data.nn_input_height = tfl_wrapper_tpu.GetInputWidth();

	if (!tfl_wrapper_tpu.IsModelQuantized()) {
		g_print("The model is not quantized! Please quantized it for egde tpu usage...\n");
		exit(0);
	}

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
	gui_gtk_style(&data);
	if (data.preview_enabled) {
		/*  Camera preview use case */
		/* Initialize GStreamer for camera preview use case*/
		gst_init(&argc, &argv);
		/* Create the GUI containing the video stream  */
		g_print("Start Creating main GTK window\n");
		gui_create_main(&data);
		/* Create the GStreamer pipeline for camera use case */
		ret = gst_pipeline_camera_creation(&data);
		if(ret)
			exit(1);
		/* Create the second GUI containing the nn information  */
		g_print("Start Creating overlay GTK window\n");
		gui_create_overlay(&data);
	} else {
		/*  Still picture use case */
		/* Create the GUI containing the random picture to infer  */
		g_print("Start Creating main GTK window\n");
		gui_create_main(&data);
		/* Create the second GUI containing the nn information  */
		g_print("Start Creating overlay GTK window\n");
		gui_create_overlay(&data);
	}

	/* Start a timeout timer in validation process to close application if
	 * timeout occurs */
	if (validation) {
		data.valid_draw_count = 0;
		data.valid_timeout_id = g_timeout_add(10000,
						      valid_timeout_callback,
						      NULL);
	}

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

	g_print(" Application exited properly \n");
	return 0;
}

