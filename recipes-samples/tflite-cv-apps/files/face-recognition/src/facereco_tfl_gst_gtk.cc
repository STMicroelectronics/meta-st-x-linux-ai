/*
 * facereco_tfl_gst_gtk.cc
 *
 * This application demonstrate a computer vision use case for face recognition
 * where frames are grabbed from a camera input (/dev/videox) and
 * analyzed by neural network models interpreted by TensorFlow Lite framework.
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
 * This software component is licensed by ST under Ultimate Liberty license
 * SLA0044, the "License"; You may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 *
 *     http://www.st.com/SLA0044
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

#include "libfacedetect_tfl.h"
#include "libfacereco_tfl.h"

/* Application parameters */
std::vector<std::string> dir_files;
std::string image_dir_str;
std::string video_device_str  = "0";
std::string camera_fps_str    = "15";
std::string camera_width_str  = "640";
std::string camera_height_str = "480";
int reco_simultaneous_face = 1;
float reco_threshold = 0.70;
int max_db_faces = 200;

/* TensorFlow Lite variables for face detect neural network*/
Tfl_facedetect libfacedetect_tfl;
float fd_inference_time;

/* TensorFlow Lite variables for face reco neural network*/
Tfl_facereco libfacereco_tfl;
float fr_inference_time;

/* Synchronization variables */
std::condition_variable cond_var;
std::mutex cond_var_m;
std::mutex mtx;

bool gtk_main_started = false;

#define DATABASE_DIRECTORY    "/usr/local/demo-ai/computer-vision/tflite-face-recognition/database/"
#define CSS_DIRECTORY         "/usr/local/demo-ai/computer-vision/tflite-face-recognition/bin/resources/"

#define MAX_HISTORY_THUMBNAILS  11 /* for 720p display */
#define FACE_THUMBNAIL_SIZE     100
#define FACE_THUMBNAIL_SPACING  10
#define IDENTITY_CLASSES        128

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

typedef struct _LiveFaceDetect {
	Position pos;
	bool registered;
} LiveFaceDetect;

typedef struct _RegisteredFace {
	std::string file_path;
	std::string label;
	cv::Mat face_bgra_thumb;
	cairo_surface_t *cairo_s_face;
	float identity[IDENTITY_CLASSES];
} RegisteredFace;

typedef struct _DetectedFace {
	cv::Mat face_rgb;
	Bbox bbox;
	std::string label;
	float identity[IDENTITY_CLASSES];
	float similarity;
} DetectedFace;

typedef struct _Key{
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
	/* Preview camera enable (else still picture use case) */
	bool preview_enabled;
	/* face reco NN input size */
	int fr_nn_input_width;
	int fr_nn_input_height;
	/* window resolution */
	int window_width;
	int window_height;
	/* Frame resolution (still picture or camera) */
	int frame_width;
	int frame_height;
	/* OpenCv camera frame representation */
	cv::Mat img_rgb;
	/* Frame position in fullscreen mode */
	Position frame_fullscreen_pos;
	int frame_fullscreen_pos_x;
	int frame_fullscreen_pos_y;
	/* Managing virtual keyboard */
	GtkWidget *keyboard;
	GtkWidget *keyboard_entry;
	GtkWidget *keyboard_label;
	GtkWidget *keyboard_fixed;
	std::vector<Key> keys;
	/* Managing faces */
	int nb_history_thumbnails;
	int max_db_faces;
	Position face_banner;
	std::vector<LiveFaceDetect> screen_face_position;
	std::vector<Position> history_thumb_position[MAX_HISTORY_THUMBNAILS];
	std::vector<RegisteredFace> registered_faces;
	std::vector<DetectedFace> detected_faces;
	/* Face reco parameters */
	int max_simultaneous_face;
	float face_reco_threshold;
	/* Still picture */
	cairo_surface_t *picture;
	cv::Mat pict_bgra_resized;
	std::string pict_file;
} CustomData;

/* Framerate statisitics */
gdouble display_avg_fps = 0;
gdouble nn_avg_fps = 0;

/**
 * This function execute the face detect NN inference
 */
static float nn_fd_inference(const cv::Mat& img,
			     CustomData* data)
{
	uint8_t nb_faces = 0;

	float ret = libfacedetect_tfl.FindFaces(img, &nb_faces);

	mtx.lock();
	/* Reset the detected faces */
	data->detected_faces.clear();

	for (uint32_t i = 0 ; i < nb_faces ; i ++) {
		DetectedFace new_face;
		Bbox bbox = libfacedetect_tfl.GetDetectedFaceBbox(i);
		new_face.label = "unknown";
		new_face.bbox = bbox;

		data->detected_faces.push_back(new_face);
	}
	mtx.unlock();
	return ret;
}

