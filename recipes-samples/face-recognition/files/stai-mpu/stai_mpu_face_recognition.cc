/*
 * Copyright (c) 2024 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 */

/*
 * This application demonstrate a computer vision use case for face detection +
 * face recognition. Frames are grabbed from a camera input (/dev/videox) and
 * analyzed by a neural network model interpreted by stai_mpu framework.
 *
 * Gstreamer pipeline is used to stream camera frames (using v4l2src), to
 * display a preview (using waylandsink) and to execute neural network inference
 * (using appsink).
 *
 * The result of the inference is displayed on the preview. The overlay is done
 * using GTK widget with cairo.
 *
 * This combination is quite simple and efficient in term of CPU consumption.
 */

#include <filesystem>
#include <stdio.h>
#include <getopt.h>
#include <glib.h>
#include <numeric>
#include <sys/stat.h>
#include <sys/time.h>
#include <semaphore.h>
#include <fstream>
#include <opencv2/opencv.hpp>
#include <thread>
#include <gst/video/videooverlay.h>
#include <gst/gst.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>

#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#else
#error "Wayland is not supported in GTK+"
#endif

#define FACE_IDENTITY_CLASSES 512
#define MAX_HISTORY_THUMBNAILS  11 /* for 720p display */

#include "stai_mpu_wrapper.hpp"
#include "blazeface_pp.hpp"
#include "facenet_pp.hpp"

/* Application parameters */
std::vector<std::string> dir_files;
std::string model_file_str;
std::string model_file_fr_str;
std::string image_dir_str;
std::string database_dir_str;
std::string video_device_str  = "";
std::string camera_fps_str    = "15";
std::string camera_width_str  = "640";
std::string camera_height_str = "480";
std::string val_run_str = "50";

bool verbose = false;
bool validation = false;
bool face_recognition_done = true;
bool database_init = false;
bool reco_simultaneous_face = false;
float input_mean = 127.5f;
float input_std = 127.5f;
float reco_threshold = 0.40;

int max_db_faces = 200;

struct wrapper_stai_mpu::stai_mpu_wrapper stai_mpu_wrapper;
struct wrapper_stai_mpu::stai_mpu_wrapper stai_mpu_wrapper_fr;
struct wrapper_stai_mpu::Config config;
struct wrapper_stai_mpu::Config config_fr;
nn_postproc::BlazeFace blaze_face;
nn_postproc::inference_Results results;
nn_postproc_fr::inference_Results results_fr;
std::vector<std::string> labels;

bool gtk_main_started = false;
bool exit_application = false;
bool isp_first_config = true;

/* Resource directory on board */
#define DEFAULT_DATABASE_DIRECTORY "/usr/local/x-linux-ai/face-recognition/database/"
#define RESOURCES_DIRECTORY "/usr/local/x-linux-ai/resources/"

/* Structure that contains frame size/position on the screen*/
typedef struct _FramePosition {
	int x;
	int y;
	int width;
	int height;
} FramePosition;

struct Point {
	float x, y;
};

struct Bbox {
	Point top_left;
	Point bot_right;
};

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
	float identity[FACE_IDENTITY_CLASSES];
} RegisteredFace;

typedef struct _DetectedFace {
	cv::Mat face_rgb;
	Bbox bbox;
	std::string label;
	float identity[FACE_IDENTITY_CLASSES];
	float similarity;
} DetectedFace;

typedef struct _Key{
	int id;
	GtkWidget *button;
} Key;

/* Structure that contains all camera configuration information */
typedef struct _Config_camera {
	std::string video_device;
	std::string camera_caps;
	std::string nn_device;
	std::string nn_caps;
	std::string dcmipp_sensor;
	std::string aux_postproc;
	std::string main_postproc;
} Config_camera;

/* Synchronization variables */
std::mutex mtx;

/* Structure that contains all information to pass around */
typedef struct _CustomData {
	/* The gstreamer pipeline */
	GstElement *pipeline;
	GstElement *pipeline_nn;

	/* The GTK window widget */
	GtkWidget *window_main;
	GtkWidget *window_overlay;

	/* The drawing area where the camera stream is shown */
	GtkWidget *video;
	GstVideoOverlay *video_overlay;
	GtkAllocation video_allocation;

	/*  Camera information  */
	Config_camera camera_info;

	/* Text widget to display info about
	 * - either the display framerate, and the inference time
	 * - in fps or ms of the model
	 */
	GtkWidget *info_inf_time;
	GtkWidget *overlay_draw;
	GtkWidget *info_inf_time_main;
	std::stringstream label_sstr;

	/* window resolution */
	int window_width;
	int window_height;

	/* UI parameters (values will depends on display size) */
	int ui_cairo_font_size_label;
	int ui_cairo_font_size;
	int ui_icon_exit_size;
	int	ui_icon_st_size;
	double ui_box_line_width;
	int ui_weston_panel_thickness;
	int ui_face_thumb_size;
	int ui_face_thumb_spacing;
	double ui_thumb_box_line_width;
	std::string keyboard_config;

	/* ST icon widget */
	GtkWidget *st_icon_main;
	GtkWidget *st_icon_ov;
	std::stringstream st_icon_sstr;

	/* exit icon widget */
	GtkWidget *exit_icon_main;
	GtkWidget *exit_icon_ov;
	std::stringstream exit_icon_sstr;

	/* Preview camera enable (else still picture use case) */
	bool preview_enabled;

	/* NN input size */
	int nn_input_width;
	int nn_input_height;
	int nn_fr_input_width;
	int nn_fr_input_height;

	/* Frame resolution (still picture or camera) */
	int frame_width;
	int frame_height;

	/* drawing area resolution */
	int widget_draw_width;
	int widget_draw_height;
	int widget_draw_ov_height;
	int widget_draw_ov_width;
	int offset;

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

	/* For validation purpose */
	int valid_timeout_id;
	int valid_draw_count;
	int val_run;
	std::vector<double> valid_fd_inference_time;
	std::vector<double> valid_fr_inference_time;
	float total_face_reco_inference_time;

	std::string file;

	/* Display frame position */
	FramePosition frame_disp_pos;

	/* Face reco parameters */
	float face_reco_threshold;

	/*  Still picture variables */
	bool new_inference;
	cv::Mat img_to_display;

	/* ISP configuration */
	int cpt_frame = 0;

} CustomData;

/* Framerate statistics */
gdouble display_avg_fps = 0;

/* Structure that contains validation information */
typedef struct _ValidFaceInfo {
	std::string name;
	Bbox bbox;
} ValidFaceInfo;

typedef struct _ValidInfo {
	/* The gstreamer pipeline */
	std::string file_name;
	std::vector<ValidFaceInfo> faces_info;
} ValidInfo;

/**
 * This function is used to get the display resolution
 */
auto get_display_resolution(CustomData *data){
	std::stringstream cmd;
	cmd << "modetest -M stm -c > /tmp/display_resolution.txt";
	system(cmd.str().c_str());
	std::string display_info_pattern = "#0";
	std::ifstream file("/tmp/display_resolution.txt");
	std::string display_information;
	std::string display_resolution;
	std::string display_width;
	std::string display_height;
	if (file.is_open()) {
		std::string line;
		while (std::getline(file, line)) {
			int found_resolution = line.find(display_info_pattern);
			if (found_resolution != -1) {
				display_information = line.substr((found_resolution + display_info_pattern.length()));
			}
	    }
	    file.close();
	}
	system("rm -rf /tmp/display_resolution.txt");
	std::stringstream display_resolution_ss(display_information);
	while(std::getline(display_resolution_ss, display_resolution, ' ')) {
		int found = display_resolution.find("x");
		if (found != -1) {
			display_width = display_resolution.substr(0,found);
			display_height = display_resolution.substr(found + 1);
		}
	}
	g_print("display resolution is : %s x %s \n",display_width.c_str(),display_height.c_str());
	data->window_width = std::stoi(display_width);
	data->window_height = std::stoi(display_height);
	return 0;
}

/**
 * This function is used to setup the camera depending on the
 * parameters defined by the user
 */
auto setup_camera(std:: string nn_input_width, std::string nn_input_height) {
	std::stringstream config_camera;
	config_camera << RESOURCES_DIRECTORY << "setup_camera.sh " << camera_width_str << " " << camera_height_str << " " << camera_fps_str << " " << nn_input_width << " " << nn_input_height << " " <<  video_device_str << " > /tmp/camera_device.txt";
	system(config_camera.str().c_str());
	std::string video_device_pattern = "V4L_DEVICE_PREV=";
	std::string nn_device_pattern = "V4L_DEVICE_NN=";
	std::string camera_caps_pattern = "V4L2_CAPS_PREV=";
	std::string nn_caps_pattern = "V4L2_CAPS_NN=";
	std::string dcmipp_sensor_pattern = "DCMIPP_SENSOR=";
	std::string aux_postproc_pattern = "AUX_POSTPROC=";
	std::string main_postproc_pattern = "MAIN_POSTPROC=";

	Config_camera camera_info;
	std::ifstream file("/tmp/camera_device.txt");
	if (file.is_open()) {
		std::string line;
		while (std::getline(file, line)) {
			int found_device = line.find(video_device_pattern);
			if (found_device != -1) {
				camera_info.video_device = line.substr((found_device + video_device_pattern.length()));
			}
			int found_camera = line.find(camera_caps_pattern);
			if (found_camera != -1) {
				camera_info.camera_caps = line.substr((found_camera + camera_caps_pattern.length()));
			}
			int found_nn_device = line.find(nn_device_pattern);
			if (found_nn_device != -1) {
				camera_info.nn_device = line.substr((found_nn_device + nn_device_pattern.length()));
			}
			int found_nn_caps = line.find(nn_caps_pattern);
			if (found_nn_caps != -1) {
				camera_info.nn_caps = line.substr((found_nn_caps + nn_caps_pattern.length()));
			}
			int found_dcmipp = line.find(dcmipp_sensor_pattern);
			if (found_dcmipp != -1) {
				camera_info.dcmipp_sensor = line.substr((found_dcmipp + dcmipp_sensor_pattern.length()));
			}
			int found_aux_postproc = line.find(aux_postproc_pattern);
			if (found_aux_postproc != -1) {
				camera_info.aux_postproc = line.substr((found_aux_postproc + aux_postproc_pattern.length()));
			}
			int found_main_postproc = line.find(main_postproc_pattern);
			if (found_main_postproc != -1) {
				camera_info.main_postproc = line.substr((found_main_postproc + main_postproc_pattern.length()));
			}
	    }
	    file.close();
	}
	system("rm -rf /tmp/camera_device.txt");
	return camera_info;
}

/**
 * This function is called to for updating ISP configuration
 */