/**
 * This function execute the face reco NN inference
 */
static float nn_fr_inference(uint8_t *img,
			     float *identity,
			     CustomData *data)
{
	mtx.lock();
	float ret = libfacereco_tfl.RunInference(img);

	/* Get face detect results */
	float *id = libfacereco_tfl.GetOutputTensor(0);
	for (unsigned int i = 0 ; i < libfacereco_tfl.GetOutputSize(0) ; i++)
		identity[i] = id[i];

	mtx.unlock();
	return ret;
}

static std::pair<std::string, float> nn_fr_prediction(float *identity,
						      CustomData *data)
{
	float sqrSumIn = 0;
	float similarity = 0.0f;
	std::string label = "unknown";
	int index = 0;

	/* No faces are registered so just return unlonw label */
	if (data->registered_faces.empty())
		return {label, similarity};

	for (int i = 0 ; i < IDENTITY_CLASSES ; i++)
		sqrSumIn += identity[i] * identity[i];

	for (unsigned int i = 0 ; i < data->registered_faces.size() ; i++) {
		float sqrSum = 0, sqrSumRef = 0;
		for (int j = 0; j < IDENTITY_CLASSES; j++) {
			sqrSum += identity[j] * data->registered_faces[i].identity[j];
			sqrSumRef += data->registered_faces[i].identity[j] *
				data->registered_faces[i].identity[j];
		}
		float cosineSimilarity = (sqrSum / sqrt(sqrSumIn) /
				    sqrt(sqrSumRef) + 1) / 2.0;
		if (similarity <= cosineSimilarity) {
			similarity = cosineSimilarity;
			label = data->registered_faces[i].label;
			index = (int)i;
		}
	}
	if (similarity < data->face_reco_threshold)
		label = "unknown";

	/* Change the file modification timestamps so that the
	 * history database is kept when the application is restarted */
	utimes(data->registered_faces[index].file_path.c_str(), NULL);

	/* If the recognized face is not in history (i.e in the last
	 * nb_history_thumbnails) the recognized face is moved to the very end
	 * position of the registered faces list so that it will be displayed
	 * first in the thumbnail history */
	if (index <  ((int)data->registered_faces.size() - data->nb_history_thumbnails)) {
		RegisteredFace face = data->registered_faces[index];
		data->registered_faces.erase(data->registered_faces.begin() + index);
		data->registered_faces.push_back(face);
	}

	return {label, similarity};
}

/**
 * This function register a new face in the database by reading a png picture
 * file
 */
static void register_new_face_from_file(std::string dir,
					std::string file_path,
					CustomData *data)
{
	RegisteredFace new_face;
	cv::Mat face_bgr, face_bgra, img_nn;

	new_face.file_path = file_path;
	new_face.label = file_path;
	new_face.label.erase(0, dir.length());
	new_face.label.erase(new_face.label.find("."));

	face_bgr = cv::imread(new_face.file_path);
	cv::cvtColor(face_bgr, face_bgra, cv::COLOR_BGR2BGRA);

	cv::Size size(FACE_THUMBNAIL_SIZE, FACE_THUMBNAIL_SIZE);
	cv::resize(face_bgra, new_face.face_bgra_thumb, size);

	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24,
						   FACE_THUMBNAIL_SIZE);
	new_face.cairo_s_face =
		cairo_image_surface_create_for_data(new_face.face_bgra_thumb.data,
						    CAIRO_FORMAT_RGB24,
						    FACE_THUMBNAIL_SIZE,
						    FACE_THUMBNAIL_SIZE,
						    stride);

	cv::Size size_nn(data->fr_nn_input_width,
			 data->fr_nn_input_height);
	cv::resize(face_bgr, img_nn, size_nn);
	cv::cvtColor(img_nn, img_nn, cv::COLOR_BGR2RGB);

	nn_fr_inference(img_nn.data, new_face.identity, data);

	data->registered_faces.push_back(new_face);
}


/**
 * Sort file from the oldest to the newest based on the modification time
 */
static bool compare_modif_date(const std::string& a,
			       const std::string& b)
{
	/* oldest comes first */
	struct stat fileInfo_a;
	struct stat fileInfo_b;

	stat(a.c_str(), &fileInfo_a);
	stat(b.c_str(), &fileInfo_b);

	return fileInfo_a.st_mtime < fileInfo_b.st_mtime;
}

/**
 * This function is call once to initialize the face database
 */
static void initialize_face_database(CustomData *data)
{
	std::vector<std::string> files;
	/* Get the list of all the files */
	DIR* dirp = opendir(DATABASE_DIRECTORY);

	/* Create the directory if it does not exist */
	if (dirp == NULL) {
		mkdir(DATABASE_DIRECTORY,
		      S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
		dirp = opendir(DATABASE_DIRECTORY);
	}

	struct dirent * dp;
	while ((dp = readdir(dirp)) != NULL) {
		if ((strcmp(dp->d_name, ".") !=0) &&
		    (strcmp(dp->d_name, "..") != 0)) {
			std::stringstream file_path_sstr;
			file_path_sstr << DATABASE_DIRECTORY << dp->d_name;
			files.push_back(file_path_sstr.str());
		}
	}
	closedir(dirp);

	/* sort file by modification date */
	std::sort(files.begin(), files.end(), compare_modif_date);

	/* If no face in the data base return */
	if (files.size() == 0)
		return;

	for (unsigned int i = 0 ; i < files.size() ; i++) {
		register_new_face_from_file(DATABASE_DIRECTORY,
					    files[i], data);
	}
}

/**
 * This function load the widget css style according to the display size
 */
static void gui_load_css(CustomData *data)
{
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
}

/**
 * This function is called when a click on the live bounding box is recieved
 */
static void gui_register_new_face(int index,
				  CustomData *data)
{
	mtx.lock();
	if (data->detected_faces.empty()) {
		/* The click event has been registered when the bouding box just
		 * disapeared => do not register any face.*/
		goto end;
	}

	if ((int)data->registered_faces.size() < data->max_db_faces) {
		/* display the virtual keyboard to enter the face label */
		gtk_label_set_markup(GTK_LABEL(data->keyboard_label),
				     "<span font='15' color='#D3007AFF'>"
				     "<b>Please enter name...</b></span>");
		gtk_widget_show_all(data->keyboard);

		cv::Mat face_bgra;
		cv::cvtColor(data->detected_faces[index].face_rgb,
			     face_bgra,
			     cv::COLOR_RGB2BGRA);

		std::stringstream tmp_sstr;
		tmp_sstr << DATABASE_DIRECTORY << "tmp.png";
		cv::imwrite(tmp_sstr.str().c_str(), face_bgra);
	} else {
		LOG(INFO) << "You reach the maximum number of faces to be registered!\n";
	}

end:
	mtx.unlock();
}

/**
 * This function is called when a click on "return" key of the virtual
 * keyboard is registered.
 */
static bool gui_finalize_face_registering(std::string label,
					  CustomData *data)
{
	std::stringstream file_name_sstr;
	file_name_sstr << DATABASE_DIRECTORY << label << ".png";

	/* Check if the label already exists */
	if (access(file_name_sstr.str().c_str(), 0) == 0) {
		gtk_label_set_markup(GTK_LABEL(data->keyboard_label),
				     "<span font='15' color='#D3007AFF'>"
				     "<b>Name already exists! please enter a "
				     "new name...</b></span>");
		return false;
	}

	/* clear the widget entry and hide the keyboard */
	gtk_entry_set_text(GTK_ENTRY(data->keyboard_entry), "");
	gtk_widget_hide(data->keyboard);

	/* rename the temporary file into the final name with the label
	 * information label.png */
	std::stringstream tmp_sstr;
	tmp_sstr << DATABASE_DIRECTORY << "tmp.png";
	rename(tmp_sstr.str().c_str(), file_name_sstr.str().c_str());

	register_new_face_from_file(DATABASE_DIRECTORY,
				    file_name_sstr.str(), data);

	return true;
}

/**
 * This function is called when a click on the thumbnail face is recieved
 */
static void gui_delete_registered_face(int i,
				       CustomData *data)
{
	unsigned int index = data->registered_faces.size() - i - 1;
	/* delete the registered face */
	cairo_surface_destroy(data->registered_faces[index].cairo_s_face);
	remove(const_cast<char*>(data->registered_faces[index].file_path.c_str()));
	data->registered_faces.erase(data->registered_faces.begin() + index);
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

	if (data->preview_enabled) {
		window_height += WESTON_PANEL_THICKNESS;
	} else {
		window_height -= BRAIN_AREA_HEIGHT + FACE_THUMBNAIL_SIZE +
			FACE_THUMBNAIL_SPACING;
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
 * This function is an helper to get thumbnail position and size depending on
 * the preview area.
 */
static void gui_compute_history_thumbnail_position(GdkWindow *window,
						CustomData *data)
{
	data->face_banner.width = data->window_width;
	data->face_banner.height = FACE_THUMBNAIL_SIZE + FACE_THUMBNAIL_SPACING;
	data->face_banner.x = 0;
	data->face_banner.y = data->window_height - data->face_banner.height;

	data->nb_history_thumbnails = (data->window_width - FACE_THUMBNAIL_SPACING)
		/ (FACE_THUMBNAIL_SIZE + FACE_THUMBNAIL_SPACING);

	/* Clip to the MAX_HISTORY_THUMBNAILS value if needed */
	data->nb_history_thumbnails = std::min(data->nb_history_thumbnails,
					       MAX_HISTORY_THUMBNAILS);

	/* Depending on the number of face registered, the placement of the
	 * history thumbnail at the bottom of the preview differs to have them
	 * always centered. The following part of code precalculate the position
	 * of history the thumbnails. */
	for (int i = 0 ; i < data->nb_history_thumbnails ; i++) {
		int starting_offset = (data->window_width -
			((FACE_THUMBNAIL_SIZE + FACE_THUMBNAIL_SPACING) * (i + 1)) +
			FACE_THUMBNAIL_SPACING) / 2;
		for (int j = 0 ; j <= i ; j++) {
			Position new_position;
			new_position.x = starting_offset +
				(j * (FACE_THUMBNAIL_SIZE +
				      FACE_THUMBNAIL_SPACING));
			new_position.y = data->face_banner.y +
				FACE_THUMBNAIL_SPACING;
			new_position.width = FACE_THUMBNAIL_SIZE;
			new_position.height = FACE_THUMBNAIL_SIZE;

			data->history_thumb_position[i].push_back(new_position);
		}
	}
}

/**
 * This function files in a directory randomly
 */
static std::string gui_get_files_in_direcotry_randomly(std::string directory)
{
	std::stringstream file_path_sstr;

	/* Get the list of whan all the file have been used */
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
 * This function is call when still picture functionnality  is activated. It
 * executes the NN inferecnes on a picture stored in the file system.
 */
static void gui_still_picture_processing(std::string file,
					 CustomData *data)
{
	GdkWindow *window = gtk_widget_get_window(data->window);

	/* Read and format the picture */
	cv::Mat img_bgr, img_bgr_resized, img_nn;

	img_bgr = cv::imread(file);
	data->frame_width =  img_bgr.size().width;
	data->frame_height = img_bgr.size().height;

	/* Get final frame position and dimension and resize it */
	gui_compute_frame_position(window, data);
	cv::Size size(data->frame_fullscreen_pos.width, data->frame_fullscreen_pos.height);
	cv::resize(img_bgr, img_bgr_resized, size);

	/* Update the frame size after having resizing it */
	data->frame_width =  img_bgr_resized.size().width;
	data->frame_height = img_bgr_resized.size().height;

	/* expand the color space before displaying the frame */
	cv::cvtColor(img_bgr_resized, data->pict_bgra_resized, cv::COLOR_BGR2BGRA);

	/* Save a reference on the frame */
	cv::cvtColor(img_bgr_resized, data->img_rgb, cv::COLOR_BGR2RGB);

	/* Generate the cairo surface */
	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24,
						   data->frame_fullscreen_pos.width);

	data->picture = cairo_image_surface_create_for_data(data->pict_bgra_resized.data,
							    CAIRO_FORMAT_RGB24,
							    data->frame_fullscreen_pos.width,
							    data->frame_fullscreen_pos.height,
							    stride);

	/* Execute the face detect inference */
	fd_inference_time = nn_fd_inference(data->img_rgb, data);
	fr_inference_time = 0;

	/* Execute the face reco inference */
	if(!data->detected_faces.empty()) {
		for (uint32_t i = 0 ; i < data->detected_faces.size() ; i++) {
			cv::Size size_facereco(data->fr_nn_input_width,
					       data->fr_nn_input_height);
			data->detected_faces[i].face_rgb =
				libfacedetect_tfl.GetDetectedFaceImage(data->img_rgb,
								       i,
								       size_facereco);

			float identity[IDENTITY_CLASSES];
			fr_inference_time += nn_fr_inference(data->detected_faces[i].face_rgb.data,
							     identity,
							     data);

			std::pair<std::string, float> pred =
				nn_fr_prediction(identity,
						 data);
			data->detected_faces[i].label = pred.first;
			data->detected_faces[i].similarity = pred.second;
		}
	}
}

/**
 * This function redraw the face locations on the preview
 */
static void gui_draw_face_positions(cairo_t *cr,
				    CustomData *data)
{
	mtx.lock();
	if(data->detected_faces.empty())
		goto end;

	/* reset the live face position */
	data->screen_face_position.clear();
	for (unsigned int i = 0; i < data->detected_faces.size() ; i++) {
		LiveFaceDetect new_screen_face_position;

		/* default color is red for unknown face */
		cairo_set_source_rgb (cr, 1.0, 0.0, 0.0);
		new_screen_face_position.registered = false;

		/* compute rectange position and dimensions */
		float x      = data->frame_fullscreen_pos.width  *
			data->detected_faces[i].bbox.top_left.x;
		float y      = data->frame_fullscreen_pos.height *
			data->detected_faces[i].bbox.top_left.y;
		float width  = data->frame_fullscreen_pos.width  *
			(data->detected_faces[i].bbox.bot_right.x -
			 data->detected_faces[i].bbox.top_left.x);
		float height = data->frame_fullscreen_pos.height *
			(data->detected_faces[i].bbox.bot_right.y -
			 data->detected_faces[i].bbox.top_left.y);

		cairo_rectangle(cr, int(x), int(y), int(width), int(height));
		cairo_stroke(cr);

		/* save the face position regarding the screen */
		new_screen_face_position.pos.x =
			data->frame_fullscreen_pos.x + x;
		new_screen_face_position.pos.y =
			data->frame_fullscreen_pos.y + y;
		new_screen_face_position.pos.width = width;
		new_screen_face_position.pos.height = height;

		std::stringstream info_sstr;
		info_sstr << data->detected_faces[i].label;

		/* if the face label is different from "unknown" then change
		 * the color to green */
		if (data->detected_faces[i].label.compare("unknown") != 0) {
			cairo_set_source_rgb (cr, 0.0, 1.0, 0.0);
			new_screen_face_position.registered = true;

			/* display similarity */
			std::stringstream similarity_sstr;
			info_sstr << std::right << std::setw(5) << std::fixed
				<< std::setprecision(0)
				<< data->detected_faces[i].similarity * 100
				<< "%";
		}

		data->screen_face_position.push_back(new_screen_face_position);

		/* display rectangle over the detected face */
		cairo_rectangle(cr, int(x), int(y), int(width), int(height));
		cairo_stroke(cr);

		/* display rectangle over the detected face */
		cairo_rectangle(cr, int(x), int(y) - 20 , int(width), 20);
		cairo_fill_preserve(cr);
		cairo_stroke(cr);

		/* display the label and the similarity */
		cairo_save(cr);
		cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
		cairo_move_to(cr, int(x) + 2, int(y) - 4);
		cairo_show_text(cr, info_sstr.str().c_str());
		cairo_restore(cr);
	}

end:
	mtx.unlock();
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

	if (data->preview_enabled) {
		/* Camera preview use case */

		/* Set the transparent background */
		cairo_set_source_rgba (cr, 0.31, 0.32, 0.31, 0.0);
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		cairo_paint(cr);

		if (first_call) {
			data->window_width = gdk_window_get_width(window);
			data->window_height = gdk_window_get_height(window);

			/* set font size according to the display size */
			gui_load_css(data);

			/* Calculating the position of the preview in fullscreen
			 * mode. This position is compute automaticaly by the
			 * waylandsink gstreamer element and is it is not
			 * exposed. Thus we need to calculate it by our own. */
			gui_compute_frame_position(window, data);

			/* Calulating the face thumbnail position and size
			 * according to the remaining space left at the right
			 * and left of the preview area. */
			gui_compute_history_thumbnail_position(window, data);
			initialize_face_database(data);

			/* Set the pipeline into playing state as face database has been
			 * initialized */
			gst_element_set_state(data->pipeline, GST_STATE_PLAYING);

			first_call = false;
		}
	} else {
		/* Still picture use case */

		/* As this function is called twice at the initialisation,
		 * return immediatly the first time it is called in a still
		 * picture context */
		if (first_call) {
			data->window_width = gdk_window_get_width(window);
			data->window_height = gdk_window_get_height(window);

			/* set font size according to the display size */
			gui_load_css(data);

			/* Calulating the face thumbnail position and size
			 * according to the remaining space left at the right
			 * and left of the preview area. */
			gui_compute_history_thumbnail_position(window, data);
			initialize_face_database(data);

			/* Display the first picture */
			/* Select a picture in the directory */
			data->pict_file = gui_get_files_in_direcotry_randomly(image_dir_str);
			gui_still_picture_processing(data->pict_file, data);

			first_call = false;
			return FALSE;
		}

		/* Set the black background */
		cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);
		cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
		cairo_paint(cr);

		/* Display the resized picture */
		cairo_set_source_surface(cr,
					 data->picture,
					 data->frame_fullscreen_pos.x,
					 data->frame_fullscreen_pos.y);

		cairo_paint(cr);
	}

	std::stringstream display_fps_sstr;
	display_fps_sstr << std::left  << std::setw(11) << "disp. fps:"
		<< std::right << std::setw(5) << std::fixed
		<< std::setprecision(1) << display_avg_fps
		<< std::right << std::setw(3) << "fps";

	std::stringstream inference_fps_sstr;
	inference_fps_sstr << std::left  << std::setw(11) << "inf. fps:"
		<< std::right << std::setw(5) << std::fixed
		<< std::setprecision(1) << nn_avg_fps
		<< std::right << std::setw(3) << "fps";

	std::stringstream inference_time_sstr;
	inference_time_sstr << std::left  << std::setw(11) << "inf. time:"
		<< std::right << std::setw(5) << std::fixed
		<< std::setprecision(1) << fd_inference_time
		<< std::right << std::setw(4) << "ms +"
		<< std::right << std::setw(6) << std::fixed
		<< std::setprecision(1) << fr_inference_time
		<< std::right << std::setw(2) << "ms";

	/* Draw the brain icon at the top left corner */
	cairo_set_source_surface(cr, data->brain_icon, 5, 5);
	cairo_paint(cr);

	/* Draw the exit icon at the top right corner */
	cairo_set_source_surface(cr,
				 data->exit_icon,
				 data->window_width - EXIT_AREA_WIDTH, 0);
	cairo_paint(cr);
	cairo_save(cr);

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
	} else {
		cairo_translate(cr, BRAIN_AREA_WIDTH, 0);
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
		cairo_show_text(cr, display_fps_sstr.str().c_str());
		cairo_move_to(cr, 2, 40);
		cairo_show_text(cr, inference_fps_sstr.str().c_str());
		cairo_move_to(cr, 2, 60);
		cairo_show_text(cr, inference_time_sstr.str().c_str());
	} else {
		/* Still picture use case */
		cairo_move_to(cr, 2, 30);
		cairo_show_text(cr, inference_time_sstr.str().c_str());
	}

	/* Draw bounding box of the 5 first detected faces */
	if (!data->preview_enabled) {
		/* Translate to the frame position */
		cairo_translate(cr,
				data->frame_fullscreen_pos.x - BRAIN_AREA_WIDTH,
				data->frame_fullscreen_pos.y);
	}

	gui_draw_face_positions(cr, data);

	/* Draw a black transparent banner to display the registered faces */
	cairo_restore(cr);

	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.60);
	cairo_rectangle(cr,
			data->face_banner.x, data->face_banner.y,
			data->face_banner.width, data->face_banner.height);
	cairo_fill_preserve(cr);
	cairo_stroke(cr);

	/* Display the history thumbnail of the registered face and draw a
	 * green rectange around the face that is currently recognized */
	unsigned int nb_registered_faces = data->registered_faces.size();
	if (nb_registered_faces > 0) {
		int nb_thumbnails = std::min((int)nb_registered_faces,
					     data->nb_history_thumbnails);
		std::vector<Position> thumb_pos =
			data->history_thumb_position[nb_thumbnails - 1];

		for (unsigned int i = 0 ; i < thumb_pos.size() ; i++) {
			unsigned int index = data->registered_faces.size() - i - 1;
			cairo_set_source_surface(cr,
						 data->registered_faces[index].cairo_s_face,
						 thumb_pos[i].x,
						 thumb_pos[i].y);
			cairo_paint(cr);

			for (unsigned int j = 0 ; j < data->detected_faces.size() ; j++) {
				double line_width = 4.0;
				cairo_set_line_width(cr, line_width);
				/* Display a green rectangle arounfd the faces
				 * that are currently recognized */
				if (data->registered_faces[index].label.compare(
				    data->detected_faces[j].label) == 0) {
					cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 1.0);
					cairo_rectangle(cr,
							thumb_pos[i].x + line_width / 2,
							thumb_pos[i].y + line_width / 2,
							thumb_pos[i].width - line_width,
							thumb_pos[i].height - line_width);
					cairo_stroke(cr);
				}
			}
		}
	}

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
	int button_index = 0;
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
			if(!gui_finalize_face_registering(entry_sstr.str(), data)) {
				/* If label already exists ask user to enter a
				 * new label */
				return;
			}
			if (!data->preview_enabled) {
				gui_still_picture_processing(data->pict_file,
							     data);
				gtk_widget_queue_draw(data->window);
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
	int y = data->window_height - allocation->height -
		data->face_banner.height;
	gtk_fixed_move(GTK_FIXED(data->keyboard_fixed), widget, x, y);
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
		}

		/* redraw UI if event occurs in the brain area */
		if ((event->x < BRAIN_AREA_WIDTH) &&
		    (event->y < BRAIN_AREA_HEIGHT)) {
			if (!data->preview_enabled) {
				/* Select a picture in the directory */
				data->pict_file = gui_get_files_in_direcotry_randomly(image_dir_str);
				gui_still_picture_processing(data->pict_file, data);
				gtk_widget_queue_draw(data->window);
				goto end;
			}
		}

		/* event occurs on one of the thumbnail face */
		unsigned int nb_registered_faces = data->registered_faces.size();
		if (nb_registered_faces != 0) {
			int nb_thumbnails = std::min((int)nb_registered_faces,
						     data->nb_history_thumbnails);
			std::vector<Position> thumb_pos = data->history_thumb_position[nb_thumbnails - 1];
			for (unsigned int i = 0 ; i < thumb_pos.size() ; i++) {
				if ((event->x > thumb_pos[i].x) &&
				    (event->x < thumb_pos[i].x + thumb_pos[i].width) &&
				    (event->y > thumb_pos[i].y) &&
				    (event->y < thumb_pos[i].y + thumb_pos[i].height)) {
					gui_delete_registered_face(i, data);
					if (!data->preview_enabled) {
						gui_still_picture_processing(data->pict_file,
									     data);
						gtk_widget_queue_draw(data->window);
					}
					goto end;
				}
			}
		}

		/* event occurs on one of the live face bouding box that has
		 * not been registered */
		for (unsigned int i = 0 ; i < data->screen_face_position.size() ; i++) {
			if (!data->screen_face_position[i].registered &&
			    (event->x > data->screen_face_position[i].pos.x) &&
			    (event->x < data->screen_face_position[i].pos.x + data->screen_face_position[i].pos.width) &&
			    (event->y > data->screen_face_position[i].pos.y) &&
			    (event->y < data->screen_face_position[i].pos.y + data->screen_face_position[i].pos.height)) {
				gui_register_new_face(i, data);
				goto end;
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

	if (data->preview_enabled)
		data->brain_icon = cairo_image_surface_create_from_png("/usr/local/demo-ai/computer-vision/tflite-face-recognition/bin/resources/ST7079_AI_neural_pink_65x80.png");
	else
		data->brain_icon = cairo_image_surface_create_from_png("/usr/local/demo-ai/computer-vision/tflite-face-recognition/bin/resources/ST7079_AI_neural_pink_65x80_next_inference.png");
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

		/* Execute the face detect inference */
		cv::Mat img_rgb(cv::Size(data->frame_width,
				data->frame_height), CV_8UC3, info.data,
				cv::Mat::AUTO_STEP);
		/* Save a reference on the frame */
		data->img_rgb = img_rgb;

		fd_inference_time = nn_fd_inference(img_rgb, data);
		fr_inference_time = 0;

		/* Execute the face reco inference */
		if(!data->detected_faces.empty()) {
			for (unsigned int i = 0 ; i < data->detected_faces.size() ; i++) {
				cv::Size size_facereco(data->fr_nn_input_width,
						       data->fr_nn_input_height);
				data->detected_faces[i].face_rgb =
					libfacedetect_tfl.GetDetectedFaceImage(data->img_rgb,
									       i,
									       size_facereco);

				float identity[IDENTITY_CLASSES];
				fr_inference_time += nn_fr_inference(data->detected_faces[i].face_rgb.data,
								     identity,
								     data);

				std::pair<std::string, float> pred =
					nn_fr_prediction(identity,
							 data);
				data->detected_faces[i].label = pred.first;
				data->detected_faces[i].similarity = pred.second;
			}
		}

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
	g_signal_connect(fpsmeasure2, "fps-measurements", G_CALLBACK(gst_fps_measure_nn_cb), NULL);

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
		"Usage: " << argv[0] << " [option]\n"
		"\n"
		"--reco_simultaneous_faces <val>:      number of faces that can be recognized simultaneously (default is 1)\n"
		"--reco_threshold <val>:               face recognition threshold for face similarity (default is 0.70 = 70%)\n"
		"--max_db_faces <val>:                 maximum number of faces to be stored in the database (default is 200)\n"
		"-i --image <directory path>:          image directory with images to be classified\n"
		"-v --video_device <n>:                video device (default /dev/video0)\n"
		"--frame_width  <val>:                 width of the camera frame (default is 640 pixels)\n"
		"--frame_height <val>:                 height of the camera frame (default is 480 pixels)\n"
		"--framerate <val>:                    frame rate of the camera (default is 15 fps)\n"
		"--help:                               show this help\n";
	exit(1);
}

/**
 * This function parse the parameters of the application
 */
#define OPT_FACE_RECO_SIM_FACE     1000
#define OPT_FACE_RECO_THRESHOLD    1001
#define OPT_FACE_RECO_MAX_DB_FACES 1002
#define OPT_FRAME_WIDTH            1003
#define OPT_FRAME_HEIGHT           1004
#define OPT_FRAMERATE              1005
void process_args(int argc, char** argv)
{
	const char* const short_opts = "i:v:h";
	const option long_opts[] = {
		{"reco_simultaneous_faces", required_argument, nullptr, OPT_FACE_RECO_SIM_FACE},
		{"reco_threshold",          required_argument, nullptr, OPT_FACE_RECO_THRESHOLD},
		{"max_db_faces",            required_argument, nullptr, OPT_FACE_RECO_MAX_DB_FACES},
		{"image",                   required_argument, nullptr, 'i'},
		{"video_device",            required_argument, nullptr, 'v'},
		{"frame_width",             required_argument, nullptr, OPT_FRAME_WIDTH},
		{"frame_height",            required_argument, nullptr, OPT_FRAME_HEIGHT},
		{"framerate",               required_argument, nullptr, OPT_FRAMERATE},
		{"help",                    no_argument,       nullptr, 'h'},
		{nullptr,                   no_argument,       nullptr, 0}
	};

	while (true)
	{
		const auto opt = getopt_long(argc, argv, short_opts, long_opts, nullptr);

		if (-1 == opt)
			break;

		switch (opt)
		{
		case OPT_FACE_RECO_SIM_FACE:
			reco_simultaneous_face = std::stoi(optarg);
			std::cout << "simultaneous face recognized set to: "
				<< reco_simultaneous_face << std::endl;
			break;
		case OPT_FACE_RECO_THRESHOLD:
			reco_threshold = std::stof(optarg);
			std::cout << "face reco threshold set to: "
				<< reco_threshold << std::endl;
			break;
		case OPT_FACE_RECO_MAX_DB_FACES:
			max_db_faces = std::stoi(optarg);
			std::cout << "maximum number of faces in the database set to: "
				<< max_db_faces << std::endl;
			break;
		case 'i':
			image_dir_str = std::string(optarg);
			std::cout << "image directory set to: "
				<< image_dir_str << std::endl;
			break;
		case 'v':
			video_device_str = std::string(optarg);
			std::cout << "camera device set to: /dev/video"
				<< video_device_str << std::endl;
			break;
		case OPT_FRAME_WIDTH:
			camera_width_str = std::string(optarg);
			std::cout << "camera frame width set to: "
				<< camera_width_str << std::endl;
			break;
		case OPT_FRAME_HEIGHT:
			camera_height_str = std::string(optarg);
			std::cout << "camera frame heightset to: "
				<< camera_height_str << std::endl;
			break;
		case OPT_FRAMERATE:
			camera_fps_str = std::string(optarg);
			std::cout << "camera framerate set to: "
				<< camera_fps_str << std::endl;
			break;
		case 'h': /* -h or --help */
		case '?': /* Unrecognized option */
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
	data.max_simultaneous_face = reco_simultaneous_face;
	data.face_reco_threshold = reco_threshold;
	data.max_db_faces = max_db_faces;

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
			return 0;
		}
	} else {
		data.preview_enabled = false;

		/* Check if directory is empty */
		std::string file = gui_get_files_in_direcotry_randomly(image_dir_str);
		if (file.empty()) {
			g_printerr("ERROR: Image directory %s is empty\n", image_dir_str.c_str());
			return 0;
		}
	}

	/* Face detect library initialization */
	libfacedetect_tfl.Initialize(data.max_simultaneous_face,
				     2 /*number_of_threads*/);

	/* Face reco library initialization */
	libfacereco_tfl.Initialize(2 /*number_of_threads*/);

	/* Get model input size */
	data.fr_nn_input_width = libfacereco_tfl.GetInputWidth();
	data.fr_nn_input_height = libfacereco_tfl.GetInputHeight();

	/* Initialize GTK */
	gtk_init(&argc, &argv);

	if (data.preview_enabled) {
		/* Initialize GStreamer for camera preview use case*/
		gst_init(&argc, &argv);

		/* Create the GStreamer pipeline for camera use case */
		ret = gst_pipeline_camera_creation(&data);
		if(ret)
			return -1;

		std::unique_lock<std::mutex> lk(cond_var_m);
		cond_var.wait(lk);
	}

	if (data.preview_enabled) {
		/* Set the pipeline into pause state and restart it once the
		 * database will be initialized */
		gst_element_set_state(data.pipeline, GST_STATE_PAUSED);
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