auto update_isp_config(CustomData *data)
{
	std::stringstream isp_file;
	std::stringstream isp_config_gamma_0;
	std::stringstream isp_config_gamma_1;
	std::stringstream isp_config_whiteb;
	std::stringstream isp_config_autoexposure;

	isp_file << "/usr/local/demo/bin/dcmipp-isp-ctrl";
	isp_config_gamma_0 << "v4l2-ctl -d " << data->camera_info.aux_postproc << " -c gamma_correction=0";
	isp_config_gamma_1 << "v4l2-ctl -d " << data->camera_info.aux_postproc << " -c gamma_correction=1";

	isp_config_whiteb << isp_file.str().c_str() << " -i0 ";
	isp_config_autoexposure << isp_file.str().c_str() << " -g > /dev/null";

	std::ifstream file(isp_file.str().c_str());

	if (isp_first_config && file.good() && data->camera_info.dcmipp_sensor.compare("imx335") == 0 ){
		system(isp_config_gamma_0.str().c_str());
		system(isp_config_gamma_1.str().c_str());
		system(isp_config_whiteb.str().c_str());
		system(isp_config_autoexposure.str().c_str());
		isp_first_config = false;
	}

	if (data->cpt_frame == 0 && file.good() && data->camera_info.dcmipp_sensor.compare("imx335") == 0 ){
		system(isp_config_whiteb.str().c_str());
		system(isp_config_autoexposure.str().c_str());
	}

    return TRUE;
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
{;
	stai_mpu_wrapper.RunInference(img);
	results.inference_time = stai_mpu_wrapper.GetInferenceTime();
}

static void nn_fr_inference(uint8_t *img)
{
	stai_mpu_wrapper_fr.RunInference(img);
	results_fr.inference_time = stai_mpu_wrapper_fr.GetInferenceTime();
}

/**
 * This function is used to process inference results and
 * and extract class detected and accuracy
 */
static void nn_postprocessing(){
	nn_postproc::nn_post_proc(stai_mpu_wrapper.m_stai_mpu_model, stai_mpu_wrapper.m_output_infos, &results, &blaze_face);
}

static void nn_fr_postprocessing(){
	nn_postproc_fr::nn_post_proc(stai_mpu_wrapper_fr.m_stai_mpu_model, stai_mpu_wrapper_fr.m_output_infos, &results_fr);
}

static int load_valid_results_from_json_file(std::string file_name, std::vector<ValidFaceInfo> *faces_info)
{
	std::stringstream json_file_sstr;
	json_file_sstr << file_name << ".json";

	int ret = access(json_file_sstr.str().c_str(),R_OK);
	if (ret != 0){
		g_print("Read permission denied to the file %s\n",json_file_sstr.str().c_str());
		exit(1);
	}

	FILE* fp = fopen(json_file_sstr.str().c_str(), "r");
	if (fp == NULL)
		return -1;

	char readBuffer[65536];
	rapidjson::FileReadStream is(fp, readBuffer, sizeof(readBuffer));

	rapidjson::Document json_value;
	json_value.ParseStream(is);

	if(json_value.HasMember("faces_info"))
	{
		const rapidjson::Value& obj_info_array = json_value["faces_info"];
		for (unsigned int i = 0; i < obj_info_array.Size(); i++) {
			ValidFaceInfo valid_obj_info;
			valid_obj_info.name = obj_info_array[i]["name"].GetString();
			valid_obj_info.bbox.top_left.x = std::stof(obj_info_array[i]["top_left_x"].GetString());
			valid_obj_info.bbox.top_left.y = std::stof(obj_info_array[i]["top_left_y"].GetString());
			valid_obj_info.bbox.bot_right.x = std::stof(obj_info_array[i]["bot_right_x"].GetString());
			valid_obj_info.bbox.bot_right.y = std::stof(obj_info_array[i]["bot_right_y"].GetString());
			faces_info->push_back(valid_obj_info);
		}
	}

	fclose(fp);
	return 0;
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

	cv::Size size(data->ui_face_thumb_size, data->ui_face_thumb_size);
	cv::resize(face_bgra, new_face.face_bgra_thumb, size);

	int stride = cairo_format_stride_for_width(CAIRO_FORMAT_RGB24,
						   data->ui_face_thumb_size);
	new_face.cairo_s_face = cairo_image_surface_create_for_data(new_face.face_bgra_thumb.data,
						    CAIRO_FORMAT_RGB24,
						    data->ui_face_thumb_size,
						    data->ui_face_thumb_size,
						    stride);

	cv::Size size_nn(data->nn_fr_input_width,data->nn_fr_input_height);
	cv::resize(face_bgr, img_nn, size_nn);
	cv::cvtColor(img_nn, img_nn, cv::COLOR_BGR2RGB);

	nn_fr_inference(img_nn.data);
	nn_fr_postprocessing();
	std::copy(std::begin(results_fr.nn_output), std::end(results_fr.nn_output), std::begin(new_face.identity));
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

	DIR* dirp = NULL;
	/* Create the directory if it does not exist */
	if (!std::filesystem::exists(database_dir_str.c_str())) {
		mkdir(database_dir_str.c_str(),
		      S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	}

	/* checking if the directory can be opened */
	if ((dirp = opendir(database_dir_str.c_str())) == NULL) {
		g_printerr("Cannot open %s\n",database_dir_str.c_str());
		exit (1);
	}

	/* Get the list of all the files */
	struct dirent * dp;
	while ((dp = readdir(dirp)) != NULL) {
		if ((strcmp(dp->d_name, ".") !=0) &&
		    (strcmp(dp->d_name, "..") != 0)) {
			std::stringstream file_path_sstr;
			file_path_sstr << database_dir_str << dp->d_name;
			files.push_back(file_path_sstr.str());
		}
	}

	closedir(dirp);

	/* sort file by modification date */
	std::sort(files.begin(), files.end(), compare_modif_date);

	/* If no face in the data base return */
	if (files.size() == 0){
		return;
	}

	for (unsigned int i = 0 ; i < files.size() ; i++) {
		register_new_face_from_file(database_dir_str.c_str(),
					    files[i], data);
	}
}

/**
 * This function is called when a click on "return" key of the virtual
 * keyboard is registered.
 */
static bool gui_finalize_face_registering(std::string label,
					  CustomData *data)
{
	std::stringstream file_name_sstr;
	file_name_sstr << database_dir_str << label << ".png";

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
	tmp_sstr << database_dir_str << "tmp.png";
	rename(tmp_sstr.str().c_str(), file_name_sstr.str().c_str());

	register_new_face_from_file(database_dir_str.c_str(),
				    file_name_sstr.str(), data);
	return true;
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

	/* Get the list of all the file in the directory if the list of the
	 * file is empty */
	if (dir_files.size() == 0) {
		DIR* dirp;
		/* checking if the directory can be opened */
		if ((dirp = opendir(directory.c_str())) == NULL) {
			g_printerr("Cannot open %s\n",directory.c_str());
			exit (1);
		}
		struct dirent * dp;
		while ((dp = readdir(dirp)) != NULL) {
			if ((strcmp(dp->d_name, ".") != 0) &&
			    (strcmp(dp->d_name, "..") != 0) &&
			    (strstr(dp->d_name, ".json") == 0))
				dir_files.push_back(dp->d_name);
		}
		closedir(dirp);
	}

	/* Check if directory is empty */
	if (dir_files.size() == 0){
		return "";
	}

	int random = rand() % dir_files.size();
	file_path_sstr << directory << dir_files[random];
	dir_files.erase(dir_files.begin() + random);
	return file_path_sstr.str();
}


/**
 * This function is an idle function waiting for a new picture
 * to be inferred
 */
bool first_call_validation = true;
static gboolean infer_new_picture(CustomData *data)
{

	if (exit_application) {
		gtk_main_quit();
		return FALSE;
	}

	if (data->new_inference){
		/* Select a picture in the directory */
		data->file = get_files_in_directory_randomly(image_dir_str);
		/* checking the reading permission of the file */
		int ret = access(data->file.c_str(),R_OK);
		if (ret != 0){
			g_printerr("Read permission denied to the file %s\n",data->file.c_str());
			exit(1);
		}
		/* Read and format the picture */
		cv::Mat img_bgr, img_bgra, img_tdp, img_nn, cropped_frame;

		img_bgr = cv::imread(data->file);
		cv::cvtColor(img_bgr, img_bgra, cv::COLOR_BGR2BGRA);
		data->frame_width =  img_bgra.size().width;
		data->frame_height = img_bgra.size().height;
		compute_frame_position(data);

		/* Get final frame position and dimension and resize it */
		cv::Size size(data->frame_disp_pos.width,data->frame_disp_pos.height);
		cv::resize(img_bgra, img_tdp , size);
		data->img_to_display = img_tdp.clone();

		/* prepare the inference */
		cv::Size size_nn(data->nn_input_width, data->nn_input_height);
		cv::resize(img_bgr, img_nn, size_nn);
		cv::cvtColor(img_nn, img_nn, cv::COLOR_BGR2RGB);

		nn_inference(img_nn.data);
		nn_postprocessing();
		data->detected_faces.clear();
		if (reco_simultaneous_face){
			for (uint32_t i = 0 ; i < results.detected_faces.size() ; i ++) {
				DetectedFace new_face;
				Bbox bbox;
				bbox.top_left.x = results.detected_faces[i].landmarks.face.x0;
				bbox.top_left.y = results.detected_faces[i].landmarks.face.y0;
				bbox.bot_right.x = results.detected_faces[i].landmarks.face.x1;
				bbox.bot_right.y = results.detected_faces[i].landmarks.face.y1;
				new_face.label = "unknown";
				new_face.bbox = bbox;
				data->detected_faces.push_back(new_face);
			}
		} else {
			if (!results.detected_faces.empty()){
				DetectedFace new_face;
				Bbox bbox;
				bbox.top_left.x = results.detected_faces[0].landmarks.face.x0;
				bbox.top_left.y = results.detected_faces[0].landmarks.face.y0;
				bbox.bot_right.x = results.detected_faces[0].landmarks.face.x1;
				bbox.bot_right.y = results.detected_faces[0].landmarks.face.y1;
				new_face.label = "unknown";
				new_face.bbox = bbox;
				data->detected_faces.push_back(new_face);
			}
		}
		if(!data->detected_faces.empty()) {
			data->total_face_reco_inference_time = 0;
			for (uint32_t i = 0 ; i < data->detected_faces.size() ; i++) {
				float face_x0 = data->frame_disp_pos.width  * data->detected_faces[i].bbox.top_left.x;
				float face_y0 = data->frame_disp_pos.height * data->detected_faces[i].bbox.top_left.y;
				float face_x1 = data->frame_disp_pos.width  * data->detected_faces[i].bbox.bot_right.x;
				float face_y1 = data->frame_disp_pos.height * data->detected_faces[i].bbox.bot_right.y;
				float width_box = data->frame_disp_pos.width  * (data->detected_faces[i].bbox.bot_right.x - data->detected_faces[i].bbox.top_left.x);
				float height_box =  data->frame_disp_pos.height *(data->detected_faces[i].bbox.bot_right.y - data->detected_faces[i].bbox.top_left.y);
				cv::Rect crop_region(face_x0, face_y0, width_box, height_box);
				cropped_frame = data->img_to_display(crop_region);
				cv::resize(cropped_frame,cropped_frame,cv::Size(160,160));
				cv::cvtColor(cropped_frame, cropped_frame, cv::COLOR_BGR2RGB);
				data->detected_faces[i].face_rgb = cropped_frame.clone();
				nn_fr_inference(data->detected_faces[i].face_rgb.data);
				nn_fr_postprocessing();
				std::copy(std::begin(results_fr.nn_output), std::end(results_fr.nn_output), std::begin(data->detected_faces[i].identity));
				data->total_face_reco_inference_time += results_fr.inference_time;
				for (unsigned int j = 0 ; j < data->registered_faces.size() ; j++) {
					float similarity = nn_postproc_fr::cosine_similarity(data->detected_faces[i].identity, data->registered_faces[j].identity, FACE_IDENTITY_CLASSES);
					if(similarity <= reco_threshold){
						data->detected_faces[i].label =  data->registered_faces[j].label;
						data->detected_faces[i].similarity = similarity;
						break;
					} else {
						data->detected_faces[i].label = "unknown";
						data->detected_faces[i].similarity = similarity;
					}
				}
			}
		}

		/* Updating the information with the new inference results */
		std::stringstream inference_time_sstr;
		std::stringstream inference_time_fr_sstr;
		std::stringstream inference_fps_sstr;

		float inf_time = 0;
		float inf_time_fr = 0;
		if (results.detected_faces.size() != 0)
		{
			inf_time = results.inference_time;
			inf_time_fr = data->total_face_reco_inference_time;
			float total_inf = inf_time + inf_time_fr;

			inference_time_sstr << std::right << std::setw(5) << std::fixed << std::setprecision(1) << inf_time << std::right << std::setw(2) << " ms ";
			inference_time_fr_sstr << std::right << std::setw(5) << std::fixed << std::setprecision(1) << inf_time_fr << std::right << std::setw(2) << " ms ";

			inference_fps_sstr << std::right << std::setw(5) << std::fixed << std::setprecision(1) << (1000 / total_inf);
			inference_fps_sstr << std::right << std::setw(3) << " fps ";
		}

		/*  Update labels */
		std::stringstream info_sstr;
		info_sstr << "  inf.fps :     " << "\n" << inference_fps_sstr.str().c_str() << "\n" << "  inf.time :     " << "\n" << inference_time_sstr.str().c_str() << "\n" << " + " << "\n" << inference_time_fr_sstr.str().c_str();
		char *label_to_display = g_strdup_printf ("<span line_height=\"1\"  font=\"%d\" color=\"#FFFFFFFF\">""<b>%s\n</b>""</span>",data->ui_cairo_font_size,info_sstr.str().c_str());
		gtk_label_set_markup(GTK_LABEL(data->info_inf_time),label_to_display);
		g_free(label_to_display);

		if (validation) {
			data->valid_fd_inference_time.push_back(inf_time);
			data->valid_fr_inference_time.push_back(inf_time_fr);

			/* Reload the timeout */
			g_source_remove(data->valid_timeout_id);
			data->valid_timeout_id = g_timeout_add(5000,
							       valid_timeout_callback,
							       NULL);

			std::stringstream label_sstr;

			std::cout << "\nInput file: " << data->file <<std::endl;

			/* Get the name of the file without extension */
			std::string pict_file =  data->file.substr(0,  data->file.find_last_of('.'));
			std::vector<ValidFaceInfo> faces_info;

			/* Load associated JSON file information */
			int ret = load_valid_results_from_json_file(pict_file, &faces_info);
			if(ret) {
				std::cout << "JSON file reading is failing (" << pict_file << ".json)\n";
				exit(1);
			}

			/* Compare the number of detected face and the expected
			 * validation result */
			std::cout << "\texpect " << faces_info.size() << " faces. Face recognition inference found " << data->detected_faces.size() << " faces." << std::endl;
			if (data->detected_faces.size() != faces_info.size()) {
				std::cout << "Inference result not aligned with the expected validation result\n";
				exit(5);
			}

			for (unsigned int i = 0; i < faces_info.size(); i++) {
				std::cout << "\t"
					<< std::left  << std::setw(12)
					<< data->detected_faces[i].label
					<< " (x0 y0 x1 y1) "
					<< std::left  << std::setw(12)
					<< data->detected_faces[i].bbox.top_left.x
					<< std::left  << std::setw(12)
					<< data->detected_faces[i].bbox.top_left.y
					<< std::left  << std::setw(12)
					<< data->detected_faces[i].bbox.bot_right.x
					<< std::left  << std::setw(12)
					<< data->detected_faces[i].bbox.bot_right.y
					<< "  expected result: "
					<< std::left  << std::setw(12)
					<< faces_info[i].name
					<< " (x0 y0 x1 y1) "
					<< std::left  << std::setw(12)
					<< faces_info[i].bbox.top_left.x
					<< std::left  << std::setw(12)
					<< faces_info[i].bbox.top_left.y
					<< std::left  << std::setw(12)
					<< faces_info[i].bbox.bot_right.x
					<< faces_info[i].bbox.bot_right.y << std::endl;

				if (faces_info[i].name.compare(data->detected_faces[i].label) != 0) {
					std::cout << "Inference result not aligned with the expected validation result\n";
					exit(5);
				}

				float error_epsilon = 0.02;
				if ((fabs(data->detected_faces[i].bbox.top_left.x - faces_info[i].bbox.top_left.x) > error_epsilon) ||
				    (fabs(data->detected_faces[i].bbox.top_left.y - faces_info[i].bbox.top_left.y) > error_epsilon) ||
				    (fabs(data->detected_faces[i].bbox.bot_right.x - faces_info[i].bbox.bot_right.x) > error_epsilon) ||
				    (fabs(data->detected_faces[i].bbox.bot_right.y - faces_info[i].bbox.bot_right.y) > error_epsilon)) {
					std::cout << "Inference result not aligned with the expected validation result\n";
					exit(5);
				}
			}

			/* Continue over all files */
			if (dir_files.size() == 0) {
				auto avg_fd_inf_time = std::accumulate(data->valid_fd_inference_time.begin(),
								       data->valid_fd_inference_time.end(), 0.0) /
					data->valid_fd_inference_time.size();
				auto avg_fr_inf_time = std::accumulate(data->valid_fr_inference_time.begin(),
								       data->valid_fr_inference_time.end(), 0.0) /
					data->valid_fr_inference_time.size();
				std::cout << "avg fd inference time= " << avg_fd_inf_time << std::endl;
				std::cout << "avg fr inference time= " << avg_fr_inf_time << std::endl;
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

/**
 * This function is called when a click on the thumbnail face is received
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
 * This function is used to load the CSS file for UI configuration
 */
static void gui_gtk_style(CustomData *data)
{
	std::stringstream css_sstr;
	css_sstr << RESOURCES_DIRECTORY;
	css_sstr << "Face_Recognition.css";

	/* checking the reading permission of the file */
	int ret = access(css_sstr.str().c_str(),R_OK);
	if (ret != 0){
		g_printerr("Read permission denied to the file %s\n",css_sstr.str().c_str());
		exit(1);
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
 * This function is called when a click on the live bounding box is received
 */
static void gui_register_new_face(int index,
				  CustomData *data)
{
	mtx.lock();
	if (data->detected_faces.empty()) {
		/* The click event has been registered when the bounding box just
		 * disappeared => do not register any face.*/
		goto end;
	}

	if ((int)data->registered_faces.size() < data->max_db_faces) {
		/* display the virtual keyboard to enter the face label */
		gtk_label_set_markup(GTK_LABEL(data->keyboard_label),
				     "<span font='15' color='#00000000'>"
				     "<b>Please enter name...</b></span>");
		gtk_widget_show_all(data->keyboard);

		cv::Mat face_bgra;
		cv::cvtColor(data->detected_faces[index].face_rgb,
			     face_bgra,
			     cv::COLOR_RGB2BGRA);

		std::stringstream tmp_sstr;
		tmp_sstr << database_dir_str << "tmp.png";
		/* checking the writing permission of the file */
		int ret = access(database_dir_str.c_str(),W_OK);
		if (ret != 0){
			g_printerr("Write permission denied to the directory %s\n",database_dir_str.c_str());
			exit(1);
		}

		bool result = cv::imwrite(tmp_sstr.str().c_str(), face_bgra);
		if (result) {
			std::cout << "Image successfully written to " << database_dir_str.c_str() << std::endl;
		} else {
			std::cerr << "Failed to write image to " << database_dir_str.c_str() << std::endl;
		}
	} else {
		LOG(INFO) << "You reach the maximum number of faces to be registered!\n";
	}

end:
	mtx.unlock();
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
	 * final step else if the del button is pressed, the last character from
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
				data->new_inference = true;
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
	int window_constraint = 0;
	if (data->window_height > data->window_width)
		window_constraint = data->window_height;
	else
		window_constraint = data->window_width;

	/* Move the keyboard to be centered */
	int x = (window_constraint - allocation->width) / 2;
	int y = data->widget_draw_ov_height - allocation->height -
		data->face_banner.height;
	gtk_fixed_move(GTK_FIXED(data->keyboard_fixed), widget, x, y);
}

/**
 * This function is called when a click or touch event is received
 */
static gboolean gui_press_event_cb(GtkWidget *widget,
				   GdkEventButton *event,
				   CustomData *data)
{
	if (event->button == GDK_BUTTON_PRIMARY) {
		/* event occurs on one of the thumbnail face */
		unsigned int nb_registered_faces = data->registered_faces.size();
		if (nb_registered_faces != 0) {
			int nb_thumbnails = std::min((int)nb_registered_faces,
						     data->nb_history_thumbnails);
			std::vector<Position> thumb_pos = data->history_thumb_position[nb_thumbnails - 1];
			for (unsigned int i = 0 ; i < thumb_pos.size() ; i++) {
				if ((event->x - data->offset > thumb_pos[i].x) &&
				    (event->x - data->offset< thumb_pos[i].x + thumb_pos[i].width) &&
				    (event->y > thumb_pos[i].y) &&
				    (event->y < thumb_pos[i].y + thumb_pos[i].height)) {
					gui_delete_registered_face(i, data);
					if (!data->preview_enabled) {
						data->new_inference = true;
					}
					goto end;
				}
			}
		}

		/* event occurs on one of the live face bounding box that has/
		 * not been registered */
		for (unsigned int i = 0 ; i < data->screen_face_position.size() ; i++) {
			if (!data->screen_face_position[i].registered &&
			    (event->x - data->offset > data->screen_face_position[i].pos.x) &&
			    (event->x - data->offset < data->screen_face_position[i].pos.x + data->screen_face_position[i].pos.width) &&
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
 * This function is an helper to get thumbnail position and size depending on
 * the preview area.
 */
static void gui_compute_history_thumbnail_position(CustomData *data)
{
	if(data->preview_enabled){
		float ratio = (float)data->frame_width/(float)data->frame_height;
		float width_preview = (ratio*(float)(data->widget_draw_height));
		data->face_banner.width = width_preview;
		data->face_banner.height = data->ui_face_thumb_size + data->ui_face_thumb_spacing ;
		data->face_banner.x = 0;
		data->face_banner.y = data->widget_draw_height - data->face_banner.height;
	} else {
		data->face_banner.width = data->frame_disp_pos.width;
		data->face_banner.height = data->ui_face_thumb_size + data->ui_face_thumb_spacing;
		data->face_banner.x = 0;
		data->face_banner.y = data->frame_disp_pos.height - data->face_banner.height;
	}
	data->nb_history_thumbnails = (data->widget_draw_width - data->ui_face_thumb_spacing)
		/ (data->ui_face_thumb_size + data->ui_face_thumb_spacing);

	/* Clip to the MAX_HISTORY_THUMBNAILS value if needed */
	data->nb_history_thumbnails = std::min(data->nb_history_thumbnails,
					       MAX_HISTORY_THUMBNAILS);

	/* Depending on the number of face registered, the placement of the
	 * history thumbnail at the bottom of the preview differs to have them
	 * always centered. The following part of code precalculate the position
	 * of history the thumbnails. */
	for (int i = 0 ; i < data->nb_history_thumbnails ; i++) {
		int starting_offset = (data->face_banner.width -
				       ((data->ui_face_thumb_size + data->ui_face_thumb_spacing) * (i + 1)) +
				       data->ui_face_thumb_spacing) / 2;
		for (int j = 0 ; j <= i ; j++) {
			Position new_position;
			new_position.x = starting_offset +
				(j * (data->ui_face_thumb_size +
				      data->ui_face_thumb_spacing));
			new_position.y = data->face_banner.y + data->ui_face_thumb_spacing/2;
			new_position.width = data->ui_face_thumb_size;
			new_position.height = data->ui_face_thumb_size;

			data->history_thumb_position[i].push_back(new_position);
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
		cairo_set_line_width(cr, data->ui_box_line_width);
		new_screen_face_position.registered = false;

		if (data->preview_enabled){
			/* Get drawing area informations */
			float ratio = (float)data->frame_width/(float)data->frame_height;
			float width_preview = (ratio*(float)(data->widget_draw_height));
			/* compute rectangle position and dimensions */
			/*  Offset to draw on the preview */
			data->offset = ((data->widget_draw_ov_width - (int)width_preview)/2);
			new_screen_face_position.pos.x = width_preview  *
				data->detected_faces[i].bbox.top_left.x + data->offset;
			new_screen_face_position.pos.y = data->frame_disp_pos.height *
				data->detected_faces[i].bbox.top_left.y;
			new_screen_face_position.pos.width  = width_preview  *
				(data->detected_faces[i].bbox.bot_right.x -
				 data->detected_faces[i].bbox.top_left.x);
			new_screen_face_position.pos.height = data->frame_disp_pos.height *
				(data->detected_faces[i].bbox.bot_right.y -
				 data->detected_faces[i].bbox.top_left.y);
		} else {
			/* compute rectangle position and dimensions */
			new_screen_face_position.pos.x = data->frame_disp_pos.width  *
				data->detected_faces[i].bbox.top_left.x;
			new_screen_face_position.pos.y = data->frame_disp_pos.height *
				data->detected_faces[i].bbox.top_left.y;
			new_screen_face_position.pos.width = data->frame_disp_pos.width  *
				(data->detected_faces[i].bbox.bot_right.x -
				 data->detected_faces[i].bbox.top_left.x);
			new_screen_face_position.pos.height = data->frame_disp_pos.height *
				(data->detected_faces[i].bbox.bot_right.y -
				 data->detected_faces[i].bbox.top_left.y);
		}
		cairo_rectangle(cr,
				int(new_screen_face_position.pos.x),
				int(new_screen_face_position.pos.y),
				int(new_screen_face_position.pos.width),
				int(new_screen_face_position.pos.height));
		cairo_stroke(cr);
		std::stringstream info_sstr;
		info_sstr << data->detected_faces[i].label;

		/* if the face label is different from "unknown" then change
		 * the color to green */
		if (data->detected_faces[i].label.compare("unknown") != 0) {
			cairo_set_source_rgb (cr, 0.0, 1.0, 0.0);
			new_screen_face_position.registered = true;
		}

		data->screen_face_position.push_back(new_screen_face_position);

		/* display rectangle over the detected face */
		cairo_rectangle(cr,
				int(new_screen_face_position.pos.x),
				int(new_screen_face_position.pos.y),
				int(new_screen_face_position.pos.width),
				int(new_screen_face_position.pos.height));
		cairo_stroke(cr);

		/* display the label and the similarity */
		cairo_save(cr);
		cairo_move_to(cr, int(new_screen_face_position.pos.x) + 2,
			      int(new_screen_face_position.pos.y) - (data->ui_cairo_font_size / 2));
		cairo_show_text(cr, info_sstr.str().c_str());
		cairo_restore(cr);
	}

end:
	mtx.unlock();
}

/**
 * This function is an helper to set UI variable according to the display size
 */
static void gui_set_ui_parameters(CustomData *data)
{

	int window_constraint = 0;
	if (data->window_height > data->window_width)
		window_constraint = data->window_width;
	else
		window_constraint = data->window_height;

	if (window_constraint <= 272) {
		/* Display 480x272 */
		g_print("Display config <= 272p \n");
		data->ui_cairo_font_size_label = 15;
		data->ui_cairo_font_size = 10;
		data->ui_icon_exit_size = 25;
		data->ui_icon_st_size = 52;
		data->ui_face_thumb_size = 32;
		data->ui_face_thumb_spacing = 3;
		data->ui_box_line_width = 2.0;
		data->ui_thumb_box_line_width = 2.0;
		data->keyboard_config = "window_272p";
	} else if (window_constraint <= 480) {
		/* Display 800x480 */
		g_print("Display config <= 480p \n");
		data->ui_cairo_font_size_label = 26;
		data->ui_cairo_font_size = 16;
		data->ui_icon_exit_size = 50;
		data->ui_icon_st_size = 80;
		data->ui_face_thumb_size = 32;
		data->ui_face_thumb_spacing = 3;
		data->ui_box_line_width = 2.0;
		data->ui_thumb_box_line_width = 2.0;
		data->keyboard_config = "window_272p";
	} else if (window_constraint <= 600) {
		/* Display 1024x600 */
		g_print("Display config <= 600p \n");
		data->ui_cairo_font_size = 18;
		data->ui_cairo_font_size_label = 30;
		data->ui_icon_exit_size = 50;
		data->ui_icon_st_size = 120;
		data->ui_face_thumb_size = 64;
		data->ui_face_thumb_spacing = 10;
		data->ui_box_line_width = 2.0;
		data->ui_thumb_box_line_width = 3.0;
		data->keyboard_config = "window_600p";
	} else if (window_constraint <= 720) {
		/* Display 1280x720 */
		g_print("Display config <= 720p \n");
		data->ui_cairo_font_size = 20;
		data->ui_cairo_font_size_label = 35;
		data->ui_icon_exit_size = 50;
		data->ui_icon_st_size = 160;
		data->ui_face_thumb_size = 96;
		data->ui_face_thumb_spacing = 20;
		data->ui_box_line_width = 2.0;
		data->ui_thumb_box_line_width = 3.0;
		data->keyboard_config = "window_720p";
	} else if (window_constraint <= 1080) {
		/* Display 1920x1080 */
		g_print("Display config <= 1080p \n");
		data->ui_cairo_font_size_label = 45;
		data->ui_cairo_font_size = 30;
		data->ui_icon_exit_size = 50;
		data->ui_icon_st_size = 160;
		data->ui_face_thumb_size = 128;
		data->ui_face_thumb_spacing = 20;
		data->ui_box_line_width = 2.0;
		data->ui_thumb_box_line_width = 3.0;
		data->keyboard_config = "window_1080p";
	} else {
		/* Default UI parameter */
		g_print("Display config fallback \n");
		data->ui_icon_exit_size = 50;
		data->ui_icon_st_size = 160;
		data->ui_cairo_font_size = 20;
		data->ui_cairo_font_size_label = 35;
		data->ui_face_thumb_size = 96;
		data->ui_face_thumb_spacing = 10;
		data->ui_box_line_width = 2.0;
		data->ui_thumb_box_line_width = 4.0;
		data->keyboard_config = "window_720p";
	}
}

/**
 * This function is an helper to set UI icons sizes according to the display size
 */
static void gui_set_icons_parameters(CustomData *data)
{
	/* set the icons */
	data->st_icon_sstr << RESOURCES_DIRECTORY << "FR_st_icon_";

	if (!data->preview_enabled)
		data->st_icon_sstr << "next_inference_";
	data->st_icon_sstr << data->ui_icon_st_size << "px.png";

	/* checking the reading permission of the file */
	int ret = access(data->st_icon_sstr.str().c_str(),R_OK);
	if (ret != 0){
		g_printerr("Read permission denied to the file %s\n",data->st_icon_sstr.str().c_str());
		exit(1);
	}

	data->exit_icon_sstr << RESOURCES_DIRECTORY << "exit_";
	data->exit_icon_sstr << data->ui_icon_exit_size << "x" << data->ui_icon_exit_size << ".png";

	/* checking the reading permission of the file */
	ret = access(data->exit_icon_sstr.str().c_str(),R_OK);
	if (ret != 0){
		g_printerr("Read permission denied to the file %s\n",data->exit_icon_sstr.str().c_str());
		exit(1);
	}
}

/**
 * This function is called each time the GTK overlay UI is redrawn
 */
bool first_call_overlay = true;
static gboolean gui_draw_overlay_cb(GtkWidget *widget,
				    cairo_t *cr,
				    CustomData *data)
{
	struct timeval start, stop;
	gettimeofday(&start, nullptr);
	/* Get drawing area informations */
	data->widget_draw_ov_width = gtk_widget_get_allocated_width(widget);
	data->widget_draw_ov_height = gtk_widget_get_allocated_height(widget);
	float ratio = (float)data->frame_width/(float)data->frame_height;
	float width_preview = data->widget_draw_ov_height*ratio;

	/* Updating the information with the new inference results */
	std::stringstream display_fps_sstr;
	std::stringstream inference_time_sstr;
	std::stringstream inference_time_fr_sstr;
	std::stringstream inference_fps_sstr;
	float inf_time = 0;
	float inf_time_fr = 0;

	if (results.inference_time != 0)
	{
		inf_time = results.inference_time;
		inf_time_fr = data->total_face_reco_inference_time;
		float total_inf = inf_time + inf_time_fr;

		display_fps_sstr   << std::right << std::setw(5) << std::fixed << std::setprecision(1) << display_avg_fps;
		display_fps_sstr   << std::right << std::setw(3) << " fps ";

		inference_time_sstr << std::right << std::setw(5) << std::fixed << std::setprecision(1) << inf_time << std::right << std::setw(2) << " ms ";
		inference_time_fr_sstr << std::right << std::setw(5) << std::fixed << std::setprecision(1) << inf_time_fr << std::right << std::setw(2) << " ms ";

		inference_fps_sstr << std::right << std::setw(5) << std::fixed << std::setprecision(1) << (1000 / total_inf);
		inference_fps_sstr << std::right << std::setw(3) << " fps ";
	}


	/* set the font and the size for the label*/
	cairo_select_font_face (cr, "monospace",
				CAIRO_FONT_SLANT_NORMAL,
				CAIRO_FONT_WEIGHT_BOLD);
	cairo_set_font_size (cr, data->ui_cairo_font_size);

	/*  Update the gtk labels with latest information */
	if (data->preview_enabled) {
		/* Camera preview use case */
		if (first_call_overlay){
			first_call_overlay = false;
			compute_frame_position(data);
			initialize_face_database(data);
			return FALSE;
		} else {
			if(!database_init){
				gui_compute_history_thumbnail_position(data);
				database_init = true;
			}
			/*  Update labels */
			std::stringstream info_sstr;
			info_sstr << "  disp.fps :     " << "\n" << display_fps_sstr.str().c_str() << "\n" << "  inf.fps :     " << "\n" << inference_fps_sstr.str().c_str() << "\n" << "  inf.time :     " << "\n" << inference_time_sstr.str().c_str() << "\n" << "+" << "\n" << inference_time_fr_sstr.str().c_str();
			char *label_to_display = g_strdup_printf ("<span line_height=\"1\"  font=\"%d\" color=\"#FFFFFFFF\">""<b>%s\n</b>""</span>",data->ui_cairo_font_size,info_sstr.str().c_str());
			gtk_label_set_markup(GTK_LABEL(data->info_inf_time),label_to_display);
			g_free(label_to_display);
		}
		if(exit_application)
			gtk_main_quit();
	} else {
		/* Still picture use case */
		if (first_call_overlay){
			first_call_overlay = false;
			compute_frame_position(data);
			initialize_face_database(data);
			gui_compute_history_thumbnail_position(data);
			/* add the inference function in idle after the initialization of
			 * the GUI */
			g_idle_add((GSourceFunc)infer_new_picture,data);
			return FALSE;
		}
	}

	/* Determine the drawing offset to scale NN results on the preview displayed */
	if (!data->preview_enabled) {
		/* Translate to the frame position */
        data->offset = (data->widget_draw_width - data->frame_disp_pos.width)/2;
		cairo_translate(cr,
				data->offset,
				0);
	} else {
		data->offset = ((data->widget_draw_ov_width - (int)width_preview)/2);
	}

	/* Draw bounding box of the 5 first detected faces */
	gui_draw_face_positions(cr, data);

	/* Draw a black transparent banner to display the registered faces */
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.60);
	if (!data->preview_enabled){
		cairo_rectangle(cr,data->face_banner.x, data->face_banner.y,data->face_banner.width, data->face_banner.height);
	} else {
		cairo_rectangle(cr,data->face_banner.x + data->offset, data->face_banner.y,data->face_banner.width, data->face_banner.height);
	}
	cairo_fill_preserve(cr);
	cairo_stroke(cr);

	/* Display the history thumbnail of the registered face and draw a
	 * green rectangle around the face that is currently recognized */
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
				double line_width = data->ui_thumb_box_line_width;
				cairo_set_line_width(cr, line_width);
				/* Display a green rectangle around the faces
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

	/*  Validation mode  */
	if (validation) {
		if (data->preview_enabled) {
			data->valid_fd_inference_time.push_back(inf_time);
			data->valid_fr_inference_time.push_back(inf_time_fr);

			/* Reload the timeout */
			g_source_remove(data->valid_timeout_id);
			data->valid_timeout_id = g_timeout_add(5000,
							       valid_timeout_callback,
							       NULL);

			data->valid_draw_count++;

			if (data->valid_draw_count > data->val_run) {
				auto avg_fd_inf_time = std::accumulate(data->valid_fd_inference_time.begin(),
								       data->valid_fd_inference_time.end(), 0.0) /
								       data->valid_fd_inference_time.size();
				auto avg_fr_inf_time = std::accumulate(data->valid_fr_inference_time.begin(),
								       data->valid_fr_inference_time.end(), 0.0) /
								       data->valid_fr_inference_time.size();
				/* Stop the timeout and properly exit the
				 * application */
				std::cout << "avg display fps= " << display_avg_fps << std::endl;
				std::cout << "avg inference fps= " << (1000 / (avg_fd_inf_time+avg_fr_inf_time)) << std::endl;
				std::cout << "avg fd inference time= " << avg_fd_inf_time << std::endl;
				std::cout << "avg fr inference time= " << avg_fr_inf_time << std::endl;
				g_source_remove(data->valid_timeout_id);
				gtk_main_quit();
			}
		}
	}
	return TRUE;
}

/**
 * This function is creating GTK widget to create the overlay window to display UI
 * information
 */
static void gui_create_overlay(CustomData *data)
{
	/*Gtk Widget used for the window */
	GtkWidget *main_box;
	GtkWidget *drawing_box;
	GtkWidget *drawing_area;
	GtkWidget *still_pict_draw;
	GtkWidget *info_box;
	GtkWidget *exit_box;
	GtkWidget *st_icon_event_box;
	GtkWidget *exit_icon_event_box;

	/* Create the window */
	data->window_overlay = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_widget_set_app_paintable(data->window_overlay, TRUE);
	g_signal_connect(G_OBJECT(data->window_overlay), "delete-event",
			 G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(data->window_overlay), "destroy",
			 G_CALLBACK(gtk_main_quit), NULL);

	/* Remove title bar */
	gtk_window_set_decorated(GTK_WINDOW(data->window_overlay), FALSE);

	/* Maximize the window size*/
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

	gtk_image_set_from_file(GTK_IMAGE(data->st_icon_ov), data->st_icon_sstr.str().c_str());
	gtk_image_set_from_file(GTK_IMAGE(data->exit_icon_ov), data->exit_icon_sstr.str().c_str());

	if (data->preview_enabled) {
		/*  Camera preview use case */
		/* Create the drawing area to draw text on it using cairo */
		drawing_area = gtk_drawing_area_new();
		gtk_widget_set_app_paintable(drawing_area, TRUE);
		gtk_widget_add_events(drawing_area, GDK_BUTTON_PRESS_MASK);
		g_signal_connect(G_OBJECT(drawing_area), "draw",G_CALLBACK(gui_draw_overlay_cb), data);
		g_signal_connect(G_OBJECT(drawing_area), "button-press-event",G_CALLBACK(gui_press_event_cb), data);

		data->info_inf_time = gtk_label_new(NULL);
		gtk_label_set_justify(GTK_LABEL(data->info_inf_time),GTK_JUSTIFY_CENTER);
		/*  initialize labels */
		std::stringstream info_sstr;
		info_sstr << "  disp.fps :     " << "\n" << "  inf.fps :     " << "\n" << "  inf.time :     " << "\n";
		char *label_to_display = g_strdup_printf ("<span line_height=\"1\"  font=\"%d\" color=\"#000000\">""<b>%s\n</b>""</span>",data->ui_cairo_font_size,info_sstr.str().c_str());
		gtk_label_set_markup(GTK_LABEL(data->info_inf_time),label_to_display);
		g_free(label_to_display);

	} else {
		/*  Still picture use case */
		/* Create the video widget */
		still_pict_draw = gtk_drawing_area_new();
		gtk_widget_set_app_paintable(still_pict_draw, TRUE);
		gtk_widget_add_events(still_pict_draw, GDK_BUTTON_PRESS_MASK);
		g_signal_connect(still_pict_draw, "draw",G_CALLBACK(gui_draw_overlay_cb), data);
		g_signal_connect(G_OBJECT(still_pict_draw), "button-press-event",G_CALLBACK(gui_press_event_cb), data);
		data->info_inf_time = gtk_label_new(NULL);
		gtk_label_set_justify(GTK_LABEL(data->info_inf_time),GTK_JUSTIFY_CENTER);
		/*  initialize labels */
		std::stringstream info_sstr;
		info_sstr << "  inf.fps :     "  << "\n" << "  inf.time :     " << "\n";
		char *label_to_display = g_strdup_printf ("<span line_height=\"1\"  font=\"%d\" color=\"#000000\">""<b>%s\n</b>""</span>",data->ui_cairo_font_size,info_sstr.str().c_str());
		gtk_label_set_markup(GTK_LABEL(data->info_inf_time),label_to_display);
		g_free(label_to_display);

	}

	/* Set the boxes */
	info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_name(info_box , "gui_overlay_stbox");
	if (data->preview_enabled){
		/* Camera preview use case */
		gtk_box_pack_start(GTK_BOX(info_box), st_icon_event_box, FALSE, FALSE, 4);
		gtk_box_pack_start(GTK_BOX(info_box), data->info_inf_time, FALSE, FALSE, 4);

	} else {
		/*  Still picture use case */
		gtk_box_pack_start(GTK_BOX(info_box), st_icon_event_box, FALSE, FALSE, 4);
		gtk_box_pack_start(GTK_BOX(info_box), data->info_inf_time, FALSE, FALSE, 4);

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

	exit_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_name(exit_box , "gui_overlay_exit");
	gtk_box_pack_start(GTK_BOX(exit_box), exit_icon_event_box, FALSE, FALSE, 2);

	main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_name(main_box, "gui_overlay");
	gtk_box_pack_start(GTK_BOX(main_box), info_box, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(main_box), drawing_box, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(main_box), exit_box , FALSE, FALSE, 2);

	/* Push the UI into the window_overlay */
	gtk_container_add(GTK_CONTAINER(data->window_overlay), main_box);
	gtk_widget_show_all(data->window_overlay);
}

/**
 * This function is called each time the GTK overlay UI is redrawn
 */
static gboolean gui_draw_main_cb(GtkWidget *widget,
				 cairo_t *cr,
				 CustomData *data)
{
	data->offset = 0;
	/* Get display information and set UI variable accordingly */
	if (data->preview_enabled) {
		/* Camera preview use case */
		data->widget_draw_width = gtk_widget_get_allocated_width(widget);
		data->widget_draw_height= gtk_widget_get_allocated_height(widget);
	} else {
		/* Still picture use case */
		data->widget_draw_width = gtk_widget_get_allocated_width(widget);
		data->widget_draw_height = gtk_widget_get_allocated_height(widget);
		data->offset = (data->widget_draw_width - data->frame_disp_pos.width)/2;

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
			cairo_set_source_surface(cr,picture,data->offset,0);
			cairo_paint(cr);
			cairo_surface_destroy(picture);
		} else {
			/* Set the black background */
			cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);
			cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
			cairo_paint(cr);
			data->new_inference = true;
		}
	}
	return FALSE;
}

/**
 * This function is creating GTK widget to create the main window used for camera preview or
 * picture preview
 */
static void gui_create_main(CustomData *data)
{
	/*Gtk Widget used for the window */
	GtkWidget *main_box;
	GtkWidget *video_box;
	GtkWidget *still_pict_draw;
	GtkWidget *overlay;
	GtkWidget *info_box;
	GtkWidget *exit_box;
	GtkWidget *st_icon_event_box;
	GtkWidget *exit_icon_event_box;

	/* Create the window */
	data->window_main = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	/* Remove title bar */
	gtk_window_set_decorated(GTK_WINDOW(data->window_main), FALSE);

	/* Maximize the window size */
	gtk_window_maximize(GTK_WINDOW(data->window_main));

	gtk_widget_set_app_paintable(data->window_main, TRUE);
	g_signal_connect(G_OBJECT(data->window_main), "delete-event",G_CALLBACK(gtk_main_quit), NULL);
	g_signal_connect(G_OBJECT(data->window_main), "destroy",G_CALLBACK(gtk_main_quit), NULL);

	/* Create the ST icon widget */
	data->st_icon_main = gtk_image_new();
	st_icon_event_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(st_icon_event_box), data->st_icon_main);
	g_signal_connect(G_OBJECT(st_icon_event_box),"button_press_event",G_CALLBACK(st_icon_cb), data);

	/* Create the exit icon widget */
	data->exit_icon_main = gtk_image_new();
	exit_icon_event_box = gtk_event_box_new();
	gtk_container_add(GTK_CONTAINER(exit_icon_event_box), data->exit_icon_main);
	g_signal_connect(G_OBJECT(exit_icon_event_box),"button_press_event",G_CALLBACK(exit_icon_cb), data);

	gtk_image_set_from_file(GTK_IMAGE(data->st_icon_main), data->st_icon_sstr.str().c_str());
	gtk_image_set_from_file(GTK_IMAGE(data->exit_icon_main), data->exit_icon_sstr.str().c_str());

	data->overlay_draw = gtk_drawing_area_new();
	gtk_widget_set_app_paintable(data->overlay_draw, TRUE);
	if (data->preview_enabled) {
		g_signal_connect(data->overlay_draw, "draw",G_CALLBACK(gui_draw_main_cb), data);
	}
	if (data->preview_enabled) {
		/* Camera preview use case */
		/* Create the video widget */
		GstElement *sink = gst_bin_get_by_name (GST_BIN (data->pipeline), "gtkwsink");
		if (!sink && !g_strcmp0 (G_OBJECT_TYPE_NAME (data->pipeline), "GstPlayBin")) {
			g_object_get (data->pipeline, "video-sink", &sink, NULL);
			if (sink && g_strcmp0 (G_OBJECT_TYPE_NAME (sink), "GstGtkWaylandSink") != 0
			    && GST_IS_BIN (sink)) {
				GstBin *sinkbin = GST_BIN (sink);
				sink = gst_bin_get_by_name (sinkbin, "gtkwsink");
				gst_object_unref (sinkbin);
			}
		}
		g_assert (sink);
		g_assert (!g_strcmp0 (G_OBJECT_TYPE_NAME (sink), "GstGtkWaylandSink"));
		g_object_get (sink, "widget", &data->video, NULL);
        gtk_widget_set_app_paintable(GTK_WIDGET(data->video), TRUE);

		data->info_inf_time_main = gtk_label_new(NULL);
		gtk_label_set_justify(GTK_LABEL(data->info_inf_time_main),GTK_JUSTIFY_CENTER);
		/*  initialize labels */
		std::stringstream info_sstr;
		info_sstr << "  disp.fps :     " << "\n" << "  inf.fps :     " << "\n" << "  inf.time :     " << "\n";
		char *label_to_display = g_strdup_printf ("<span line_height=\"1\"  font=\"%d\" color=\"#000000\">""<b>%s\n</b>""</span>",data->ui_cairo_font_size,info_sstr.str().c_str());
		gtk_label_set_markup(GTK_LABEL(data->info_inf_time_main),label_to_display);
		g_free(label_to_display);

	} else {
		/*  Still picture preview use case */
		/* Create the drawing widget */
		still_pict_draw = gtk_drawing_area_new();
		gtk_widget_set_app_paintable(still_pict_draw, TRUE);
		g_signal_connect(still_pict_draw, "draw",
				 G_CALLBACK(gui_draw_main_cb), data);

		data->info_inf_time_main = gtk_label_new(NULL);
		gtk_label_set_justify(GTK_LABEL(data->info_inf_time_main),GTK_JUSTIFY_CENTER);
		/*  initialize labels */
		std::stringstream info_sstr;
		info_sstr << "  inf.fps :     "  << "\n" << "  inf.time :     " << "\n" ;
		char *label_to_display = g_strdup_printf ("<span line_height=\"1\"  font=\"%d\" color=\"#000000\">""<b>%s\n</b>""</span>",data->ui_cairo_font_size,info_sstr.str().c_str());
		gtk_label_set_markup(GTK_LABEL(data->info_inf_time_main),label_to_display);
		g_free(label_to_display);
	}

	/* Set the boxes */
	info_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_name(info_box, "gui_main_stbox");
	if (data->preview_enabled){
		/*  Camera preview use case */
		gtk_box_pack_start(GTK_BOX(info_box), st_icon_event_box, FALSE, FALSE, 4);
		gtk_box_pack_start(GTK_BOX(info_box), data->info_inf_time_main, TRUE, FALSE, 4);
	} else {
		/*  Still picture use case */
		gtk_box_pack_start(GTK_BOX(info_box), st_icon_event_box, FALSE, FALSE, 4);
		gtk_box_pack_start(GTK_BOX(info_box), data->info_inf_time_main, TRUE, FALSE, 4);
	}
	video_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_app_paintable(GTK_WIDGET(video_box), TRUE);
	gtk_widget_set_name(video_box, "gui_main_video");
	if (data->preview_enabled){
		/*  Camera preview use case  */
		gtk_box_pack_start(GTK_BOX(video_box), GTK_WIDGET(data->video), TRUE, TRUE, 0);
	} else {
		/*  Still picture use case */
		gtk_box_pack_start(GTK_BOX(video_box), still_pict_draw, TRUE, TRUE, 0);
	}
	exit_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_set_name(exit_box, "gui_main_exit");
	gtk_box_pack_start(GTK_BOX(exit_box), exit_icon_event_box, FALSE, FALSE, 2);

	main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_set_app_paintable(GTK_WIDGET(main_box), TRUE);
	gtk_widget_set_name(main_box, "gui_main");
	gtk_box_pack_start(GTK_BOX(main_box), info_box, FALSE, FALSE, 2);
	gtk_box_pack_start(GTK_BOX(main_box), video_box, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(main_box), exit_box, FALSE, FALSE, 2);

	overlay = gtk_overlay_new();
	gtk_container_add(GTK_CONTAINER(overlay),main_box);
	gtk_overlay_add_overlay(GTK_OVERLAY(overlay), data->overlay_draw);
	gtk_container_add(GTK_CONTAINER(data->window_main),overlay);
 	gtk_widget_show_all(data->window_main);
}

/**
 * This function is creating GTK widget to create the main window to display UI
 * information
 */
static void gui_create_keyboard(CustomData *data)
{
	/* manage a virtual keyboard */
	data->keyboard = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	g_signal_connect(data->keyboard, "destroy",G_CALLBACK(gtk_main_quit), NULL);
	/* Remove title bar */
	gtk_window_set_decorated(GTK_WINDOW(data->keyboard), FALSE);
	/* Tell GTK that we want to draw the background ourself */
	gtk_widget_set_app_paintable(data->keyboard, TRUE);
	gtk_window_maximize(GTK_WINDOW(data->keyboard));

	data->keyboard_fixed = gtk_fixed_new();
	gtk_container_add(GTK_CONTAINER(data->keyboard), data->keyboard_fixed);

	data->keyboard_entry = gtk_entry_new();
	gtk_widget_set_name(data->keyboard_entry , data->keyboard_config.c_str());

	data->keyboard_label = gtk_label_new(NULL);
	g_object_set(data->keyboard_entry, "margin", 5, NULL);

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
			gtk_widget_set_name(new_key.button , data->keyboard_config.c_str());

			if (_keys[index].compare("return") == 0) {
				gtk_grid_attach(GTK_GRID(grid1),
						new_key.button,
						j, i, 1, 3);
			} else {
				gtk_grid_attach(GTK_GRID(grid1),
						new_key.button,
						j, i, 1, 1);
			}

			g_signal_connect(new_key.button,"clicked",G_CALLBACK(gui_keyboard_clicked),data);

			data->keys.push_back(new_key);
			index++;
		}
	}

	GtkWidget *grid2=gtk_grid_new();
	gtk_grid_attach(GTK_GRID(grid2), grid1, 0, 0, 1, 1);
	gtk_grid_attach(GTK_GRID(grid2), data->keyboard_label, 0, 1, 1, 1);
	gtk_grid_attach(GTK_GRID(grid2), data->keyboard_entry, 0, 2, 1, 1);

	gtk_fixed_put(GTK_FIXED(data->keyboard_fixed), grid2, 200, 200);

	g_signal_connect(grid2, "size-allocate",G_CALLBACK(gui_get_keyboard_size), data);
}

/**
 * This function is called when appsink Gstreamer element receives a buffer
 */
static GstFlowReturn gst_new_sample_cb(GstElement *sink, CustomData *data)
{
	/* Retrieve the buffer */
	if (face_recognition_done) {
		GstSample *sample;
		GstBuffer *app_buffer, *buffer;
		GstMapInfo info;
		g_signal_emit_by_name (sink, "pull-sample", &sample);
		if (sample) {
			if (data->cpt_frame == 0)
				update_isp_config(data);

			data->cpt_frame += 1;

			if (data->cpt_frame == 300)
				data->cpt_frame = 0;

			buffer = gst_sample_get_buffer (sample);

			/* Make a copy */
			app_buffer = gst_buffer_ref (buffer);

			gst_buffer_map(app_buffer, &info, GST_MAP_READ);

			#ifdef DEBUG
				FILE *file = fopen("NN_sample_dump.raw", "wb");
				if (file != NULL) {
					fwrite(info.data, info.size, 1, file);
					fclose(file);
					int ret = GST_FLOW_OK;
				}
			#endif

			/* Execute the inference */
			mtx.lock();
			nn_inference(info.data);
			nn_postprocessing();
			data->detected_faces.clear();
			if (reco_simultaneous_face){
				for (uint32_t i = 0 ; i < results.detected_faces.size() ; i ++) {
					DetectedFace new_face;
					Bbox bbox;
					bbox.top_left.x = results.detected_faces[i].landmarks.face.x0;
					bbox.top_left.y = results.detected_faces[i].landmarks.face.y0;
					bbox.bot_right.x = results.detected_faces[i].landmarks.face.x1;
					bbox.bot_right.y = results.detected_faces[i].landmarks.face.y1;
					new_face.label = "unknown";
					new_face.bbox = bbox;
					data->detected_faces.push_back(new_face);
				}
			} else {
				if (!results.detected_faces.empty()){
					DetectedFace new_face;
					Bbox bbox;
					bbox.top_left.x = results.detected_faces[0].landmarks.face.x0;
					bbox.top_left.y = results.detected_faces[0].landmarks.face.y0;
					bbox.bot_right.x = results.detected_faces[0].landmarks.face.x1;
					bbox.bot_right.y = results.detected_faces[0].landmarks.face.y1;
					new_face.label = "unknown";
					new_face.bbox = bbox;
					data->detected_faces.push_back(new_face);
				}
			}
			face_recognition_done = false;
			gst_buffer_unmap(app_buffer, &info);
			gst_buffer_unref(app_buffer);

			/* We don't need the appsink sample anymore */
			gst_sample_unref (sample);
			return GST_FLOW_OK;
		} else {
			return GST_FLOW_ERROR;
		}
	} else {
		return GST_FLOW_OK;
	}
}

/**
 * This function is called when appsink Gstreamer element receives a buffer
 */
static GstFlowReturn gst_new_sample_fr_cb(GstElement *sink, CustomData *data)
{
	if(!face_recognition_done) {
		GstSample *sample;
		GstBuffer *app_buffer, *buffer;
		GstMapInfo info;
		/* Retrieve the buffer */
		g_signal_emit_by_name (sink, "pull-sample", &sample);
		if (sample) {
			if (data->cpt_frame == 0)
				update_isp_config(data);

			data->cpt_frame += 1;

			if (data->cpt_frame == 1000)
				data->cpt_frame = 0;

			buffer = gst_sample_get_buffer (sample);

			/* Make a copy */
			app_buffer = gst_buffer_ref (buffer);

			gst_buffer_map(app_buffer, &info, GST_MAP_READ);

			#ifdef DEBUG
				FILE *file = fopen("NN_sample_dump_fr.raw", "wb");
				if (file != NULL) {
					fwrite(info.data, info.size, 1, file);
					fclose(file);
					int ret = GST_FLOW_OK;
				}
			#endif

			int width = data->window_width;
			int height = data->window_height;
			int channels = 3;
			cv::Mat frame(height, width, CV_8UC3, info.data);
			cv::Mat cropped_frame;

			data->total_face_reco_inference_time = 0;
			for (uint32_t i = 0 ; i < data->detected_faces.size() ; i++) {
				float face_x0 = width  * data->detected_faces[i].bbox.top_left.x;
				float face_y0 = height * data->detected_faces[i].bbox.top_left.y;
				float face_x1 = width  * data->detected_faces[i].bbox.bot_right.x;
				float face_y1 = height * data->detected_faces[i].bbox.bot_right.y;
				float width_box = width  * (data->detected_faces[i].bbox.bot_right.x - data->detected_faces[i].bbox.top_left.x);
				float height_box =  height *(data->detected_faces[i].bbox.bot_right.y - data->detected_faces[i].bbox.top_left.y);
				cv::Rect crop_region(face_x0, face_y0, width_box, height_box);
				cropped_frame = frame(crop_region);
				cv::resize(cropped_frame,cropped_frame,cv::Size(160,160));
				data->detected_faces[i].face_rgb = cropped_frame.clone();
				nn_fr_inference(data->detected_faces[i].face_rgb.data);
				nn_fr_postprocessing();
				data->total_face_reco_inference_time += results_fr.inference_time;
				std::copy(std::begin(results_fr.nn_output), std::end(results_fr.nn_output), std::begin(data->detected_faces[i].identity));
				for (unsigned int j = 0 ; j < data->registered_faces.size() ; j++) {
					float similarity = nn_postproc_fr::cosine_similarity(data->detected_faces[i].identity, data->registered_faces[j].identity, FACE_IDENTITY_CLASSES);
					if(similarity <= reco_threshold){
						data->detected_faces[i].label =  data->registered_faces[j].label;
						data->detected_faces[i].similarity = similarity;
						break;
					} else {
						data->detected_faces[i].label = "unknown";
						data->detected_faces[i].similarity = similarity;
					}
				}
			}
			face_recognition_done = true;
			mtx.unlock();
			gst_buffer_unmap(app_buffer, &info);
			gst_buffer_unref(app_buffer);
			/* We don't need the appsink sample anymore */
			gst_sample_unref (sample);
			/* Call application callback only in playing state */
			gst_element_post_message(sink,
						gst_message_new_application(GST_OBJECT(sink),
						gst_structure_new_empty("inference-done")));
			return GST_FLOW_OK;
		} else {
			return GST_FLOW_ERROR;
		}
	} else {
		return GST_FLOW_OK;
	}
}


/**
 * This function is called by Gstreamer fpsdisplaysink to get fps measurement
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
 * This function is called when a Gstreamer error or EOS message are posted on the bus.
 */
static void gst_bus_error_cb (GstBus *bus, GstMessage *msg, CustomData *data)
{
	switch (GST_MESSAGE_TYPE(msg)) {
		case GST_MESSAGE_EOS: {
			gst_element_set_state(data->pipeline, GST_STATE_READY);
			g_print("End-Of-Stream \n");
            break;
		}
		case GST_MESSAGE_ERROR: {
			gchar *debug = NULL;
			GError *error = NULL;
			gst_message_parse_error (msg, &error, &debug);
            g_free(debug);
            g_printerr("Error: %s\n", error->message);
            g_error_free(error);
			gst_element_set_state(data->pipeline, GST_STATE_READY);
            break;
		}
        default:
            break;
    }
}

static void gst_state_changed_cb (GstBus *bus, GstMessage *msg, CustomData *data) {
	GstState oldState;
	GstState newState;
	gst_message_parse_state_changed(msg, &oldState, &newState, 0);
	if (newState == GST_STATE_PLAYING && oldState == GST_STATE_PAUSED) {
		gst_debug_bin_to_dot_file(GST_BIN(data->pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "pipeline_cpp_PAUSED_PLAYING");
	}
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
 * Construct the Gstreamer pipeline used to stream camera frame
 */
static int gst_pipeline_preview_creation(CustomData *data)
{
	GstStateChangeReturn ret;
	GstElement *pipeline, *source, *dispsink, *tee, *scale, *framerate;
	GstElement *queue1, *queue2, *convert, *convert2, *appsink;
	GstElement *fpsmeasure1;
	GstBus *bus;

	/* Create the pipeline */
	pipeline  = gst_pipeline_new("camera preview pipeline");
	data->pipeline = pipeline;

	/* Create gstreamer elements */
	source      = gst_element_factory_make_full("v4l2src","name","camera-source","io-mode",0,NULL);
	tee         = gst_element_factory_make("tee",            "frame-tee");
	queue1      = gst_element_factory_make("queue",          "queue-1");
	queue2      = gst_element_factory_make("queue",          "queue-2");
	convert     = gst_element_factory_make("videoconvert",   "video-convert");
	convert2    = gst_element_factory_make("videoconvert",   "video-convert-wayland");
	scale       = gst_element_factory_make("videoscale",     "videoscale");
	dispsink    = gst_element_factory_make_full("gtkwaylandsink", "name", "gtkwsink", "drm-device",NULL,NULL);
	appsink     = gst_element_factory_make("appsink",        "app-sink");
	framerate   = gst_element_factory_make("videorate",      "video-rate");
	fpsmeasure1 = gst_element_factory_make("fpsdisplaysink", "fps-measure1");

	if (!pipeline || !source || !tee || !queue1 || !queue2 || !convert || !convert2 ||
	    !scale || !dispsink || !appsink || !framerate || !fpsmeasure1 ) {
		g_printerr("One element could not be created. Exiting.\n");
		return -1;
	}

	/* Configure the source element */
	std::stringstream device_sstr;
	device_sstr << "/dev/" << data->camera_info.video_device;
	g_object_set(G_OBJECT(source), "device", device_sstr.str().c_str(), NULL);
	g_print("video source used : %s \n",device_sstr.str().c_str());

	/* Configure the queue elements */
	g_object_set(G_OBJECT(queue1), "max-size-buffers", 1, "leaky", 2 /* downstream */, NULL);
	g_object_set(G_OBJECT(queue2), "max-size-buffers", 1, "leaky", 2 /* downstream */, NULL);

	/* Create caps based on application parameters */
	std::stringstream sourceCaps_sstr;
	sourceCaps_sstr << data->camera_info.camera_caps << ",framerate=" << camera_fps_str << "/1";
	g_print("camera pipeline configuration : %s \n",sourceCaps_sstr.str().c_str());
	GstCaps *sourceCaps = gst_caps_from_string(sourceCaps_sstr.str().c_str());
	std::stringstream scaleCaps_sstr;
	scaleCaps_sstr << "video/x-raw,format=RGB,width=" << data->window_width << ",height=" << data->window_height;
	GstCaps *scaleCaps  = gst_caps_from_string(scaleCaps_sstr.str().c_str());

	/* Configure fspdisplaysink */
	g_object_set(fpsmeasure1, "signal-fps-measurements", TRUE,
		     "fps-update-interval", 2000, "text-overlay", FALSE,
		     "video-sink", dispsink, NULL);
	g_signal_connect(fpsmeasure1, "fps-measurements", G_CALLBACK(gst_fps_measure_display_cb), NULL);

	/* Configure appsink */
	g_object_set(appsink, "emit-signals", TRUE, "sync", FALSE,
		     "max-buffers", 1, "drop", TRUE, "caps", scaleCaps, NULL);
	g_signal_connect(appsink, "new-sample", G_CALLBACK(gst_new_sample_fr_cb), data);

	/* Add all elements into the pipeline */
	gst_bin_add_many(GST_BIN(pipeline),
			 source, framerate, tee, convert, convert2, queue1, queue2, scale, fpsmeasure1, appsink, NULL);

	/* Link the elements together */
	if (!gst_element_link_many(source, framerate, NULL)) {
		g_error("Failed to link elements (1)");
		return -2;
	}
	if (!gst_element_link_filtered(framerate, tee, sourceCaps)) {
		g_error("Failed to link elements (2)");
		return -2;
	}
	if (!gst_element_link_many(tee, queue2, convert, scale, appsink, NULL)) {
		g_error("Failed to link elements (3)");
		return -2;
	}
	if (!gst_element_link_many(tee, queue1, convert2, fpsmeasure1, NULL)) {
		g_error("Failed to link elements (4)");
		return -2;
	}

	gst_caps_unref(sourceCaps);
	gst_caps_unref(scaleCaps);
	/* Instruct the bus to emit signals for each received message, and
	 * connect to the interesting signals */
	bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	gst_bus_add_signal_watch(bus);
	g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)gst_bus_error_cb, data);
	g_signal_connect(G_OBJECT(bus), "message::eos", (GCallback)gst_bus_error_cb, data);
	g_signal_connect(G_OBJECT(bus), "message::application", (GCallback)gst_application_cb, data);
	g_signal_connect(G_OBJECT(bus), "message::state-changed", (GCallback)gst_state_changed_cb, data);
	gst_object_unref(bus);
	return 0;
}

/**
 * Construct the Gstreamer pipeline used to run
 * NN inference engine using appsink.
 */
static int gst_pipeline_nn_creation(CustomData *data)
{
	GstStateChangeReturn ret;
	GstElement *pipeline, *source,*queue2, *appsink;
	GstBus *bus;

	/* Create the pipeline */
	pipeline  = gst_pipeline_new("nn input camera pipeline");
	data->pipeline_nn = pipeline;

	/* Create gstreamer elements */
	source      = gst_element_factory_make_full("v4l2src","name","camera-source","io-mode",0,NULL);
	queue2      = gst_element_factory_make("queue",          "queue-2");
	appsink     = gst_element_factory_make("appsink",        "app-sink");

	if (!pipeline || !source || !queue2 || !appsink) {
		g_printerr("One element could not be created. Exiting.\n");
		return -1;
	}

	/* Configure the source element */
	std::stringstream device_sstr;
	device_sstr << "/dev/" << data->camera_info.nn_device;
	g_object_set(G_OBJECT(source), "device", device_sstr.str().c_str(), NULL);
	g_print("nn source used : %s \n",device_sstr.str().c_str());
	/* Configure the queue elements */
	g_object_set(G_OBJECT(queue2), "max-size-buffers", 1, "leaky", 2 /* downstream */, NULL);

	/* Create caps based on application parameters */
	std::stringstream sourceCaps_sstr;
	sourceCaps_sstr << data->camera_info.nn_caps;
	g_print("nn pipeline configuration : %s \n",sourceCaps_sstr.str().c_str());
	GstCaps *sourceCaps = gst_caps_from_string(sourceCaps_sstr.str().c_str());

	/* Configure appsink */
	g_object_set(appsink, "emit-signals", TRUE, "sync", FALSE,
		     "max-buffers", 1, "drop", TRUE, "caps", sourceCaps, NULL);
	g_signal_connect(appsink, "new-sample", G_CALLBACK(gst_new_sample_cb), data);

	/* Add all elements into the pipeline */
	gst_bin_add_many(GST_BIN(pipeline),
			 source, queue2, appsink, NULL);

	/* Link the elements together */
	if (!gst_element_link_filtered(source,queue2, sourceCaps)) {
		g_error("Failed to link elements (1)");
		return -2;
	}
	if (!gst_element_link_many(queue2, appsink, NULL)) {
		g_error("Failed to link elements (2)");
		return -2;
	}

	gst_caps_unref(sourceCaps);

	/* Instruct the bus to emit signals for each received message, and
	 * connect to the interesting signals */
	bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
	gst_bus_add_signal_watch(bus);
	g_signal_connect(G_OBJECT(bus), "message::error", (GCallback)gst_bus_error_cb, data);
	g_signal_connect(G_OBJECT(bus), "message::eos", (GCallback)gst_bus_error_cb, data);
	g_signal_connect(G_OBJECT(bus), "message::application", (GCallback)gst_application_cb, data);
	g_signal_connect(G_OBJECT(bus), "message::state-changed", (GCallback)gst_state_changed_cb, data);
	gst_object_unref(bus);
	return 0;
}

/**
 * This function display the help when -h or --help is passed as parameter.
 */
static void print_help(int argc, char** argv)
{
	std::cout <<
		"Usage: " << argv[0] << " -m <model .nb> \n"
		"\n"
		"-m --model_file <.nb file path>:      .nb model for face detection to be executed\n"
		"-f --model_fr <.nb file path>:        .nb model for face recognition to be executed\n"
		"-i --image <directory path>:          image directory with image to be classified\n"
		"-v --video_device <n>:                video device is automatically detected but can be set (example video0)\n"
		"-d --database <database directory path>: database directory with known people to be recognized \n"
		"--reco_threshold <val>: 			   threshold used to consider a person recognized from the database \n"
		"--reco_simultaneous_faces: 		   activate the recognition of simultaneous faces\n"
		"--max_db_faces <val>: 				   maximum of people in the database \n"
		"--frame_width  <val>:                 width of the camera frame (default is 640)\n"
		"--frame_height <val>:                 height of the camera frame (default is 480)\n"
		"--framerate <val>:                    framerate of the camera (default is 15fps)\n"
		"--input_mean <val>:                   model input mean (default is 127.5)\n"
		"--input_std  <val>:                   model input standard deviation (default is 127.5)\n"
		"--verbose:                            enable verbose mode\n"
		"--validation:                         enable the validation mode\n"
		"--val_run:                            set the number of draws in the validation mode\n"
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
#define OPT_VAL_RUN      1008
#define OPT_FACE_RECO_THRESHOLD    1011
#define OPT_FACE_RECO_MAX_DB_FACES 1012
#define OPT_FACE_RECO_SIM_FACE 1013

void process_args(int argc, char** argv)
{
	const char* const short_opts = "m:f:i:v:d:h";
	const option long_opts[] = {
		{"model_file",   required_argument, nullptr, 'm'},
		{"model_fr",   	 required_argument, nullptr, 'f'},
		{"image",        required_argument, nullptr, 'i'},
		{"video_device", no_argument      , nullptr, 'v'},
		{"database",     required_argument, nullptr, 'd'},
		{"reco_threshold", required_argument, nullptr, OPT_FACE_RECO_THRESHOLD},
		{"reco_simultaneous_faces", no_argument,       nullptr, OPT_FACE_RECO_SIM_FACE},
		{"max_db_faces", required_argument, nullptr, OPT_FACE_RECO_MAX_DB_FACES},
		{"frame_width",  required_argument, nullptr, OPT_FRAME_WIDTH},
		{"frame_height", required_argument, nullptr, OPT_FRAME_HEIGHT},
		{"framerate",    required_argument, nullptr, OPT_FRAMERATE},
		{"input_mean",   required_argument, nullptr, OPT_INPUT_MEAN},
		{"input_std",    required_argument, nullptr, OPT_INPUT_STD},
		{"verbose",      no_argument,       nullptr, OPT_VERBOSE},
		{"validation",   no_argument,       nullptr, OPT_VALIDATION},
		{"val_run",      required_argument, nullptr, OPT_VAL_RUN},
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
		case 'f':
			model_file_fr_str = std::string(optarg);
			std::cout << "face reco model file set to: " << model_file_fr_str << std::endl;
			break;
		case 'i':
			image_dir_str = std::string(optarg);
			std::cout << "image directory set to: " << image_dir_str << std::endl;
			break;
		case 'v':
			video_device_str = std::string(optarg);
			std::cout << "camera device set to: /dev/" << video_device_str << std::endl;
			break;
		case 'd':
			database_dir_str = std::string(optarg);
			std::cout << "database directory set to: "
				<< database_dir_str << std::endl;
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
		case OPT_FACE_RECO_SIM_FACE:
			reco_simultaneous_face = true;
			std::cout << "enable simultaneous face recognition"
				<< std::endl;
			break;
		case OPT_FRAME_WIDTH:
			camera_width_str = std::string(optarg);
			std::cout << "camera frame width set to: " << camera_width_str << std::endl;
			break;
		case OPT_FRAME_HEIGHT:
			camera_height_str = std::string(optarg);
			std::cout << "camera frame height set to: " << camera_height_str << std::endl;
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
		case OPT_VAL_RUN:
			val_run_str = std::string(optarg);
			std::cout << "number of draws has been set" << val_run_str << std::endl;
			break;
		case 'h': // -h or --help
		case '?': // Unrecognized option
		default:
			print_help(argc, argv);
			break;
		}
	}

	if (model_file_str.empty())
		print_help(argc, argv);
}

/**
 * Function called when CTRL + C or Kill signal is detected
 */
static void int_handler(int dummy)
{
	if (gtk_main_started){
		gtk_main_quit();
	} else {
		exit(1);
	}
}

/**
 * Main function
 */
int main(int argc, char *argv[])
{
	CustomData data;
	int ret;
	int ret_nn;

	/* Catch CTRL + C signal */
	signal(SIGINT, int_handler);
	/* Catch kill signal */
	signal(SIGTERM, int_handler);

	/* Process the application parameters */
	process_args(argc, argv);

	/* Get number of CPU cores available */
	uint8_t nb_cpu_cores = 1;
	const auto processor_count = std::thread::hardware_concurrency();
	std::cout << processor_count << " cpu core(s) available\n";
	if (processor_count)
		nb_cpu_cores = (uint8_t)processor_count;

	/* Initialize our data structure */
	data.pipeline = NULL;
	data.pipeline_nn = NULL;
	data.window_main = NULL;
	data.window_overlay = NULL;
	data.new_inference = false;
	data.frame_width  = std::stoi(camera_width_str);
	data.frame_height = std::stoi(camera_height_str);
	data.val_run  = std::stoi(val_run_str);
	data.ui_box_line_width = 2.0;
	data.ui_weston_panel_thickness = 32;
	data.max_db_faces = max_db_faces;
	data.total_face_reco_inference_time = 0;

	if (database_dir_str.empty())
		database_dir_str = DEFAULT_DATABASE_DIRECTORY;

	/* stai_mpu wrapper initialization */
	config.verbose = verbose;
	config.model_name = model_file_str;
	config.input_mean = input_mean;
	config.input_std = input_std;
	config.number_of_threads = nb_cpu_cores;

	stai_mpu_wrapper.Initialize(&config);

	/* stai_mpu wrapper fr initialization */
	config_fr.verbose = verbose;
	config_fr.model_name = model_file_fr_str;
	config_fr.input_mean = input_mean;
	config_fr.input_std = input_std;
	config_fr.number_of_threads = nb_cpu_cores;

	stai_mpu_wrapper_fr.Initialize(&config_fr);

	/* Get model input size */
	data.nn_input_width = stai_mpu_wrapper.GetInputWidth();
	data.nn_input_height = stai_mpu_wrapper.GetInputHeight();
	std::string nn_input_width = std::to_string(data.nn_input_width);
	std::string nn_input_height = std::to_string(data.nn_input_height);

	data.nn_fr_input_width = stai_mpu_wrapper_fr.GetInputWidth();
	data.nn_fr_input_height = stai_mpu_wrapper_fr.GetInputHeight();
	std::string nn_fr_input_width = std::to_string(data.nn_input_width);
	std::string nn_fr_input_height = std::to_string(data.nn_input_height);

	/* Calculate BlaseFace anchors */
	blaze_face.CalculateAnchors();

	/* If image_dir is set by the user, test data picture are used instead
	 * of camera frames */
	if (image_dir_str.empty()) {
		data.frame_width  = std::stoi(camera_width_str);
		data.frame_height = std::stoi(camera_height_str);
		data.preview_enabled = true;
		std::stringstream check_camera_cmd;
		int check_camera;
		//Test if a camera is connected
		check_camera_cmd << "source " << RESOURCES_DIRECTORY << "check_camera_preview.sh";
		check_camera = system(check_camera_cmd.str().c_str());
		if (check_camera != 0){
			g_print("no camera connected \n");
			exit(1);
		}
		data.camera_info = setup_camera(nn_input_width,nn_input_height);
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

	/* Initialize GTK */
	gtk_init(&argc, &argv);
	get_display_resolution(&data);
	gui_gtk_style(&data);
	gui_set_ui_parameters(&data);
	gui_set_icons_parameters(&data);

	if (data.preview_enabled) {
		/*  Camera preview use case */
		/* Initialize GStreamer for camera preview use case*/
		gst_init(&argc, &argv);
		/* Create the GStreamer pipeline for camera use case */
		ret = gst_pipeline_preview_creation(&data);
		if(ret)
			exit(1);
		ret_nn = gst_pipeline_nn_creation(&data);
		if(ret_nn)
			exit(1);

	 }

	/* Create the GUI containing the video stream  */
	g_print("Start Creating main GTK window\n");
	gui_create_main(&data);
	if (data.preview_enabled) {
		g_print("gst set pipeline playing state\n");
		gst_element_set_state (data.pipeline, GST_STATE_PLAYING);
		gst_element_set_state (data.pipeline_nn, GST_STATE_PLAYING);
	}

	/* Create the second GUI containing the nn information  */
	g_print("Start Creating overlay GTK window\n");
	gui_create_overlay(&data);

	/* Create the virtual keyboard  */
	g_print("Start Creating keyboard GTK window\n");
	gui_create_keyboard(&data);

	/* Start a timeout timer in validation process to close application if
	 * timeout occurs */
	if (validation) {
		data.valid_draw_count = 0;
		data.valid_timeout_id = g_timeout_add(100000,
						      valid_timeout_callback,
						      NULL);
	}

	gtk_main_started = true;
	gtk_main();
	gtk_main_started = false;

	/* Out of the main loop, clean up nicely */
	if (data.preview_enabled) {
		/* Camera preview use case */
		g_print("Returned, stopping Gst pipeline\n");
		gst_element_set_state(data.pipeline, GST_STATE_NULL);
		gst_element_set_state(data.pipeline_nn, GST_STATE_NULL);

		g_print("Deleting Gst pipeline\n");
		gst_object_unref(data.pipeline);
		gst_object_unref(data.pipeline_nn);
	}
	g_print(" Application exited properly \n");
	return 0;
}
