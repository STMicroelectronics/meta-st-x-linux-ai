#!/usr/bin/python3
#
# Copyright (c) 2025 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.


from datetime import datetime
import shutil
import uuid
import numpy as np
import argparse
import signal
import os
import random
import time
import subprocess

import os.path

import threading
import cv2
import matplotlib.pyplot as plt
import matplotlib
matplotlib.use('Agg')

from utils.student_utils import postprocessing_utils
from utils.student_utils.inference_utils import SSD_MobileNetV2 as ssd_mobilenetv2
from utils.student_utils.inference_utils import process_output
from utils.student_utils.postprocessing_utils import generate_ssd_anchors
from utils.student_utils.postprocessing_utils import AnchorsMatcher
from utils.student_utils.quantization_utils import *
from utils.student_utils.evaluation_utils import group_annotation_by_class, compute_mAP_per_class
from utils.student_utils.plot_utils import *

from utils.teacher_utils.annotation_utils import RTDETR, COCO_CLASSES

from utils.dataset_utils.preprocessing_utils import TrainSetTransform, TestSetTransform
from utils.dataset_utils.dataloading_utils import CustomDataset, split_data

import onnx
import onnxruntime.training.api as orttraining
from onnxruntime.tools.onnx_model_utils import fix_output_shapes, make_dim_param_fixed
from onnxruntime.quantization import quant_pre_process, quantize_static, QuantFormat, QuantType
import utils.student_utils.ssd_mobilenetv2_utils as mobilenetv2_ssd_config

from torch.utils.data import DataLoader
import numpy as np
import os
import time

import socket

import gi
gi.require_version('Gtk', '3.0')
gi.require_version('Gst', '1.0')
from gi.repository import Gtk
from gi.repository import Gdk
from gi.repository import GLib
from gi.repository import GdkPixbuf
from gi.repository import Gst
from gi.repository import Pango
from gi.repository import Gio


# init gstreamer
Gst.init(None)
Gst.init_check(None)
# init gtk
Gtk.init(None)
Gtk.init_check(None)

# path definition
RESOURCES_DIRECTORY = os.path.abspath(os.path.dirname(__file__)) + "/../resources/"

# Directories paths
NEW_IMAGES_DIRECTORY = os.path.abspath(os.path.dirname(__file__))  + "/dataset/new_images/"
OLD_IMAGES_DIRECTORY = os.path.abspath(os.path.dirname(__file__))  + "/dataset/old_images/"
ANNOTATED_IMG_DIRECTORY = os.path.abspath(os.path.dirname(__file__))  + "/annotated_images/"
DATASET_DIRECTORY = os.path.abspath(os.path.dirname(__file__))  + "/dataset/"
ANNOTATIONS_DIRECTORY = os.path.abspath(os.path.dirname(__file__))  + "/dataset/Annotations/"
ARTIFACTS_DIRECTORY = os.path.abspath(os.path.dirname(__file__)) + "/student_model/ssd_mobilenet_v2/training_artifacts/"
INFERENCE_MODELS_DIRECTORY = os.path.abspath(os.path.dirname(__file__)) + "/student_model/ssd_mobilenet_v2/inference_artifacts/"

# Number of images to move from new_images to old_images at each data retrieval
NB_NEW_IMAGES_TO_MOVE_TO_OLD_IMAGES = 20


class MainWindow(Gtk.Window):
    """
    This class handles all the functions necessary
    to display video stream in GTK GUI or still
    pictures using OpenCV
    """

    def __init__(self, args, app):
        """
        Setup instances of class and shared variables
        useful for the application
        """
        Gtk.Window.__init__(self)
        self.app = app
        self.set_ui_param()
        self.exit_icon_path = RESOURCES_DIRECTORY + f"exit_{self.ui_icon_exit_width}x{self.ui_icon_exit_height}.png"
        self.args = args

        # Initialize a dictionary to track last click times for images
        self.last_click_times = {}
        self.double_click_threshold = 0.5  # seconds

        self.data_retrieve_video_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, margin_start=15, margin_bottom=5)
        self.inference_video_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=5, margin_end=5, margin_start=5, margin_top=5)

        # Initialize attributes for images capture
        self.dataset_limit = 0
        self.retrieval_frequency = 0
        self.stop_data_retrieval = False
        self.image_capture_count = 0
        self.image_capture_active = False
        self.save_next_frame = False
        self.image_to_send = ""
        self.last_sampled_image = ""

        # Get the class name from label.txt and check if present in COCO classes
        self.class_names = [name.strip() for name in open(self.args.label_file).readlines()]
        if self.class_names[0] in COCO_CLASSES.keys():
            self.target_label = COCO_CLASSES[self.class_names[0]]
        else:
            self.target_label = 0 # Detect person by default

        # Create the Gstreamer pipeline along with the GTK Main window
        self.camera_pipeline_creation()
        self.main_ui_creation(args)
        self.pipeline_preview.set_state(Gst.State.PLAYING)

        # Used to stop the annotation process
        self.stop_event = threading.Event()

        # During data retrieval, if no image is received since this timeout, the process is stopped
        self.last_image_time = time.time() + 1000
        self.timeout_seconds = 7



    def camera_pipeline_creation(self):
        """
        Creation of the gstreamer pipeline when gstwidget is created dedicated to handle
        camera stream and NN inference (mode single pipeline)
        """
        # gstreamer pipeline creation
        self.pipeline_preview = Gst.Pipeline()
        # creation of the source libcamera
        self.libcamerasrc = Gst.ElementFactory.make("libcamerasrc", "libcamera")
        if not self.libcamerasrc:
            raise Exception("Could not create Gstreamer camera source element")
        # creation of the libcamerasrc caps for the pipelines for camera
        caps = "video/x-raw,width=" + str(self.app.frame_width) + ",height=" + str(self.app.frame_height) + ",format=RGB16"
        print("Camera pipeline configuration : ",caps)
        caps_src = Gst.Caps.from_string(caps)
        # creation of the libcamerasrc caps for the pipelines for nn
        caps = "video/x-raw,width=" + str(self.app.nn_input_width) + ",height=" + str(self.app.nn_input_height) + ",format=RGB"
        print("Aux pipe configuration:  ", caps)
        caps_src0 = Gst.Caps.from_string(caps)

        # creation of the queues elements
        queue  = Gst.ElementFactory.make("queue", "queue")
        # Only one buffer in the queue0 to get 30fps on the display preview pipeline
        queue0 = Gst.ElementFactory.make("queue", "queue0")
        queue0.set_property("leaky", 2)  # 0: no leak, 1: upstream (oldest), 2: downstream (newest)
        queue0.set_property("max-size-buffers", 1)  # Maximum number of buffers in the queue

        # creation of the gtkwaylandsink element to handle the gstreamer video stream
        self.gtkwaylandsink = Gst.ElementFactory.make("gtkwaylandsink")

         # creation and configuration of the fpsdisplaysink element to measure display fps
        fpsdisplaysink = Gst.ElementFactory.make("fpsdisplaysink", "fpsmeasure")
        fpsdisplaysink.set_property("signal-fps-measurements", True)
        fpsdisplaysink.set_property("fps-update-interval", 2000)
        fpsdisplaysink.set_property("text-overlay", False)
        fpsdisplaysink.set_property("video-sink", self.gtkwaylandsink)
        fpsdisplaysink.connect("fps-measurements",self.get_fps_display)

        # creation and configuration of the appsink element
        self.appsink = Gst.ElementFactory.make("appsink", "appsink")
        self.appsink.set_property("emit-signals", True)
        self.appsink.set_property("sync", False)
        self.appsink.set_property("max-buffers", 1)
        self.appsink.set_property("drop", True)
        self.appsink.connect("new-sample", self.new_sample)

        # check if all elements were created
        if not all([self.pipeline_preview, self.libcamerasrc, queue, queue0, fpsdisplaysink, self.gtkwaylandsink, self.appsink]):
            print("Not all elements could be created. Exiting.")
            return False

        # add all elements to the pipeline
        self.pipeline_preview.add(self.libcamerasrc)
        self.pipeline_preview.add(queue)
        self.pipeline_preview.add(queue0)
        self.pipeline_preview.add(fpsdisplaysink)
        self.pipeline_preview.add(self.appsink)

        # linking elements together
        #
        #              | src   --> queue  [caps_src]  --> fpsdisplaysink (connected to gtkwaylandsink)
        # libcamerasrc |
        #              | src_0 --> queue0 [caps_src0] --> appsink
        #
        queue.link_filtered(fpsdisplaysink, caps_src)
        queue0.link_filtered(self.appsink, caps_src0)
        src_pad = self.libcamerasrc.get_static_pad("src")
        src_request_pad_template = self.libcamerasrc.get_pad_template("src_%u")
        src_request_pad0 = self.libcamerasrc.request_pad(src_request_pad_template, None, None)
        queue_sink_pad = queue.get_static_pad("sink")
        queue0_sink_pad = queue0.get_static_pad("sink")

        # view-finder
        src_pad.set_property("stream-role", 3)
        src_request_pad0.set_property("stream-role", 1)
        src_pad.link(queue_sink_pad)
        src_request_pad0.link(queue0_sink_pad)

        # getting pipeline bus
        self.bus_pipeline = self.pipeline_preview.get_bus()
        self.bus_pipeline.add_signal_watch()
        self.bus_pipeline.connect('message::error', self.msg_error_cb)
        self.bus_pipeline.connect('message::eos', self.msg_eos_cb)
        self.bus_pipeline.connect('message::info', self.msg_info_cb)
        self.bus_pipeline.connect('message::state-changed', self.msg_state_changed_cb)
        self.bus_pipeline.connect('message::application', self.msg_application_cb)

        return True


    def msg_eos_cb(self, bus, message):
        """
        Catch gstreamer end of stream signal
        """
        print('eos message -> {}'.format(message))

    def msg_info_cb(self, bus, message):
        """
        Catch gstreamer info signal
        """
        print('info message -> {}'.format(message))

    def msg_error_cb(self, bus, message):
        """
        Catch gstreamer error signal
        """
        print('error message -> {}'.format(message.parse_error()))

    def msg_state_changed_cb(self, bus, message):
        """
        Catch gstreamer state changed signal
        """
        oldstate,newstate,pending = message.parse_state_changed()
        if (oldstate == Gst.State.NULL) and (newstate == Gst.State.READY):
            Gst.debug_bin_to_dot_file(self.pipeline_preview, Gst.DebugGraphDetails.ALL,"pipeline_py_NULL_READY")

    def msg_application_cb(self, bus, message):
        """
        Catch gstreamer application signal
        """
        if message.get_structure().get_name() == 'inference-done':
            self.app.update_ui()
        return



    def gst_to_opencv(self,sample):
        """
        conversion of the gstreamer frame buffer into numpy array
        """
        buf = sample.get_buffer()
        caps = sample.get_caps()

        number_of_column = caps.get_structure(0).get_value('width')
        number_of_lines = caps.get_structure(0).get_value('height')
        channels = 3
        strides = (912,3,1)
        arr = np.ndarray(
            (number_of_lines,
             number_of_column,
             channels),
            strides=strides,
            buffer=buf.extract_dup(0, buf.get_size()),
            dtype=np.uint8)
        return arr

    def new_sample(self,*data):
        """
        recover video frame from appsink
        and run inference
        """
        sample = self.appsink.emit("pull-sample")
        arr = self.gst_to_opencv(sample)
        if arr is not None :
            # Save the frame if image capture is active
            if self.image_capture_active and self.save_next_frame:
                timestamp = datetime.now().strftime("%Y%m%d")
                # Generate a unique filename using a random ID
                random_id = uuid.uuid4().hex
                image_filename = f"{random_id}-{timestamp}.png"
                image_path = os.path.join(NEW_IMAGES_DIRECTORY, image_filename)
                self.image_to_send = image_path
                # Convert from RGB to BGR
                arr_bgr = cv2.cvtColor(arr, cv2.COLOR_RGB2BGR)
                print(f"Saving image to: {image_path}")
                cv2.imwrite(image_path, arr_bgr)
                # Reset flag to save frames only according to retrieval frequency
                self.save_next_frame = False

            # Perform inference only in inference tab
            if self.notebook.get_current_page() == 4:
                self.app.nn_result_locations, self.app.nn_result_scores, self.app.nn_result_classes, _, self.app.nn_inference_time, _ = self.app.nn(arr)
                self.app.nn_inference_fps = (1/(self.app.nn_inference_time/1000))

            struc = Gst.Structure.new_empty("inference-done")
            msg = Gst.Message.new_application(None, struc)
            self.bus_pipeline.post(msg)
        return Gst.FlowReturn.OK

    def get_fps_display(self,fpsdisplaysink,fps,droprate,avgfps):
        """
        measure and recover display fps
        """
        self.instant_fps = fps
        return self.instant_fps


    def set_ui_param(self):
        """
        Setup all the UI parameter depending
        on the screen size
        """

        window_constraint = min(self.app.window_width, self.app.window_height)
        if window_constraint <= 272:
            print("Display config <= 272p")
            self.ui_icon_exit_width = 25
            self.ui_icon_exit_height = 25
        elif window_constraint <= 600:
            print("Display config <= 600p")
            self.ui_icon_exit_width = 50
            self.ui_icon_exit_height = 50
        elif window_constraint <= 720:
            print("Display config <= 720p")
            self.ui_icon_exit_width = 50
            self.ui_icon_exit_height = 50
        elif window_constraint <= 1080:
            print("Display config <= 1080p")
            self.ui_icon_exit_width = 50
            self.ui_icon_exit_height = 50
        else:
            print("Display config fallback")

    def apply_css(self):
        """Load and apply CSS from the style.css file."""
        css_provider = Gtk.CssProvider()

        # Try loading the CSS file and print if it succeeds
        try:
            css_provider.load_from_path(RESOURCES_DIRECTORY + "ODL_style.css")
            print("CSS loaded successfully!")
        except Exception as e:
            print(f"Failed to load CSS: {e}")

        # Apply the CSS provider to the default screen
        screen = Gdk.Screen.get_default()
        Gtk.StyleContext.add_provider_for_screen(screen, css_provider, Gtk.STYLE_PROVIDER_PRIORITY_USER)


    def main_ui_creation(self, args):
        """
        Setup the Gtk UI of the main window
        """
        self.set_decorated(False)
        self.maximize()
        self.main_box = Gtk.Box()
        self.main_box.set_homogeneous(False)

        # Create the left container to contain the Notebook with several tabs
        self.left_container = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0, name="left-container")
        self.left_container.set_homogeneous(False)
        self.main_box.pack_start(self.left_container, False, True, 0)

        # Create right container for displaying application information
        self.right_container = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        self.right_container.set_homogeneous(False)
        self.main_box.pack_end(self.right_container, False, True, 0)

        # Create the grid container for the notebook
        grid_notebook = Gtk.Grid()
        grid_notebook.set_row_homogeneous(True)
        grid_notebook.set_column_spacing(0)
        self.left_container.pack_start(grid_notebook, True, True, 0)
        self.notebook = Gtk.Notebook()
        self.notebook.set_name("nbmain")
        self.notebook.set_hexpand(True)
        self.notebook.set_show_tabs(False)
        grid_notebook.attach(self.notebook, 0, 0, 1, 1)


        ##################################################
        #####        1st tab: Data Retrieval         #####
        ##################################################
        tab0_page = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, name="tab-page")
        self.notebook.append_page(tab0_page, None)
        tab0_label = Gtk.Label(label="Data Retrieval")
        tab0_label.set_sensitive(False)
        tab0_label.set_hexpand(True)
        self.notebook.set_tab_label(self.notebook.get_nth_page(0), tab0_label)

        tab0_label.set_name("tab-data-retrieval")
        self.notebook.get_tab_label(tab0_page).set_sensitive(False)

        retrieval_params_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=5, margin_end=5, margin_start=5, margin_top=15)
        tab0_page.pack_start(retrieval_params_box, False, False, 5)

        retrieval_frequency_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, margin_start=5, name="white-bg-box")
        retrieval_limit_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, name="white-bg-box")
        retrieval_launch_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, margin_end=5, name="white-bg-box")
        retrieval_params_box.pack_start(retrieval_frequency_box, True, True, 5)
        retrieval_params_box.pack_start(retrieval_limit_box, True, True, 5)
        retrieval_params_box.pack_start(retrieval_launch_box, True, True, 5)

        # Dataset limit scale
        retrieval_limit_adjustment = Gtk.Adjustment(value=20, lower=20, upper=500, step_increment=1)
        retrieval_limit_scale = Gtk.Scale(
            orientation=Gtk.Orientation.HORIZONTAL, adjustment=retrieval_limit_adjustment, name="custom-scale",
            margin_bottom=5, margin_end=5, margin_start=5)
        retrieval_limit_scale.set_digits(0)
        retrieval_limit_scale.set_value_pos(Gtk.PositionType.TOP)
        retrieval_limit_scale.set_hexpand(True)
        retrieval_limits = [30, 50, 100, 200, 300, 400, 500]
        for limits in retrieval_limits:
            retrieval_limit_scale.add_mark(limits, Gtk.PositionType.TOP, str(limits))
        retrieval_limit_label = Gtk.Label(label="Dataset limit", margin_top=5)
        retrieval_limit_label.set_halign(Gtk.Align.CENTER)
        retrieval_limit_box.pack_start(retrieval_limit_label, True, False, 0)
        retrieval_limit_box.pack_start(retrieval_limit_scale, True, False, 0)

        # Retrieval frequency scale
        retrieval_freq_adjustment = Gtk.Adjustment(value=5, lower=2, upper=10, step_increment=2)
        retrieval_freq_scale = Gtk.Scale(
            orientation=Gtk.Orientation.HORIZONTAL, adjustment=retrieval_freq_adjustment, name="custom-scale",
            margin_bottom=5, margin_start=5, margin_end=5)
        retrieval_freq_scale.set_digits(0)
        retrieval_freq_scale.set_value_pos(Gtk.PositionType.TOP)
        retrieval_freq_scale.set_hexpand(True)
        retrieval_freqs = [2, 4, 6, 8, 10]
        for freq in retrieval_freqs:
            retrieval_freq_scale.add_mark(freq, Gtk.PositionType.TOP, str(freq))
        retrieval_freq_label = Gtk.Label(label="Retrieval frequency (img/10s)", margin_top=5)
        retrieval_freq_label.set_halign(Gtk.Align.CENTER)
        retrieval_frequency_box.pack_start(retrieval_freq_label, True, False, 0)
        retrieval_frequency_box.pack_start(retrieval_freq_scale, True, False, 0)

        # Launch data retrieval button
        retrieval_launch_button = Gtk.Button(label="Launch data retrieval", name="button", halign=Gtk.Align.CENTER,valign=Gtk.Align.CENTER,)
        retrieval_launch_button.connect("clicked", self.on_launch_data_retrieval_clicked)
        retrieval_launch_button.hide()
        retrieval_image_count = Gtk.Label(label="Retrieved images: 0", valign=Gtk.Align.CENTER)
        retrieval_image_count.set_halign(Gtk.Align.CENTER)
        retrieval_image_count.hide()

        self.retrieval_limit_scale = retrieval_limit_scale
        self.retrieval_freq_scale = retrieval_freq_scale
        self.retrieval_launch_button = retrieval_launch_button
        self.retrieval_image_count = retrieval_image_count
        retrieval_launch_box.pack_start(retrieval_launch_button, True, False, 0)
        retrieval_launch_box.pack_start(retrieval_image_count, True, False, 0)

        separator_data = Gtk.Separator(orientation=Gtk.Orientation.HORIZONTAL)
        tab0_page.pack_start(separator_data, False, True, 5)

        # Create a central box to contain the video_box and scrolled window
        retrieval_main_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=5, margin_start=15, margin_end=15, margin_bottom=5, name="white-bg-box")
        tab0_page.pack_start(retrieval_main_box, False, False, 5)

        # Create a data retrieval video box and put it inside retrieval_main_box (left)
        self.data_retrieve_video_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, margin_start=20, margin_bottom=35, margin_top=35, margin_end=5)
        self.data_retrieve_video_box.pack_start(self.gtkwaylandsink.props.widget, False, False, 5)
        retrieval_main_box.pack_start(self.data_retrieve_video_box, False, False, 5)

        # Create a scrolled window for the images and put it inside center box (right)
        retrieval_scroll_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, margin_start=5, margin_bottom=5, margin_top=23)
        retrieval_main_box.pack_start(retrieval_scroll_box, True, True, 5)
        retrieval_scroll_window = Gtk.ScrolledWindow()
        retrieval_scroll_box.pack_start(retrieval_scroll_window, True, True, 5)
        retrieval_scroll_window.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        retrieval_main_box.pack_start(retrieval_scroll_window, True, True, 5)

        # Create a FlowBox to arrange images and placeholders when no images are available
        retrieval_flowbox = Gtk.FlowBox(row_spacing=10, margin_bottom=10, margin_top=10, column_spacing=10)
        retrieval_flowbox.set_valign(Gtk.Align.START)
        retrieval_flowbox.set_max_children_per_line(4)
        retrieval_flowbox.set_min_children_per_line(4)
        retrieval_flowbox.set_selection_mode(Gtk.SelectionMode.NONE)
        retrieval_scroll_window.add(retrieval_flowbox)
        self.retrieval_flowbox = retrieval_flowbox
        self.retrieval_scroll_window = retrieval_scroll_window

        # Calculate the fixed width for the FlowBox and add empty placeholders to the FlowBox
        child_width = 80
        column_spacing = 10
        num_columns = 4
        total_width = (child_width * num_columns) + (column_spacing * (num_columns - 1)) + 40
        self.retrieval_flowbox.set_size_request(total_width, -1)  # Set the width, height is -1 to keep it dynamic
        num_placeholders = 12
        for _ in range(num_placeholders):
            placeholder = self.create_empty_image_placeholder()
            self.retrieval_flowbox.add(placeholder)

        # Initialize image files list
        self.image_files = []
        self.image_widgets = {}
        self.monitoring_active = False

        # Data retrieval bottom navigation box
        retrieval_nav_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=5, valign=Gtk.Align.CENTER, name="nav-box", height_request=50)
        tab0_page.pack_start(retrieval_nav_box, True, True, 0)
        retrieval_empty_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, valign=Gtk.Align.CENTER, margin_end=70)
        retrieval_nav_box.pack_start(retrieval_empty_box, False, False, 0)
        retrieval_label = Gtk.Label(label="Data retrieval",  vexpand=False, valign=Gtk.Align.CENTER, halign=Gtk.Align.CENTER, name="tab-label")
        retrieval_nav_box.pack_start(retrieval_label, True, True, 0)
        self.retrieval_next_button = Gtk.Button(label="Next >", name="button", vexpand=False, valign=Gtk.Align.CENTER, margin_end=25)
        self.retrieval_next_button.connect("clicked", self.on_retrieval_next_button_clicked)
        retrieval_nav_box.pack_end(self.retrieval_next_button, False, False, 0)


        ##################################################
        #####      2nd tab: Data Visualization       #####
        ##################################################
        tab1_page = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, name="tab-page")
        self.notebook.append_page(tab1_page, None)
        tab1_label = Gtk.Label(label="Data Visualization")
        tab1_label.set_sensitive(False)
        tab1_label.set_hexpand(True)
        self.notebook.set_tab_label(self.notebook.get_nth_page(1), tab1_label)
        tab1_label.set_name("tab-data-viz")
        self.notebook.get_tab_label(tab1_page).set_sensitive(False)
        controls_tab1_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=5, margin_end=15, margin_start=15, margin_top=15, name="white-bg-box")
        tab1_page.pack_start(controls_tab1_box, False, False, 5)

        # Launch data visualization button
        launch_data_viz_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, margin_start=5)
        controls_tab1_box.pack_start(launch_data_viz_box, True, True, 5)
        data_viz_button = Gtk.Button(label="Launch data visualization", name="button", vexpand=False, valign=Gtk.Align.CENTER)
        data_viz_button.set_halign(Gtk.Align.CENTER)
        launch_data_viz_box.pack_start(data_viz_button, True, False, 0)
        data_viz_button.connect("clicked", self.on_data_viz_button_clicked)
        self.data_viz_button = data_viz_button

        # Percentage control
        percentage_scale_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5)
        controls_tab1_box.pack_start(percentage_scale_box, True, False, 5)
        percentage_label = Gtk.Label(label="      Ratio of old data     ", margin_top=5)
        percentage_label.set_halign(Gtk.Align.CENTER)
        percentage_adjustment = Gtk.Adjustment(value=20, lower=0, upper=50, step_increment=1)
        percentage_scale = Gtk.Scale(orientation=Gtk.Orientation.HORIZONTAL, adjustment=percentage_adjustment, name="custom-scale")
        percentage_scale.set_digits(0)
        percentage_scale.set_value_pos(Gtk.PositionType.TOP)
        percentage_scale.set_hexpand(False)
        percentages = [5, 10, 25, 50]
        for percentage in percentages:
            percentage_scale.add_mark(percentage, Gtk.PositionType.TOP, str(percentage))
        percentage_scale_box.pack_start(percentage_label, True, False, 0)
        percentage_scale_box.pack_start(percentage_scale, True, False, 0)
        self.percentage_scale = percentage_scale

        # Percentage control
        percentage_split_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5)
        controls_tab1_box.pack_start(percentage_split_box, True, False, 5)
        percentage_split_label = Gtk.Label(label="    Ratio of training set    ", margin_top=5)
        percentage_split_label.set_halign(Gtk.Align.CENTER)
        percentage_split_adjustment = Gtk.Adjustment(value=70, lower=50, upper=90, step_increment=1)
        percentage_split_scale = Gtk.Scale(orientation=Gtk.Orientation.HORIZONTAL, adjustment=percentage_split_adjustment, name="custom-scale")
        percentage_split_scale.set_digits(0)
        percentage_split_scale.set_value_pos(Gtk.PositionType.TOP)
        percentage_split_scale.set_hexpand(False)
        percentages = [50, 60, 70, 80, 90]
        for percentage in percentages:
            percentage_split_scale.add_mark(percentage, Gtk.PositionType.TOP, str(percentage))
        percentage_split_box.pack_start(percentage_split_label, True, False, 0)
        percentage_split_box.pack_start(percentage_split_scale, True, False, 0)

        # Split button
        self.percentage_split_scale = percentage_split_scale
        data_split_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, margin_end=5)
        controls_tab1_box.pack_start(data_split_box, True, True, 5)
        data_split_button = Gtk.Button(label="Split data", name="button", vexpand=False)
        data_split_button.set_halign(Gtk.Align.CENTER)
        data_split_button.set_valign(Gtk.Align.CENTER)
        data_split_box.pack_start(data_split_button, True, False, 0)
        data_split_button.connect("clicked", self.on_data_split_button_clicked)
        self.data_split_button = data_split_button

        # Data visualization: Old Data Section
        old_data_label_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, valign=Gtk.Align.CENTER, name="blue-bg-box", margin_top=5, margin_start=15, margin_end=15)
        tab1_page.pack_start(old_data_label_box, False, False, 0)
        old_data_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, margin_bottom=10, margin_top=10, margin_start=15, margin_end=15, name="white-bg-box")
        tab1_page.pack_start(old_data_box, True, True, 0)
        old_data_label = Gtk.Label(label="Old data", name="data-label")
        old_data_label.set_size_request(100, -1)
        old_data_label.set_halign(Gtk.Align.CENTER)
        old_data_label.set_valign(Gtk.Align.CENTER)
        old_data_label.set_margin_bottom(5)
        old_data_label_box.pack_start(old_data_label, False, False, 0)
        old_data_scrolled_window = Gtk.ScrolledWindow(margin_start=10, margin_end=10)
        old_data_scrolled_window.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.NEVER)
        old_data_scrolled_window.set_hexpand(True)
        old_data_box.pack_start(old_data_scrolled_window, True, True, 0)
        old_data_image_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=70)
        old_data_image_box.set_valign(Gtk.Align.CENTER)
        old_data_scrolled_window.add(old_data_image_box)
        for _ in range(5):
            placeholder = self.create_empty_image_placeholder()
            old_data_image_box.pack_start(placeholder, False, False, 5)
        old_data_image_box.show_all()
        self.old_data_label = old_data_label
        self.old_data_image_box = old_data_image_box

        # Data visualization: New Data Section
        new_data_label_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, valign=Gtk.Align.CENTER, name="blue-bg-box", margin_top=5, margin_start=15, margin_end=15)
        tab1_page.pack_start(new_data_label_box, False, False, 0)
        new_data_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, margin_bottom=10, margin_top=10, margin_start=15, margin_end=15, name="white-bg-box")
        tab1_page.pack_start(new_data_box, True, True, 0)
        new_data_label = Gtk.Label(label="New data", name="data-label")
        new_data_label.set_size_request(100, -1)
        new_data_label.set_halign(Gtk.Align.CENTER)
        new_data_label.set_valign(Gtk.Align.CENTER)
        new_data_label.set_margin_bottom(5)
        new_data_label_box.pack_start(new_data_label, False, False, 0)
        new_data_scrolled_window = Gtk.ScrolledWindow(margin_start=10, margin_end=10)
        new_data_scrolled_window.set_policy(Gtk.PolicyType.AUTOMATIC, Gtk.PolicyType.NEVER)
        new_data_scrolled_window.set_hexpand(True)
        new_data_box.pack_start(new_data_scrolled_window, True, True, 0)
        new_data_image_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=70)
        new_data_image_box.set_valign(Gtk.Align.CENTER)
        new_data_scrolled_window.add(new_data_image_box)
        for _ in range(5):
            placeholder = self.create_empty_image_placeholder()
            new_data_image_box.pack_start(placeholder, False, False, 5)
        new_data_image_box.show_all()
        self.new_data_label = new_data_label
        self.new_data_image_box = new_data_image_box

        # Flag to indicate if images have been loaded
        self.images_loaded = False

        # Initialize image widgets dictionary
        self.image_widgets_data_viz = {}

        # Data Visualization horizontal navigation box
        viz_nav_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=5, valign=Gtk.Align.CENTER, name="nav-box", height_request=50)
        tab1_page.pack_start(viz_nav_box, False, False, 0)
        viz_next_button = Gtk.Button(label="Next >", name="button", margin_end=20, vexpand=False, valign=Gtk.Align.CENTER)
        viz_back_button = Gtk.Button(label="< Back", name="button", margin_start=20, vexpand=False, valign=Gtk.Align.CENTER)
        viz_next_button.connect("clicked", self.on_viz_next_button_clicked)
        viz_back_button.connect("clicked", self.on_viz_back_button_clicked)
        viz_label = Gtk.Label(label="Data visualization",  vexpand=False, valign=Gtk.Align.CENTER, halign=Gtk.Align.CENTER, name="tab-label")
        viz_nav_box.pack_start(viz_back_button, False, False, 5)
        viz_nav_box.pack_start(viz_label, True, True, 0)
        viz_nav_box.pack_end(viz_next_button, False, False, 5)

        ##################################################
        #####            3rd tab: Annotation         #####
        ##################################################
        tab2_page = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, name="tab-page")
        self.notebook.append_page(tab2_page, None)
        tab2_label = Gtk.Label(label="Annotation")
        tab2_label.set_sensitive(False)
        tab2_label.set_hexpand(True)
        self.notebook.set_tab_label(self.notebook.get_nth_page(2), tab2_label)
        tab2_label.set_name("tab-annotation")

        # Annotation header box
        annotation_header_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=5, margin_end=10, margin_start=10, margin_top=10, homogeneous=True)
        tab2_page.pack_start(annotation_header_box, False, False, 5)
        teacher_model_label_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, name="white-bg-box")
        annotation_header_box.pack_start(teacher_model_label_box, True, True, 5)
        annotation_button_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, name="white-bg-box")
        annotation_header_box.pack_start(annotation_button_box, True, True, 5)

        # Teacher model label
        teacher_model_label = Gtk.Label(label="Teacher model")
        teacher_model_label.set_halign(Gtk.Align.CENTER)
        teacher_model_label.set_valign(Gtk.Align.CENTER)
        teacher_model_description = Gtk.Label(name="teacher-label")
        teacher_model_description_str  = f'* Model: <b>RT-DETR</b> pretrained on COCO (53% mAP).\n'
        teacher_model_description_str += f'* Class to annotate: <b>{self.class_names[0]}</b> from <b>labels.txt</b>.\n'
        teacher_model_description_str += f'* Execution Provider: <b>CPU</b> in <b>ONNX</b> format.\n'
        teacher_model_description.set_markup(teacher_model_description_str)
        teacher_model_description.set_halign(Gtk.Align.CENTER)
        teacher_model_label_box.pack_start(teacher_model_label, False, False, 5)
        teacher_model_label_box.pack_start(teacher_model_description, False, False, 5)
        teacher_model_label_box.set_size_request(150, -1)

        # Annotate Data Button
        annotate_button = Gtk.Button(label="Launch annotation", name="button")
        annotate_button.set_halign(Gtk.Align.CENTER)
        annotation_button_box.pack_start(annotate_button, True, False, 0)
        annotate_button.connect("clicked", self.on_launch_annotate_button_clicked)
        self.annotate_button = annotate_button
        self.annotation_status_label = Gtk.Label(name="teacher-label", margin_bottom=5)
        self.annotation_status_label.set_halign(Gtk.Align.CENTER)
        annotation_button_box.pack_start(self.annotation_status_label, False, False, 0)

        # Create a horizontal box to hold the annotation progress bar and timer
        annotation_progress_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=15, margin_bottom=10)
        annotation_progress_box.set_halign(Gtk.Align.CENTER)
        annotation_button_box.pack_start(annotation_progress_box, False, False, 0)

        annotation_progress_bar = Gtk.ProgressBar(valign=Gtk.Align.START)
        annotation_progress_bar.set_show_text(True)
        annotation_progress_bar.set_text("0%")
        annotation_timer_label = Gtk.Label(label="00:00:00")
        annotation_timer_label.set_halign(Gtk.Align.END)
        annotation_progress_box.pack_start(annotation_progress_bar, False, False, 0)
        annotation_progress_box.pack_start(annotation_timer_label, False, False, 0)

        # Store the progress bar and timer label as instance variables
        self.annotation_progress_bar = annotation_progress_bar
        self.annotation_timer_label = annotation_timer_label
        self.annotation_progress_box = annotation_progress_box
        self.annotation_progress_box.show()

        # Create a scrolled window for the images
        scrolled_window_annotation = Gtk.ScrolledWindow(name="white-bg-box", margin_start=15, margin_end=15, margin_bottom=10, margin_top=10)
        scrolled_window_annotation.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
        scrolled_window_annotation.hide()
        tab2_page.pack_start(scrolled_window_annotation, True, True, 0)

        # Create a FlowBox to arrange images
        annotation_flowbox = Gtk.FlowBox(row_spacing=30, margin_top=10)
        annotation_flowbox.set_valign(Gtk.Align.START)
        annotation_flowbox.set_max_children_per_line(4)
        annotation_flowbox.set_selection_mode(Gtk.SelectionMode.NONE)
        scrolled_window_annotation.add(annotation_flowbox)
        self.scrolled_window_annotation = scrolled_window_annotation
        self.annotation_flowbox = annotation_flowbox
        self.annotation_image_files = []
        self.annotation_image_widgets = {}
        self.annotation_monitoring_active = False

        # Data Annotation horizontal navigation box
        annotation_nav_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=5, valign=Gtk.Align.CENTER, halign=Gtk.Align.FILL, name="nav-box", height_request=50)
        tab2_page.pack_start(annotation_nav_box, False, False, 0)
        self.annotation_next_button = Gtk.Button(label="Next >", name="button", margin_end=20, vexpand=False, valign=Gtk.Align.CENTER)
        self.annotation_back_button = Gtk.Button(label="< Back", name="button", margin_start=20, vexpand=False, valign=Gtk.Align.CENTER)
        self.annotation_next_button.connect("clicked", self.on_annotation_next_button_clicked)
        self.annotation_back_button.connect("clicked", self.on_annotation_back_button_clicked)
        annotation_label = Gtk.Label(label="Data annotation",  vexpand=False, valign=Gtk.Align.CENTER, halign=Gtk.Align.CENTER, name="tab-label")
        annotation_nav_box.pack_start(self.annotation_back_button, False, False, 5)
        annotation_nav_box.pack_start(annotation_label, True, True, 0)
        annotation_nav_box.pack_end(self.annotation_next_button, False, False, 5)


        ##################################################
        #####          4th tab: Training             #####
        ##################################################
        tab3_page = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, name="tab-page")
        self.notebook.append_page(tab3_page, None)
        tab3_label = Gtk.Label(label="Training")
        tab3_label.set_sensitive(False)
        tab3_label.set_hexpand(True)
        self.notebook.set_tab_label(self.notebook.get_nth_page(3), tab3_label)
        # Apply a CSS ID to the label for custom styling
        tab3_label.set_name("tab-training")

        # Training parameters setting box
        training_params_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=5, margin_end=5, margin_start=5, margin_top=10)
        tab3_page.pack_start(training_params_box, False, False, 5)
        training_epoch_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, margin_start=5, name="white-bg-box")
        training_lr_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, name="white-bg-box")
        training_batch_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=5, margin_end=5, name="white-bg-box")
        training_params_box.pack_start(training_epoch_box, True, True, 5)
        training_params_box.pack_start(training_lr_box, True, True, 5)
        training_params_box.pack_start(training_batch_box, True, True, 5)

        # Epochs setting box
        training_epochs_adjustment = Gtk.Adjustment(value=5, lower=1, upper=50, step_increment=1)
        training_epochs_scale = Gtk.Scale(orientation=Gtk.Orientation.HORIZONTAL, adjustment=training_epochs_adjustment, name="custom-scale")
        training_epochs_scale.set_digits(0)
        training_epochs_scale.set_value_pos(Gtk.PositionType.TOP)
        training_epochs_scale.set_hexpand(True)
        training_epochs_label = Gtk.Label(label="Number of epochs", margin_top=5)
        training_epochs_label.set_halign(Gtk.Align.CENTER)
        epochs = [5, 10, 20, 30, 40, 50] # Marks to the scale for epochs
        for e in epochs:
            training_epochs_scale.add_mark(e, Gtk.PositionType.TOP, str(e))
        training_epoch_box.pack_start(training_epochs_label, True, False, 0)
        training_epoch_box.pack_start(training_epochs_scale, True, False, 0)

        # Learning rate scale
        training_lr_adjustment = Gtk.Adjustment(value=0.005, lower=0.0001, upper=0.1, step_increment=0.0001)
        training_lr_scale = Gtk.Scale(orientation=Gtk.Orientation.HORIZONTAL, adjustment=training_lr_adjustment, name="custom-scale")
        training_lr_scale.set_digits(4)
        training_lr_scale.set_value_pos(Gtk.PositionType.TOP)
        training_lr_scale.set_hexpand(True)
        training_lr_label = Gtk.Label(label="Learning rate", margin_top=5)
        training_lr_label.set_halign(Gtk.Align.CENTER)
        training_lr = [0.005, 0.01, 0.05, 0.1] # Marks to the scale for lr
        for lr in training_lr:
            training_lr_scale.add_mark(lr, Gtk.PositionType.TOP, str(lr))
        training_lr_box.pack_start(training_lr_label, True, False, 0)
        training_lr_box.pack_start(training_lr_scale, True, False, 0)

        # Batch sizes list
        self.batch_sizes = [4, 8]
        batch_size_adjustment = Gtk.Adjustment(value=4, lower=4, upper=8, step_increment=1, page_increment=1, page_size=0)
        self.training_batch_scale = Gtk.Scale(orientation=Gtk.Orientation.HORIZONTAL, adjustment=batch_size_adjustment, hexpand=True, name="custom-scale")
        self.training_batch_scale.set_digits(0)
        self.training_batch_scale.set_draw_value(True)
        self.training_batch_scale.set_increments(1, 1)
        self.training_batch_scale.set_value_pos(Gtk.PositionType.TOP)
        self.training_batch_scale.connect("value-changed", self.on_batch_size_changed)
        for bs in self.batch_sizes:
            self.training_batch_scale.add_mark(bs, Gtk.PositionType.TOP, str(bs))
        self.training_batch_scale.set_value(self.batch_sizes[0])
        training_batch_label = Gtk.Label(label="Batch size", margin_top=5)
        training_batch_label.set_halign(Gtk.Align.CENTER)
        training_batch_box.pack_start(training_batch_label, True, False, 0)
        training_batch_box.pack_start(self.training_batch_scale, True, False, 0)

        # Fixed-size container so that the training and evaluation areas match.
        training_hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=10, name="white-bg-box", margin_start=15, margin_end=15, margin_bottom=0, margin_top=0)
        tab3_page.pack_start(training_hbox, True, True, 10)

        # LEFT COLUMN: Launch training button and training info (epoch and timer)
        left_vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        left_vbox.set_halign(Gtk.Align.CENTER)
        left_vbox.set_valign(Gtk.Align.CENTER)
        training_hbox.pack_start(left_vbox, True, True, 0)
        launch_training_button = Gtk.Button(label="Launch training", name="button")
        launch_training_button.set_margin_top(10)
        launch_training_button.set_margin_bottom(10)
        left_vbox.pack_start(launch_training_button, False, False, 5)
        launch_training_button.connect("clicked", self.on_launch_training_clicked)
        training_status_label = Gtk.Label(label="")
        left_vbox.pack_start(training_status_label, False, False, 5)
        epoch_label = Gtk.Label(label="Epoch 0 / 0")
        left_vbox.pack_start(epoch_label, False, False, 5)
        training_timer_label = Gtk.Label(label="Elapsed time: 00:00:00")
        left_vbox.pack_start(training_timer_label, False, False, 5)

        # MIDDLE COLUMN: Loss curve diagram
        middle_vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        middle_vbox.set_halign(Gtk.Align.CENTER)
        middle_vbox.set_valign(Gtk.Align.CENTER)
        training_hbox.pack_start(middle_vbox, True, True, 0)
        loss_curve_image = Gtk.Image()
        generate_default_loss_plot()  # Create an empty default loss plot
        loss_curve_image.set_from_file("loss_curve.png")
        middle_vbox.pack_start(loss_curve_image, True, True, 5)

        # RIGHT COLUMN: Accuracy curve diagram
        right_vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        right_vbox.set_halign(Gtk.Align.CENTER)
        right_vbox.set_valign(Gtk.Align.CENTER)
        training_hbox.pack_start(right_vbox, True, True, 0)

        map_curve_image = Gtk.Image()
        generate_default_map_plot()  # Create an empty default mAP plot with proper axes/labels
        map_curve_image.set_from_file("map_curve.png")
        right_vbox.pack_start(map_curve_image, True, True, 5)

        # Save training UI widgets as instance variables (for later updates)
        self.launch_training_button = launch_training_button
        self.training_status_label = training_status_label
        self.epoch_label = epoch_label
        self.training_timer_label = training_timer_label
        self.loss_curve_image = loss_curve_image
        self.map_curve_image = map_curve_image
        self.training_epochs_scale = training_epochs_scale
        self.training_lr_scale = training_lr_scale

        # We use a two‑column layout: left column for the “Launch evaluation” button and status;
        # right column for a bar plot showing evaluation metrics.
        evaluation_hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL,
                                  name="white-bg-box",
                                  margin_start=15,
                                  margin_end=15,
                                  margin_bottom=15,
                                  margin_top=5)
        tab3_page.pack_start(evaluation_hbox, True, True, 0)

        # Left column: evaluation controls
        eval_left_vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        eval_left_vbox.set_halign(Gtk.Align.CENTER)
        eval_left_vbox.set_valign(Gtk.Align.CENTER)
        evaluation_hbox.pack_start(eval_left_vbox, True, False, 0)
        launch_evaluation_button = Gtk.Button(label="Launch evaluation", name="button")
        launch_evaluation_button.set_margin_top(10)
        launch_evaluation_button.set_margin_bottom(10)
        launch_evaluation_button.connect("clicked", self.on_launch_evaluation_clicked)
        eval_left_vbox.pack_start(launch_evaluation_button, False, False, 0)
        evaluation_status_label = Gtk.Label(label="")
        eval_left_vbox.pack_start(evaluation_status_label, False, False, 0)

        # middle column: evaluation bar plot
        eval_middle_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        eval_middle_box.set_halign(Gtk.Align.CENTER)
        eval_middle_box.set_valign(Gtk.Align.CENTER)
        evaluation_hbox.pack_start(eval_middle_box, True, True, 0)
        evaluation_bar_plot_image = Gtk.Image()
        generate_default_evaluation_plot()  # Create a default empty evaluation bar plot
        evaluation_bar_plot_image.set_from_file("evaluation_bar_plot.png")
        eval_middle_box.pack_start(evaluation_bar_plot_image, True, True, 0)

        # right column: evaluation bar plot
        eval_right_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        eval_right_box.set_halign(Gtk.Align.CENTER)
        eval_right_box.set_valign(Gtk.Align.CENTER)
        evaluation_hbox.pack_start(eval_right_box, False, False, 0)
        old_data_eval_grid = Gtk.Grid(margin_top=10)
        eval_right_box.add(old_data_eval_grid)
        old_data_eval_grid.set_halign(Gtk.Align.CENTER)
        old_data_eval_grid.set_valign(Gtk.Align.CENTER)
        old_data_switch_label = Gtk.Label(label="Eval results on old data")

        self.old_data_switch = Gtk.Switch()
        self.old_data_switch.set_halign(Gtk.Align.CENTER)
        self.old_data_switch.set_valign(Gtk.Align.CENTER)
        self.old_data_switch.set_margin_top(5)
        self.old_data_switch.set_margin_start(60)
        self.old_data_switch.connect("state-set", self.on_eval_switch_activated)

        old_data_descr_label = Gtk.Label(name="teacher-label")
        old_data_descr_str  = f'To monitor if the model has been \n'
        old_data_descr_str += f'   subject to any <b>catastrophic</b> \n'
        old_data_descr_str += f'      <b>forgetting</b> after training. \n'
        old_data_descr_label.set_markup(old_data_descr_str)
        old_data_descr_label.set_halign(Gtk.Align.CENTER)
        old_data_descr_label.set_margin_top(20)

        old_data_eval_grid.attach(old_data_switch_label, 0, 0, 5, 1)
        old_data_eval_grid.attach(self.old_data_switch, 0, 1, 1, 1)
        old_data_eval_grid.attach(old_data_descr_label, 0, 2, 5, 1)

        # Save evaluation UI widgets as instance variables
        self.launch_evaluation_button = launch_evaluation_button
        self.evaluation_status_label = evaluation_status_label
        self.evaluation_bar_plot_image = evaluation_bar_plot_image
        self.evaluation_hbox = evaluation_hbox

        # Training bottom navigation box
        training_nav_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=5, valign=Gtk.Align.CENTER, name="nav-box", height_request=50)
        tab3_page.pack_start(training_nav_box, False, False, 0)
        self.training_next_button = Gtk.Button(label="Next >", name="button", margin_end=20, vexpand=False, valign=Gtk.Align.CENTER)
        self.training_next_button.connect("clicked", self.on_training_next_button_clicked)
        self.training_back_button = Gtk.Button(label="< Back", name="button", margin_start=20, vexpand=False, valign=Gtk.Align.CENTER)
        self.training_back_button.connect("clicked", self.on_training_back_button_clicked)
        training_label = Gtk.Label(label="Training and evaluation",  vexpand=False, valign=Gtk.Align.CENTER, halign=Gtk.Align.CENTER, name="tab-label")
        training_nav_box.pack_start(self.training_back_button, False, False, 5)
        training_nav_box.pack_start(training_label, True, True, 0)
        training_nav_box.pack_end(self.training_next_button, False, False, 5)


        ##################################################
        #####          5th tab: Inference            #####
        ##################################################
        tab5_page = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        self.notebook.append_page(tab5_page, None)
        tab5_label = Gtk.Label(label="Inference")
        tab5_label.set_sensitive(False)
        tab5_label.set_hexpand(True)
        self.notebook.set_tab_label(self.notebook.get_nth_page(4), tab5_label)
        tab5_label.set_name("tab-inference")
        self.tab5_page = tab5_page
        self.inference_video_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        self.inference_video_box.set_name("inference-main-box")
        tab5_page.pack_start(self.inference_video_box, True, True, 0)
        inference_nav_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=5, valign=Gtk.Align.CENTER, name="nav-box", height_request=50)
        inference_back_button = Gtk.Button(label="< Back", name="button", margin_start=20, vexpand=False, valign=Gtk.Align.CENTER)
        inference_back_button.connect("clicked", self.on_inference_back_button_clicked)
        inference_label = Gtk.Label(label="Inference",  vexpand=False, valign=Gtk.Align.CENTER, halign=Gtk.Align.CENTER, name="tab-label")
        inference_empty_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, valign=Gtk.Align.CENTER, margin_end=70)
        inference_nav_box.pack_start(inference_back_button, False, False, 5)
        inference_nav_box.pack_start(inference_label, True, True, 0)
        inference_nav_box.pack_start(inference_empty_box, False, False, 0)


        ##################################################
        #####        Right container: infos          #####
        ##################################################
        # Create the exit icon widget
        exit_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        exit_icon_main = Gtk.Image()
        exit_icon_event_box = Gtk.EventBox()
        exit_icon_event_box.add(exit_icon_main)

        def exit_icon_cb(widget, event):
            print("Exit button clicked")
            Gtk.main_quit()

        exit_icon_event_box.connect("button_press_event", exit_icon_cb)
        exit_icon_main.set_from_file(self.exit_icon_path)
        exit_box.pack_end(exit_icon_event_box, False, False, 2)
        self.right_container.pack_start(exit_box, False, False, 0)
        # Create a grid container for the volume buttons and text labels
        info_grid = Gtk.Grid()
        info_grid.set_halign(Gtk.Align.CENTER)
        info_grid.set_valign(Gtk.Align.CENTER)
        self.right_container.add(info_grid)
        # Create the main app logo
        logo0_sstr = f"{RESOURCES_DIRECTORY}ODL_st_icon_232px.png"
        logo_pixbuf = GdkPixbuf.Pixbuf.new_from_file(logo0_sstr)
        scaled_logo_pixbuf = logo_pixbuf.scale_simple(140, 140, GdkPixbuf.InterpType.BILINEAR)
        logo0 = Gtk.Image.new_from_pixbuf(scaled_logo_pixbuf)
        # Create the first label
        label0 = Gtk.Label(label="")
        info_sstr2 = "      Teacher-Student    \n      machine learning    \n"
        label_to_display2 = f'<span font="14" color="#FFFFFFFF"><b>{info_sstr2}</b></span>'
        label0.set_markup(label_to_display2)
        label0.set_justify(Gtk.Justification.CENTER)
        label0.set_margin_top(20)
        # Create the second label
        label1 = Gtk.Label(label="")
        info_sstr2 = " Powered by:\n"
        label_to_display2 = f'<span font="14" color="#FFD200FF"><b>{info_sstr2}</b></span>'
        label1.set_markup(label_to_display2)
        label1.set_justify(Gtk.Justification.CENTER)
        label1.set_margin_top(5)
        # Create the main app logo
        logo1_sstr = f"{RESOURCES_DIRECTORY}ODL_onnxruntime.png"
        logo_pixbuf = GdkPixbuf.Pixbuf.new_from_file(logo1_sstr)
        scaled_ort_logo_pixbuf = logo_pixbuf.scale_simple(120, 55, GdkPixbuf.InterpType.BILINEAR)
        logo1 = Gtk.Image.new_from_pixbuf(scaled_ort_logo_pixbuf)
        # Create the second label
        label2 = Gtk.Label(label="")
        info_sstr2 = " Use case:\n"
        label_to_display2 = f'<span font="14" color="#FFD200FF"><b>{info_sstr2}</b></span>'
        label2.set_markup(label_to_display2)
        label2.set_justify(Gtk.Justification.CENTER)
        label2.set_margin_top(20)
        # Create the main app logo
        logo2_sstr = f"{RESOURCES_DIRECTORY}ODL_object_detect.png"
        logo_pixbuf = GdkPixbuf.Pixbuf.new_from_file(logo2_sstr)
        scaled_ort_logo_pixbuf = logo_pixbuf.scale_simple(120, 90, GdkPixbuf.InterpType.BILINEAR)
        logo2 = Gtk.Image.new_from_pixbuf(scaled_ort_logo_pixbuf)
        # Attach widgets to the grid
        info_grid.attach(logo0, 0, 0, 1, 1)
        info_grid.attach(label0, 0, 1, 1, 1)
        info_grid.attach(label1, 0, 2, 1, 1)
        info_grid.attach(logo1, 0, 3, 1, 1)
        info_grid.attach(label2, 0, 4, 1, 1)
        info_grid.attach(logo2, 0, 5, 1, 1)
        # Set the background color of the right section to blue
        blue_color = Gdk.RGBA()
        blue_color.parse("#03234b")
        self.right_container.override_background_color(Gtk.StateFlags.NORMAL, blue_color)
        self.apply_css()
        self.add(self.main_box)
        return True

    def create_empty_image_placeholder(self):
        # Create an empty image with a gray border
        empty_image = Gtk.Image()
        empty_image.set_size_request(80, 80)
        # Create an EventBox to add a border
        event_box = Gtk.EventBox()
        event_box.add(empty_image)
        # Apply the CSS class for styling
        event_box.get_style_context().add_class("placeholder")
        return event_box

    def on_batch_size_changed(self, scale):
        value = scale.get_value()
        closest_value = min(self.batch_sizes, key=lambda x: abs(x - value))
        scale.set_value(closest_value)

    def on_launch_data_retrieval_clicked(self, button):
        self.data_viz_button.set_sensitive(True)
        self.retrieval_next_button.set_sensitive(False)
        if self.retrieval_launch_button.get_label() == "Launch data retrieval" or self.retrieval_launch_button.get_label() == "New data retrieval":
            self.last_image_time = time.time() + 1000
            # Intialization
            self.image_files = []
            self.retrieval_image_count.set_text("Retrieved images: 0")
            # Create dataset directory and subdirectories
            dataset_dir = DATASET_DIRECTORY
            subdirs = ["new_images", "old_images", "train", "test", "eval", "Annotations"]
            os.makedirs(dataset_dir, exist_ok=True)
            for subdir in subdirs:
                os.makedirs(os.path.join(dataset_dir, subdir), exist_ok=True)
            if not os.path.exists(DATASET_DIRECTORY + "train/"):
                print(f"Directory {DATASET_DIRECTORY}train/ does not exist.")
                contents = []
            else:
                contents = os.listdir(DATASET_DIRECTORY + "train/")
            # Check if the directory is empty
            if not contents:
                print(f"Directory {DATASET_DIRECTORY}train/ is empty.")
            else:
                train_dir_contents = os.listdir(DATASET_DIRECTORY + "train/")
                eval_dir_contents = os.listdir(DATASET_DIRECTORY + "eval/")
                test_dir_contents = os.listdir(DATASET_DIRECTORY + "test/")
                old_img_dir_contents = os.listdir(DATASET_DIRECTORY + 'old_images/')
                for img_test in test_dir_contents:
                    img_test_path = os.path.join(DATASET_DIRECTORY + "test/", img_test)
                    if os.path.isfile(img_test_path):
                        os.remove(img_test_path)
                        print(f"Deleted file: {img_test_path}")
                    # Delete the corresponding .xml files in the Annotations dir
                    xml_test_file = os.path.splitext(img_test)[0] + '.xml'
                    xml_test_file_path = os.path.join(ANNOTATIONS_DIRECTORY, xml_test_file)
                    if os.path.exists(xml_test_file_path) and img_test not in old_img_dir_contents:
                        os.remove(xml_test_file_path)
                        print(f"Deleted XML file: {xml_test_file_path}")
                    else:
                        print(f"XML file {xml_test_file_path} does not exist or should not be removed.")
                for img_eval in eval_dir_contents:
                    img_eval_path = os.path.join(DATASET_DIRECTORY + "eval/", img_eval)
                    if os.path.isfile(img_eval_path):
                        os.remove(img_eval_path)
                        print(f"Deleted file: {img_eval_path}")
                    # Delete the corresponding .xml files in the Annotations dir
                    xml_eval_file = os.path.splitext(img_eval)[0] + '.xml'
                    xml_eval_file_path = os.path.join(ANNOTATIONS_DIRECTORY, xml_eval_file)
                    if os.path.exists(xml_eval_file_path) and img_eval not in old_img_dir_contents :
                        os.remove(xml_eval_file_path)
                        print(f"Deleted XML file: {xml_eval_file_path}")
                    else:
                        print(f"XML file {xml_eval_file_path} does not exist or should not be removed.")
                for img_train in train_dir_contents:
                    img_train_path = os.path.join(DATASET_DIRECTORY + "train/", img_train)
                    if os.path.isfile(img_train_path):
                        os.remove(img_train_path)
                        print(f"Deleted file: {img_train_path}")
                    # Delete the corresponding .xml files in the Annotations dir
                    xml_train_file = os.path.splitext(img_train)[0] + '.xml'
                    xml_train_file_path = os.path.join(ANNOTATIONS_DIRECTORY, xml_train_file)
                    if os.path.exists(xml_train_file_path) and img_train not in old_img_dir_contents:
                        os.remove(xml_train_file_path)
                        print(f"Deleted XML file: {xml_train_file_path}")
                    else:
                        print(f"XML file {xml_train_file_path} does not exist or should not be removed.")
            new_imgs = os.listdir(NEW_IMAGES_DIRECTORY)
            for new_img in new_imgs:
                new_img_path = os.path.join(NEW_IMAGES_DIRECTORY, new_img)
                if os.path.isfile(new_img_path):
                    os.remove(new_img_path)
                    print(f"Deleted file: {new_img_path}")
            # Start monitoring the images directory if not already started
            if not self.monitoring_active:
                self.start_directory_monitor()
            # Clear existing new images in data retrieval section
            for child in self.retrieval_flowbox.get_children():
                self.retrieval_flowbox.remove(child)
            # Disable scales
            self.retrieval_freq_scale.set_sensitive(False)
            self.retrieval_limit_scale.set_sensitive(False)
            # Set to False in order to load images in data viz tab when button clicked
            self.images_loaded = False
            # Show the images count label and scrolled window
            self.retrieval_image_count.show()
            self.retrieval_scroll_window.show()
            # Clear any existing images in new images data visualization
            for child in self.new_data_image_box.get_children():
                self.new_data_image_box.remove(child)
            # Add empty image placeholders
            for _ in range(5):
                placeholder = self.create_empty_image_placeholder()
                self.new_data_image_box.pack_start(placeholder, False, False, 5)
            self.new_data_image_box.show_all()
            self.start_image_capture()
            print("Setting label to stop data retrieval")
            self.retrieval_launch_button.set_label("Stop data retrieval")
            self.retrieval_launch_button.set_name("button-stop-retrieval")

        elif self.retrieval_launch_button.get_label() == "Stop data retrieval":
            self.stop_image_capture()
            # Update button label
            self.retrieval_launch_button.set_label("New data retrieval")
            self.retrieval_launch_button.set_name("button")
            # Re-enable scales
            self.retrieval_freq_scale.set_sensitive(True)
            self.retrieval_limit_scale.set_sensitive(True)
            self.retrieval_next_button.set_sensitive(True)

        self.annotate_button.set_sensitive(True)
        self.data_split_button.set_sensitive(True)

    def start_image_capture(self):
        if self.image_capture_active:
            print("Image capture already in progress.")
            return
        self.image_capture_active = True
        self.stop_data_retrieval = False
        self.image_capture_count = 0
        # Start a timer to capture images at the specified frequency
        # Retrieval frequency is expressed as nb of images per 10 seconds
        self.capture_interval_ms = int(1000 / self.retrieval_freq_scale.get_value() * 10)
        self.capture_timer_id = GLib.timeout_add(self.capture_interval_ms, self.capture_image)

    def capture_image(self):
        if self.image_capture_count >= self.retrieval_limit_scale.get_value():
            print("Dataset limit reached. Stopping image capture.")
            self.image_capture_active = False
            self.image_to_send = ""
            self.retrieval_next_button.set_sensitive(True)
            return False  # Stop the timer
        elif self.stop_data_retrieval:
            print("Data retrieval stopped by the user")
            self.retrieval_next_button.set_sensitive(True)
            return False

        self.save_next_frame = True

        if self.image_to_send != "" and self.image_to_send != self.last_sampled_image:
            self.last_sampled_image = self.image_to_send
            self.image_capture_count += 1
        return True  # Continue the timer

    def stop_image_capture(self):
        self.image_capture_active = False
        self.save_next_frame = False
        self.stop_data_retrieval = True

    def start_directory_monitor(self):
        # Set the monitoring flag
        self.monitoring_active = True
        # Create a GFile for the directory
        self.directory = Gio.File.new_for_path(NEW_IMAGES_DIRECTORY)
        # Create a file monitor for the directory
        self.monitor = self.directory.monitor_directory(Gio.FileMonitorFlags.NONE, None)
        # Connect the signal for changes in the directory
        self.monitor.connect("changed", self.on_directory_changed)
        # Start the timeout check
        GLib.timeout_add_seconds(1, self.check_timeout)

    def on_directory_changed(self, monitor, file, other_file, event_type):
        if event_type == Gio.FileMonitorEvent.CREATED:
            self.last_image_time = time.time()
            # A new file was created
            GLib.idle_add(self.add_image_to_gallery, DATASET_DIRECTORY + file.get_path().split("dataset", 1)[1])
        elif event_type == Gio.FileMonitorEvent.DELETED:
            # A file was deleted
            GLib.idle_add(self.remove_image_from_gallery, DATASET_DIRECTORY + file.get_path().split("dataset", 1)[1])

    def add_image_to_gallery(self, image_path):
        # Needed to avoid Pixbuf error
        time.sleep(0.2)
        # Check if the file is an image
        if image_path.lower().endswith(('.png', '.jpg', '.jpeg', '.bmp', '.gif')):
            # Add image to the list if not already added
            if image_path not in self.image_files:
                self.image_files.append(image_path)

                # Load and add the image to the flowbox
                try:
                    pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale(
                        image_path, width=80, height=80, preserve_aspect_ratio=False
                    )
                    image = Gtk.Image.new_from_pixbuf(pixbuf)
                    # Wrap the image in an EventBox
                    event_box = Gtk.EventBox()
                    event_box.add(image)
                    # Connect the click event
                    event_box.connect("button-press-event", self.on_image_clicked, image_path)
                    self.retrieval_flowbox.add(event_box)
                    self.retrieval_flowbox.show_all()
                    # Store the widget in the dictionary
                    self.image_widgets[image_path] = event_box
                except Exception as e:
                    print(f"Error loading image {image_path}: {e}")

                # Update images count label
                self.retrieval_image_count.set_text(f"Retrieved images: {len(self.image_files)}")
                # Check if dataset limit reached
                dataset_limit = int(self.retrieval_limit_scale.get_value())
                current_time = time.time()
                if len(self.image_files) >= dataset_limit:
                    # Stop monitoring
                    self.stop_monitoring()

        return False

    def check_timeout(self):
        current_time = time.time()
        if current_time - self.last_image_time > self.timeout_seconds:
            print("Stopping directory monitoring")
            self.stop_monitoring()
        return self.monitoring_active

    def stop_monitoring(self):
        if self.monitoring_active:
            self.monitor.cancel()
            self.monitoring_active = False
            self.retrieval_launch_button.set_label("New data retrieval")
            self.retrieval_launch_button.set_name("button")
            self.retrieval_launch_button.set_sensitive(True)
            self.retrieval_freq_scale.set_sensitive(True)
            self.retrieval_limit_scale.set_sensitive(True)

    def load_images_to_box(self, directory, box):
        # Ensure the directory exists
        if not os.path.exists(directory):
            print(f"Directory {directory} does not exist.")
            return
        image_files = [
            os.path.join(directory, f)
            for f in os.listdir(directory)
            if f.lower().endswith(('.png', '.jpg', '.jpeg', '.bmp', '.gif'))
        ]
        if image_files:
            # Clear any existing images
            for child in box.get_children():
                box.remove(child)
        for i, image_path in enumerate(image_files):
            try:
                pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale(
                    image_path, width=80, height=80, preserve_aspect_ratio=False
                )
                image = Gtk.Image.new_from_pixbuf(pixbuf)
                # Wrap the image in an EventBox
                event_box = Gtk.EventBox(name="event-box")
                event_box.add(image)
                # Connect the click event
                event_box.connect("button-press-event", self.on_image_clicked, image_path)
                # Store the widget in the dictionary
                self.image_widgets_data_viz[image_path] = event_box
                box.pack_start(event_box, False, False, 5)
            except Exception as e:
                print(f"Error loading image {image_path}: {e}")

        box.show_all()

    def remove_image_from_gallery(self, image_path):
        # Remove image from the list if it exists old_image_files
        if image_path in self.image_files:
            self.image_files.remove(image_path)
            # Remove the widget from the flowbox
            if image_path in self.image_widgets:
                image_widget = self.image_widgets.pop(image_path)
                self.retrieval_flowbox.remove(image_widget)
                image_widget_data_viz = self.image_widgets_data_viz.pop(image_path)
                self.new_data_image_box.remove(image_widget_data_viz)
                # Update images count label
                self.retrieval_image_count.set_text(f"Retrieved images: {len(self.image_files)}")
        return False

    def on_data_viz_button_clicked(self, button):
        # Switch to the Data Visualization tab
        self.data_viz_button.set_sensitive(False)
        old_images = [
            os.path.join(OLD_IMAGES_DIRECTORY, f)
            for f in os.listdir(OLD_IMAGES_DIRECTORY)
            if f.lower().endswith(('.png', '.jpg', '.jpeg', '.bmp'))
        ]
        new_images = [
            os.path.join(NEW_IMAGES_DIRECTORY, f)
            for f in os.listdir(NEW_IMAGES_DIRECTORY)
            if f.lower().endswith(('.png', '.jpg', '.jpeg', '.bmp'))
        ]
        self.old_data_label.set_label(f"Old data: {len(old_images)} images")
        self.new_data_label.set_label(f"New data: {len(new_images)} images")
        self.old_data_label.show()
        self.new_data_label.show()
        # Load images only if not already loaded
        if not self.images_loaded:
            # Load images into the old and new data boxes
            self.load_images_to_box(OLD_IMAGES_DIRECTORY, self.old_data_image_box)
            self.load_images_to_box(NEW_IMAGES_DIRECTORY, self.new_data_image_box)
            self.images_loaded = True

    def on_data_split_button_clicked(self, button):
        split_data(DATASET_DIRECTORY, NEW_IMAGES_DIRECTORY, OLD_IMAGES_DIRECTORY, self.percentage_split_scale, self.percentage_scale, self.data_split_button)
        self.notebook.set_current_page(2)

    def on_retrieval_next_button_clicked(self, button):
        self.notebook.set_current_page(1)
        self.pipeline_preview.set_state(Gst.State.PAUSED)
        self.pipeline_preview.set_state(Gst.State.READY)
        self.data_retrieve_video_box.remove(self.gtkwaylandsink.props.widget)

    def on_viz_next_button_clicked(self, button):
        self.notebook.set_current_page(2)

    def on_viz_back_button_clicked(self, button):
        self.notebook.set_current_page(0)
        self.data_retrieve_video_box.pack_start(self.gtkwaylandsink.props.widget, False, False, 5)
        self.pipeline_preview.set_state(Gst.State.PLAYING)

    def on_annotation_next_button_clicked(self, button):
        self.notebook.set_current_page(3)

    def on_annotation_back_button_clicked(self, button):
        self.notebook.set_current_page(1)

    def on_training_back_button_clicked(self, button):
        self.notebook.set_current_page(2)

    def on_training_next_button_clicked(self, button):
        self.notebook.set_current_page(4)
        if self.gtkwaylandsink.props.widget.get_parent() is not self.inference_video_box:
            self.inference_video_box.pack_start(self.gtkwaylandsink.props.widget, True, False, 0)
        # Start the pipeline
        self.pipeline_preview.set_state(Gst.State.PLAYING)
        self.inference_video_box.show_all()
        video_box_alloc = self.app.overlay_window.video_box.get_allocation()
        video_box_w, video_box_h = video_box_alloc.width, video_box_alloc.height
        self.app.overlay_window.drawing_area.set_size_request(video_box_w, video_box_h)
        self.app.overlay_window.show_all()

    def on_inference_back_button_clicked(self, button):
        self.notebook.set_current_page(3)
        self.pipeline_preview.set_state(Gst.State.PAUSED)
        self.pipeline_preview.set_state(Gst.State.READY)
        self.inference_video_box.hide()
        self.inference_video_box.remove(self.gtkwaylandsink.props.widget)
        self.app.overlay_window.hide()

    def add_image_to_annotation_gallery(self, image_path):
        # This method is called via GLib.idle_add from the background thread
        if image_path.lower().endswith(('.png', '.jpg', '.jpeg', '.bmp', '.gif')):
            if image_path not in self.annotation_image_files:
                self.annotation_image_files.append(image_path)
                try:
                    pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale(
                        image_path, width=80, height=80, preserve_aspect_ratio=False)
                    image = Gtk.Image.new_from_pixbuf(pixbuf)
                    event_box = Gtk.EventBox()
                    event_box.add(image)
                    event_box.connect("button-press-event", self.on_image_clicked, image_path)
                    self.annotation_flowbox.add(event_box)
                    self.annotation_flowbox.show_all()
                    self.annotation_image_widgets[image_path] = event_box
                except Exception as e:
                    print(f"Error loading image {image_path}: {e}")
        return False  # Return False to stop the idle_add callback

    def on_image_clicked(self, widget, event, image_path):
        current_time = time.time()
        last_click_time = self.last_click_times.get(image_path, 0)
        time_diff = current_time - last_click_time
        if time_diff < self.double_click_threshold:
            # Double-click detected
            self.zoom_image(image_path)
            # Reset last click time
            self.last_click_times[image_path] = 0
        else:
            self.last_click_times[image_path] = current_time
        return False

    def on_gesture_multi_press(self, gesture, n_press, x, y, image_path):
        if n_press == 2:  # Check if it's a double-click
            self.zoom_image(image_path)

    def zoom_image(self, image_path):
        # Create a new top-level window or dialog
        zoom_window = Gtk.Window(title="Zoomed Image")
        zoom_window.set_transient_for(self)
        zoom_window.set_modal(True)
        zoom_window.set_position(Gtk.WindowPosition.CENTER)
        # Create a vertical box to hold the image
        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
        zoom_window.add(vbox)
        # Load the full-size image
        try:
            pixbuf = GdkPixbuf.Pixbuf.new_from_file(image_path)
            # Scale the image to fit the window if necessary
            screen = Gdk.Screen.get_default()
            screen_width = screen.get_width()
            screen_height = screen.get_height()
            max_width = int(screen_width * 0.8)
            max_height = int(screen_height * 0.8)

            # Maintain aspect ratio
            width_ratio = max_width / pixbuf.get_width()
            height_ratio = max_height / pixbuf.get_height()
            scale_ratio = min(width_ratio, height_ratio, 1)

            new_width = int(pixbuf.get_width() * scale_ratio)
            new_height = int(pixbuf.get_height() * scale_ratio)

            scaled_pixbuf = pixbuf.scale_simple(
                new_width, new_height, GdkPixbuf.InterpType.BILINEAR
            )

            image = Gtk.Image.new_from_pixbuf(scaled_pixbuf)
        except Exception as e:
            print(f"Error loading image {image_path}: {e}")
            return
        # Wrap the image in an EventBox to capture clicks
        image_event_box = Gtk.EventBox()
        image_event_box.add(image)
        # Connect the click event to close the window
        image_event_box.connect("button-press-event", lambda w, e: zoom_window.destroy())
        # Add the image_event_box to the vbox
        vbox.pack_start(image_event_box, True, True, 0)
        # Connect the "Escape" key to close the window
        zoom_window.connect("key-press-event", self.on_zoom_window_key_press)
        # Show the window with the image
        zoom_window.show_all()
        zoom_window.present()

    def on_zoom_window_key_press(self, widget, event):
        if event.keyval == Gdk.KEY_Escape:
            widget.destroy()

    def launch_annotation_process(self):
        try:
            teacher_model_path = self.args.teacher_model
            dataset_dir = DATASET_DIRECTORY
            subdirs = ["train", "test", "eval", "old_images"]
            # Create folder to store annotated images
            if not os.path.exists(ANNOTATED_IMG_DIRECTORY):
                os.mkdir(ANNOTATED_IMG_DIRECTORY)
                print(f"Directory '{ANNOTATED_IMG_DIRECTORY}' created.\n")
            annotations_dir_path = os.path.join(dataset_dir, "Annotations")
            existing_annotations = os.listdir(ANNOTATED_IMG_DIRECTORY)
            existing_annotations_filenames = [os.path.basename(annotation).split('.')[0] for annotation in existing_annotations]
            # Collect all images to process
            self.images_to_process = []
            for subdir in subdirs:
                images_dir_path = os.path.join(dataset_dir, subdir)
                images = os.listdir(images_dir_path)
                images_basenames = [os.path.basename(image).split('.')[0] for image in images]
                img_files_paths = [os.path.join(images_dir_path, f) for f in images]
                # Check if old_images annotations are already present, we don't annotate it again
                if subdir == "old_images":
                    img_files_paths = [img_path for img_path, image_name in zip(img_files_paths, images_basenames) if image_name not in existing_annotations_filenames]
                self.images_to_process.extend(img_files_paths)
            # Store the total images and initialize processed images count
            self.total_images = len(self.images_to_process)
            self.processed_images = 0
            # Start the timer
            self.start_time = time.perf_counter()
            self.timer_source_id = GLib.timeout_add(100, self.update_timer)
            # Initialize the model
            self.teacher_model = RTDETR(teacher_model_path, conf_thres=0.6, target_labels=[self.target_label])
            # Start processing images using GLib.idle_add
            GLib.idle_add(self.process_next_image, annotations_dir_path, ANNOTATED_IMG_DIRECTORY)

        except Exception as e:
            print(f"Error in annotation process: {e}")
            self.annotate_button.set_sensitive(True)

    def generate_teacher_label_for_image(self, image_path, annotations_dir_path,
                                     annotated_img_dir, update_callback=None):
        img_filename = os.path.basename(image_path)
        img = cv2.imread(image_path)
        # Detect Objects
        boxes, _, class_ids, _, _, _ = self.teacher_model(img)
        # Check for detections
        if type(class_ids) == list or class_ids.shape[0] == 0:  # If there's no detection
            print(f"No object found in {img_filename}")
            # Remove the original image if no objects are detected
            if os.path.exists(image_path):
                os.remove(image_path)
                print(f"Removed {image_path} due to no detections.")
            # Remove the annotated image if it was created
            annotated_img_name = f"annotated_{img_filename}"
            annotated_img_path = os.path.join(annotated_img_dir, annotated_img_name)
            if os.path.exists(annotated_img_path):
                os.remove(annotated_img_path)
                print(f"Removed {annotated_img_path} due to no detections.")
            # Remove the XML annotation file if it was created
            xml_annotation_path = os.path.join(annotations_dir_path, f"{os.path.splitext(img_filename)[0]}.xml")
            if os.path.exists(xml_annotation_path):
                os.remove(xml_annotation_path)
                print(f"Removed {xml_annotation_path} due to no detections.")
            return

        # Save img with bounding boxes
        bb_img = self.teacher_model.draw_detections(img)
        annotated_img_name = f"annotated_{img_filename}"
        annotated_img_path = os.path.join(annotated_img_dir, annotated_img_name)
        cv2.imwrite(annotated_img_path, bb_img)

        # Generate XML annotations
        nb_detections = class_ids.shape[0]
        teacher_class_names = self.class_names
        if "BACKGROUND" in self.class_names:
            teacher_class_names.remove("BACKGROUND")

        # Format the prediction into XML for PASCAL VOC format
        objects = self.teacher_model.format_predictions_for_xml(nb_detections, self.class_names, boxes)
        img_name_without_extension = os.path.splitext(img_filename)[0]
        output_file = f"{annotations_dir_path}/{img_name_without_extension}.xml"
        width = self.teacher_model.img_width
        height = self.teacher_model.img_height
        depth = self.teacher_model.img_depth
        self.teacher_model.generate_voc_xml_annotation(
            width, height, depth, objects, output_file, filename=img_filename, path=image_path)

        # Call the update callback with the annotated image path
        if update_callback:
            update_callback(annotated_img_path)

    def process_next_image(self, annotations_dir_path, annotated_img_dir):
        if self.stop_event.is_set():
            print("Stopping annotation process.")
            self.annotation_back_button.set_sensitive(True)
            self.annotation_next_button.set_sensitive(True)
            return False  # Stop the GLib.idle_add loop
        if self.images_to_process:
            image_path = self.images_to_process.pop(0)
            self.generate_teacher_label_for_image(
                image_path=image_path,
                annotations_dir_path=annotations_dir_path,
                annotated_img_dir=annotated_img_dir,
                update_callback=self.update_annotation_progress
            )
            # Schedule the next image processing
            return True  # Return True to continue processing
        else:
            if not os.path.exists(DATASET_DIRECTORY + "train/"):
                print(f"Directory {DATASET_DIRECTORY}train/ does not exist.")
                contents = []
            else:
                # List the contents of the directory
                contents = os.listdir(DATASET_DIRECTORY + "train/")
            nb_img_to_move = NB_NEW_IMAGES_TO_MOVE_TO_OLD_IMAGES
            images_to_move = random.sample(contents, nb_img_to_move) if nb_img_to_move < len(contents) else random.sample(contents, len(contents))
            for image in images_to_move:
                # Copy the image to the old_images dir
                src_image_path = os.path.join(DATASET_DIRECTORY + "train/", image)
                dest_image_path = os.path.join(OLD_IMAGES_DIRECTORY, image)
                shutil.copy(src_image_path, dest_image_path)
                print(f"Moved image: {src_image_path} to {dest_image_path}")
            # No more images to process
            GLib.idle_add(self.on_annotation_complete)
            return False  # Stop scheduling

    def on_launch_annotate_button_clicked(self, button):
        if self.annotate_button.get_label() == "Launch annotation":
            self.stop_event.clear()
            # Clear existing new images in data retrieval section
            for child in self.annotation_flowbox.get_children():
                self.annotation_flowbox.remove(child)
            self.annotation_back_button.set_sensitive(False)
            self.annotation_next_button.set_sensitive(False)
            # Start the annotation process in a new thread
            annotation_thread = threading.Thread(target=self.launch_annotation_process)
            # Show the images count label and scrolled window
            self.annotation_progress_box.show()
            self.scrolled_window_annotation.show()
            self.scrolled_window_annotation.show()
            annotation_thread.start()
            self.annotate_button.set_label("Stop annotation")
        elif self.annotate_button.get_label() == "Stop annotation":
            self.stop_event.set()  # Signal the thread to stop
            self.annotate_button.set_label("Launch annotation")
            print("Annotation process stopped.")

    def update_timer(self):
        if self.stop_event.is_set():
            return False
        elapsed = int(time.perf_counter() - self.start_time)
        hours = elapsed // 3600
        minutes = (elapsed % 3600) // 60
        seconds = elapsed % 60
        time_str = f"{hours:02d}:{minutes:02d}:{seconds:02d}"
        self.annotation_timer_label.set_text(time_str)
        # Return True to continue calling this function every 1 second
        return True

    def update_annotation_progress(self, image_path):
        # Update the image gallery
        self.add_image_to_annotation_gallery(image_path)
        # Update the processed images count
        self.annotation_status_label.set_markup(f'Annotation in progress ... <b>{self.processed_images} / {self.total_images}</b> images.')
        self.processed_images += 1
        # Calculate progress fraction
        fraction = self.processed_images / self.total_images
        # Update the progress bar
        self.annotation_progress_bar.set_fraction(fraction)
        self.annotation_progress_bar.set_text(f"{int(fraction * 100)}%")
        print(f"Processed {self.processed_images}/{self.total_images} images. Progress: {fraction*100:.1f}%")
        return False

    def on_annotation_complete(self):
        # Update progress bar to 100%
        self.annotation_progress_bar.set_fraction(1.0)
        self.annotation_progress_bar.set_text("100%")
        self.annotation_back_button.set_sensitive(True)
        self.annotation_next_button.set_sensitive(True)
        del self.teacher_model
        # Stop the timer
        GLib.source_remove(self.timer_source_id)
        self.annotation_status_label.set_markup(f'Annotation completed: <b>{self.total_images}</b> images.')
        self.annotate_button.set_label("Launch annotation")
        print("Annotation completed.")
        return False

    #----------------------------------------#
    #-----------    Training       ----------#
    #----------------------------------------#
    def on_launch_training_clicked(self, button):
        # Disable evaluation button during training
        self.launch_evaluation_button.set_sensitive(False)
        self.training_status_label.set_text("Training in progress...")
        generate_default_loss_plot()
        self.loss_curve_image.set_from_file("loss_curve.png")
        generate_default_map_plot()
        self.map_curve_image.set_from_file("map_curve.png")
        # Reset loss and accuracy lists
        self.loss_values = []
        self.map_values = []
        # Get hyperparameters from the scales
        num_epochs = int(self.training_epochs_scale.get_value())
        learning_rate = self.training_lr_scale.get_value()
        batch_size = int(self.training_batch_scale.get_value())
        self.epoch_label.set_text("Epoch 0 / " + str(num_epochs))
        # Start the elapsed time timer
        self.training_start_time = time.perf_counter()
        self.training_timer_source_id = GLib.timeout_add(100, self.update_training_timer)

        if self.launch_training_button.get_label() == "Launch training" or self.launch_training_button.get_label() == "Launch new training":
            self.stop_event.clear()
            training_thread = threading.Thread(
                target=self.perform_training_wrapper,
                args=(num_epochs, learning_rate, batch_size)
            )
            training_thread.start()
            self.launch_training_button.set_label("Stop training")
        elif self.launch_training_button.get_label() == "Stop training":
            self.stop_event.set()
            self.launch_training_button.set_label("Launch training")
            self.training_status_label.set_text("Training stopped")
            self.launch_evaluation_button.set_sensitive(True)
            print("Training process stopped.")

    def update_training_timer(self):
        if self.stop_event.is_set():
            return False
        elapsed = int(time.perf_counter() - self.training_start_time)
        hours = elapsed // 3600
        minutes = (elapsed % 3600) // 60
        seconds = elapsed % 60
        time_str = f"Elapsed time: {hours:02d}:{minutes:02d}:{seconds:02d}"
        self.training_timer_label.set_text(time_str)
        # Return True to continue calling this function every 1 second
        return True

    def perform_training_wrapper(self, num_epochs, learning_rate, batch_size):
        try:
            self.training_epochs_scale.set_sensitive(False)
            self.training_batch_scale.set_sensitive(False)
            self.training_lr_scale.set_sensitive(False)
            self.training_back_button.set_sensitive(False)
            self.training_next_button.set_sensitive(False)
            # Call perform_training with callbacks
            self.perform_training(
                artifacts_dir_path=self.args.training_artifacts_path,
                dataset_path=DATASET_DIRECTORY,
                labels_file_path=self.args.label_file,
                num_epochs=num_epochs,
                lr=learning_rate,
                batch_size=batch_size,
                img_size=300
            )
        except Exception as e:
            self.stop_event.set()  # Signal the thread to stop
            self.training_back_button.set_sensitive(True)
            self.training_next_button.set_sensitive(True)
            GLib.idle_add(self.on_training_error, str(e))
            return
        if self.stop_event.is_set():
            self.training_back_button.set_sensitive(True)
            self.training_next_button.set_sensitive(True)
            GLib.idle_add(self.on_training_stopped)
            return
        # Training is complete, update UI
        GLib.idle_add(self.on_training_complete)

    def on_training_error(self, error_message):
        self.launch_training_button.set_label("Launch training")
        self.training_status_label.set_text("An error occurred")
        print(f"An error occurred during training: {error_message}")
        self.training_epochs_scale.set_sensitive(True)
        self.training_batch_scale.set_sensitive(True)
        self.training_lr_scale.set_sensitive(True)
        # Reset the loss and mAP plots if the training is interrupted
        generate_default_loss_plot()
        self.loss_curve_image.set_from_file("loss_curve.png")
        generate_default_map_plot()
        self.map_curve_image.set_from_file("map_curve.png")

    def on_training_stopped(self):
        self.training_status_label.set_text("Training stopped...")
        self.launch_training_button.set_label("Launch new training")
        self.training_epochs_scale.set_sensitive(True)
        self.training_batch_scale.set_sensitive(True)
        self.training_lr_scale.set_sensitive(True)
        # Reset the loss and mAP plots if the training stopped
        generate_default_loss_plot()
        self.loss_curve_image.set_from_file("loss_curve.png")
        generate_default_map_plot()
        self.map_curve_image.set_from_file("map_curve.png")

    def perform_training(self, artifacts_dir_path, dataset_path, labels_file_path, num_epochs=5, lr=0.005, batch_size=4, img_size=300):
        # Instantiate the training session by defining the checkpoint state, module, and optimizer
        # The checkpoint state contains the state of the model parameters at any given time.
        checkpoint_state = orttraining.CheckpointState.load_checkpoint(f"{artifacts_dir_path}checkpoint")
        model = orttraining.Module(f"{artifacts_dir_path}training_model.onnx",
                                    checkpoint_state,
                                    f"{artifacts_dir_path}eval_model.onnx",
                                  )
        optimizer = orttraining.Optimizer(f"{artifacts_dir_path}optimizer_model.onnx", model)
        optimizer.set_learning_rate(learning_rate=lr)
        config = mobilenetv2_ssd_config

        # Define training and valid set transformations to be applied
        train_transform = TrainSetTransform(img_size, config.image_mean, config.image_std)
        valid_transform = TestSetTransform(img_size, config.image_mean, config.image_std)

        # Generate the SSD MobileNet V2 anchors for training
        anchors = generate_ssd_anchors(config.specs, img_size)
        target_transform = AnchorsMatcher(anchors, config.center_variance, config.size_variance, 0.5)
        train_dataset = CustomDataset(dataset_path, transform=train_transform, target_transform=target_transform, dataset_type="train", labels_file=self.args.label_file)
        valid_dataset = CustomDataset(dataset_path, transform=valid_transform, target_transform=target_transform, dataset_type="test", labels_file=self.args.label_file)

        # Define the train set and the validation set dataloaders
        print("Train dataset size: {}".format(len(train_dataset)))
        print("Validation dataset size: {}".format(len(valid_dataset)))
        train_loader = DataLoader(train_dataset, batch_size, shuffle=True)
        valid_loader = DataLoader(valid_dataset, batch_size, shuffle=False)

        # Define training and validation functions with UI updates
        def train_with_ui_updates(loader, model, optimizer, epoch, num_epochs):
            model.train()
            train_loss = []
            for i, data in enumerate(loader):
                if self.stop_event.is_set():
                    self.training_status_label.set_text("Training stopped...")
                    print("Stopping training process.")
                    return
                images, boxes, labels = data
                loss, confs, _ = model(np.array(images), np.array(labels).astype(np.float32), np.array(boxes))
                optimizer.step()
                model.lazy_reset_grad()
                train_loss.append(loss.item())
            return sum(train_loss) / len(train_loss)

        def test(loader, model):
            model.eval()
            valid_losses = []
            results = []
            for i, data in enumerate(loader):
                if self.stop_event.is_set():
                    self.training_status_label.set_text("Training stopped...")
                    print("Stopping training process.")
                    return
                images, gt_boxes, labels = data
                valid_loss, probs, pred_boxes = model(np.array(images), np.array(labels).astype(np.float32), np.array(gt_boxes))
                valid_losses.append(valid_loss.item())
                # Loop over each image in the batch
                for j in range(np.array(pred_boxes).shape[0]):
                    boxes, scores, pred_labels = process_output(scores=np.expand_dims(probs[j], axis=0),
                                                                boxes=np.expand_dims(pred_boxes[j], axis=0),
                                                                anchors=anchors, iou_threshold=0.45,
                                                                img_width=img_size, img_height=img_size)
                    pred_labels = np.array(pred_labels)
                    indexes = np.ones((pred_labels.shape[0], 1)) * (j + i * batch_size)
                    results.append(np.concatenate([
                        indexes.reshape(-1, 1),
                        pred_labels.reshape(-1, 1).astype(np.float32),
                        scores.reshape(-1, 1),
                        boxes + 1.0  # matlab's indexes start from 1
                    ], axis=1))

            map = self.perform_validation_during_training(dataset_path=dataset_path, labels_file_path=labels_file_path, results=results, dataset=valid_dataset)

            self.map_values.append(map)
            # Update loss values and UI every epoch
            avg_loss = sum(valid_losses) / len(valid_losses)
            self.loss_values.append(avg_loss)
            # Update loss curve
            GLib.idle_add(self.update_loss_curve)
            GLib.idle_add(self.update_map_curve)
            # Update epoch label
            GLib.idle_add(self.update_epoch_label, epoch + 1, num_epochs)

            return avg_loss

        last_epoch = -1
        print(f"Start training from epoch {last_epoch + 2}.")
        # Main training loop (training + evaluation) for num_epochs
        for epoch in range(last_epoch + 1, num_epochs):
            if self.stop_event.is_set():
                self.training_status_label.set_text("Training stopped...")
                return
            # Compute the training loss after forward+backward pass
            train_loss = train_with_ui_updates(loader=train_loader,
                                               model=model,
                                               optimizer=optimizer,
                                               epoch=epoch,
                                               num_epochs=num_epochs)
            if self.stop_event.is_set():
                self.training_status_label.set_text("Training stopped...")
                return

            # Compute the evaluation loss after forward pass
            val_loss = test(loader=valid_loader, model=model)

            if self.stop_event.is_set():
                self.training_status_label.set_text("Training stopped...")
                return
            print(f"Epoch: {epoch + 1} / {num_epochs}, Training Loss: {train_loss:.4f}, Validation Loss: {val_loss:.4f}")

        # Save the checkpoint
        orttraining.CheckpointState.save_checkpoint(checkpoint_state, f"{artifacts_dir_path}checkpoint")
        print(f"Checkpoint saved to: {artifacts_dir_path}checkpoint\n")

        # Folder to store onnx models
        inference_models_dir = INFERENCE_MODELS_DIRECTORY
        if not os.path.exists(inference_models_dir):
            os.mkdir(inference_models_dir)
            print(f"Directory '{inference_models_dir}' created.\n")
        else:
            print(f"Directory '{inference_models_dir}' already exists.\n")

        # If there's a trained model available, we keep it as an old model called "inference_model.onnx"
        if os.path.exists(f"{inference_models_dir}updated_inference_model.onnx"):
            # Remove the old inference_model.onnx file
            old_model_path = f"{INFERENCE_MODELS_DIRECTORY}inference_model.onnx"
            if os.path.exists(old_model_path):
                os.remove(old_model_path)
                print(f"Removed old model: {old_model_path}")

            # Copy updated_inference_model.onnx to inference_model.onnx
            updated_model_path = f"{INFERENCE_MODELS_DIRECTORY}updated_inference_model.onnx"
            new_model_path = f"{INFERENCE_MODELS_DIRECTORY}inference_model.onnx"
            shutil.copy(updated_model_path, new_model_path)
            print(f"Copied {updated_model_path} to {new_model_path}")

        if os.path.exists(f"{inference_models_dir}inference_model.onnx"):
            model.export_model_for_inferencing(
                f"{inference_models_dir}updated_inference_model.onnx", ["confs", "out_boxes"])
            print(
                f"Model exported to: {inference_models_dir}updated_inference_model.onnx")
            print()
        else:
            model.export_model_for_inferencing(
                f"{inference_models_dir}inference_model.onnx", ["confs", "out_boxes"])
            print(
                f"Model exported to: {inference_models_dir}inference_model.onnx")
            print()

        #----------------------------------------#
        #---------     Quantization    ----------#
        #----------------------------------------#
        calib_dir_path = dataset_path + "train/"
        nb_images_calib = self.args.nb_img_calib
        inference_models_dir = INFERENCE_MODELS_DIRECTORY
        self.training_status_label.set_text("Quantization in progress...")
        if os.path.exists(f"{inference_models_dir}updated_inference_model.onnx"):
            float_model_path = f"{inference_models_dir}updated_inference_model.onnx"
        else:
            float_model_path = f"{inference_models_dir}inference_model.onnx"
        out_model_name = "ssd_preprocessed"
        # Preprocessing before static per tensor quantization
        quant_pre_process(float_model_path, f"{inference_models_dir}/{out_model_name}.onnx")
        # Static per tensor quantization
        in_model_name = "ssd_preprocessed"
        out_model_name = "ssd_static_quant"
        datareader = SSDDataReader(calib_dir_path, float_model_path, nb_images=nb_images_calib)
        quantize_static(f"{inference_models_dir}/{in_model_name}.onnx",
                        f"{inference_models_dir}/{out_model_name}.onnx",
                        calibration_data_reader=datareader,
                        activation_type=QuantType.QInt8,
                        weight_type=QuantType.QInt8,
                        quant_format=QuantFormat.QDQ,
                        per_channel=True, reduce_range=True)
        print("Quantization done!")
        model = onnx.load(f"{inference_models_dir}/{out_model_name}.onnx")
        make_dim_param_fixed(model.graph, "batch", 1)
        # update the output shapes to make them fixed if possible.
        fix_output_shapes(model)
        # Rename existing model to old model
        if os.path.exists(f"{inference_models_dir}/{out_model_name}_fixed.onnx"):
            os.rename(f"{inference_models_dir}/{out_model_name}_fixed.onnx", f"{inference_models_dir}/old_quant_model.onnx")
        onnx.save(model, f"{inference_models_dir}/{out_model_name}_fixed.onnx")
        self.app.nn = ssd_mobilenetv2(
                f"{inference_models_dir}/{out_model_name}_fixed.onnx",
                labels_file_path, conf_thres=args.conf_threshold,
                iou_thres=args.iou_threshold, hw_acceleration=False)
        self.training_back_button.set_sensitive(True)
        self.training_next_button.set_sensitive(True)


    # -------------------------------------------------------------------------
    # Plot update methods – called via GLib.idle_add so they run in the GTK main loop.
    # -------------------------------------------------------------------------
    def update_loss_curve(self):
        """
        Callback to update loss curve plot
        """
        loss_curve_image = self.loss_curve_image
        self.loss_curve_image = update_loss_curve_plot(self.loss_values, self.training_epochs_scale, loss_curve_image)
        return False  # Stop further idle calls

    def update_map_curve(self):
        """
        Callback to update mAP curve plot
        """
        map_curve_image = self.map_curve_image
        self.map_curve_image = update_map_curve_plot(self.map_values, self.training_epochs_scale, map_curve_image)
        return False

    def on_eval_switch_activated(self, switch, state):
        """
        Callback function triggered when the switch state changes.

        Args:
            switch: The Gtk.Switch widget that emitted the signal.
            state: A boolean indicating the new state of the switch (True for ON, False for OFF).
        """
        if state:
            print("Switch turned ON")
            pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale('evaluation_bar_plot_old.png',
                                                            width=int(FIGSIZE_EVAL[0]*DPI),
                                                            height=int(FIGSIZE_EVAL[1]*DPI),
                                                            preserve_aspect_ratio=False)
        else:
            print("Switch turned OFF")
            pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale('evaluation_bar_plot_new.png',
                                                            width=int(FIGSIZE_EVAL[0]*DPI),
                                                            height=int(FIGSIZE_EVAL[1]*DPI),
                                                            preserve_aspect_ratio=False)
        self.evaluation_bar_plot_image.set_from_pixbuf(pixbuf)
        return False

    def update_evaluation_bar_plot(self, eval_data):
        """
        Callback to update evaluation bar plot
        """
        #evaluation_bar_plot_image = self.evaluation_bar_plot_image
        update_evaluation_plot(eval_data)
        if self.old_data_switch.get_active():
            pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale('evaluation_bar_plot_old.png',
                                                            width=int(FIGSIZE_EVAL[0]*DPI),
                                                            height=int(FIGSIZE_EVAL[1]*DPI),
                                                            preserve_aspect_ratio=False)
        else:
            pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale('evaluation_bar_plot_new.png',
                                                            width=int(FIGSIZE_EVAL[0]*DPI),
                                                            height=int(FIGSIZE_EVAL[1]*DPI),
                                                            preserve_aspect_ratio=False)

        # Update the Gtk.Image with the selected pixbuf
        self.evaluation_bar_plot_image.set_from_pixbuf(pixbuf)
        return False

    def update_epoch_label(self, current_epoch, total_epochs):
        """
        Callback to update the epochs evolution
        """
        self.epoch_label.set_text(f"Epoch {current_epoch} / {total_epochs}")
        return False

    def on_training_complete(self):
        """
        Callback to check if the training is complete
        """
        # Update the training status label
        self.training_status_label.set_text("Training and quantization complete!")
        # Stop the timer
        GLib.source_remove(self.training_timer_source_id)
        # Re-enable the "Launch training" button if desired
        self.launch_training_button.set_sensitive(True)
        self.launch_evaluation_button.set_sensitive(True)
        self.launch_training_button.set_label("Launch new training")
        # Optionally, display a message
        print("Training and quantization completed.")
        # Show the evaluation box
        self.evaluation_hbox.show()
        self.training_epochs_scale.set_sensitive(True)
        self.training_batch_scale.set_sensitive(True)
        self.training_lr_scale.set_sensitive(True)

        return False

    #---------------------------------------#
    #---------     Evaluation      ---------#
    #---------------------------------------#
    def on_launch_evaluation_clicked(self, button):
        if self.launch_evaluation_button.get_label() == "Launch evaluation":
            self.stop_event.clear()
            GLib.idle_add(self.evaluation_status_label.set_text, "Evaluation ongoing...")

            generate_default_evaluation_plot()  # Create a default empty evaluation bar plot
            self.evaluation_bar_plot_image.set_from_file("evaluation_bar_plot.png")

            evaluation_thread = threading.Thread(target=self.perform_evaluation_wrapper)
            evaluation_thread.start()
            self.launch_evaluation_button.set_label("Stop evaluation")
        elif self.launch_evaluation_button.get_label() == "Stop evaluation":
            self.stop_event.set()
            self.launch_evaluation_button.set_label("Launch evaluation")
            self.evaluation_status_label.set_text("Evaluation stopped")
            print("Evaluation process stopped.")

    def perform_evaluation_wrapper(self):
        def update_eval_data(eval_data):
            GLib.idle_add(self.update_evaluation_bar_plot, eval_data)

        def check_stop_event():
            if self.stop_event.is_set():
                self.training_back_button.set_sensitive(True)
                self.training_next_button.set_sensitive(True)
                GLib.idle_add(self.on_evaluation_stopped)
                return True
            return False

        GLib.idle_add(self.evaluation_status_label.set_text, "Evaluation ongoing...")
        self.training_back_button.set_sensitive(False)
        self.training_next_button.set_sensitive(False)
        dataset_eval_path = DATASET_DIRECTORY
        labels_file_path = self.args.label_file
        initial_model_path = f"{INFERENCE_MODELS_DIRECTORY}inference_model.onnx"
        updated_model_path = f"{INFERENCE_MODELS_DIRECTORY}updated_inference_model.onnx" if os.path.exists(f"{INFERENCE_MODELS_DIRECTORY}updated_inference_model.onnx") else initial_model_path
        quant_model_path = f"{INFERENCE_MODELS_DIRECTORY}ssd_static_quant_fixed.onnx"

        eval_data = {
            'new_before': None,
            'new_after': None,
            'old_before': None,
            'old_after': None,
            'quant_old': None,
            'quant_new': None,
        }

        data_types = ["new", "old"]
        stages = ["before", "after"]

        for data_type in data_types:
            if data_type == "old" and not os.listdir(OLD_IMAGES_DIRECTORY):
                print("Old images directory is empty.")
                continue

            for stage in stages:
                if stage == "before":
                    model_path = initial_model_path
                elif stage == "after":
                    model_path = updated_model_path

                print(f"Evaluating {data_type} data {stage} training...")
                map_result = self.perform_evaluation(dataset_path=dataset_eval_path,
                                                     labels_file_path=labels_file_path,
                                                     inference_model_path=model_path,
                                                     data_type=data_type, model_stage=stage)
                if check_stop_event(): return
                eval_data[f"{data_type}_{stage}"] = map_result
                update_eval_data(eval_data)

        # Evaluate quantized model
        for data_type in data_types:
            if data_type == "old" and not os.listdir(OLD_IMAGES_DIRECTORY):
                continue

            print(f"Evaluating {data_type} data with quantized model...")
            map_result = self.perform_evaluation(dataset_path=dataset_eval_path,
                                                 labels_file_path=labels_file_path,
                                                 inference_model_path=quant_model_path,
                                                 data_type=data_type, model_stage="after")
            if check_stop_event(): return
            eval_data[f"quant_{data_type}"] = map_result
            update_eval_data(eval_data)

        print(f"Eval data: {eval_data}")
        GLib.idle_add(self.update_evaluation_bar_plot, eval_data)

        GLib.idle_add(self.evaluation_status_label.set_text, "Evaluation complete!")
        self.training_back_button.set_sensitive(True)
        self.training_next_button.set_sensitive(True)
        self.launch_evaluation_button.set_sensitive(True)
        self.launch_evaluation_button.set_label("Launch evaluation")

    def on_evaluation_stopped(self):
        self.evaluation_status_label.set_text("Evaluation stopped...")
        self.launch_evaluation_button.set_label("Launch evaluation")

    def perform_evaluation(self, dataset_path, labels_file_path, inference_model_path, data_type, model_stage, iou_threshold=0.5):
        iou_threshold = self.args.iou_threshold
        test_data_empty = os.listdir(dataset_path + "/test")
        hw_acceleration =  False
        if self.stop_event.is_set() or len(test_data_empty) == 0:
            self.training_back_button.set_sensitive(True)
            self.training_next_button.set_sensitive(True)
            GLib.idle_add(self.on_evaluation_stopped)
            return
        try:
            if inference_model_path.endswith("quant_fixed.onnx"):
                hw_acceleration = True
            ssd_detector = ssd_mobilenetv2(inference_model_path, labels_file_path,
                                           conf_thres=0.01, iou_thres=iou_threshold,
                                           hw_acceleration=hw_acceleration)
            class_names = [name.strip() for name in open(labels_file_path).readlines()]

            if "BACKGROUND" not in class_names:
                class_names.insert(0, "BACKGROUND")

            if data_type == "old":
                dataset = CustomDataset(dataset_path, dataset_type="old_images", labels_file=self.args.label_file)
            elif data_type == "test":
                dataset = CustomDataset(dataset_path, dataset_type=data_type, labels_file=self.args.label_file)
            else:
                dataset = CustomDataset(dataset_path, dataset_type="eval", labels_file=self.args.label_file)

            true_case_stat, all_gt_boxes, all_difficult_cases = group_annotation_by_class(dataset)

            results = []
            preprocess_times = []
            inference_times = []
            postprocess_times = []
            len_dataset = len(dataset)

            for i in range(len_dataset):
                print(f"Processing {data_type} data {model_stage} training: image {i + 1} / {len_dataset}")
                if self.stop_event.is_set() or len(test_data_empty) == 0:
                    self.training_back_button.set_sensitive(True)
                    self.training_next_button.set_sensitive(True)
                    GLib.idle_add(self.on_evaluation_stopped)
                    return

                image, name = dataset.get_image_with_name(i)

                # Detect objects from the input image
                boxes, probs, labels, preprocess_time, inference_time, postprocessing_utils_time = ssd_detector(image)
                preprocess_times.append(preprocess_time)
                inference_times.append(inference_time)
                postprocess_times.append(postprocessing_utils_time)
                labels = np.array(labels)
                indexes = np.ones((labels.shape[0], 1)) * i
                results.append(np.concatenate([indexes.reshape(-1, 1),
                                               labels.reshape(-1, 1).astype(np.float32),
                                               probs.reshape(-1, 1),
                                               boxes + 1.0  # matlab's indexes start from 1
                                               ], axis=1))
            if len(results) > 0:
                results = np.concatenate(results)
            else:
                results = np.array([])

            for class_index, class_name in enumerate(class_names):
                if class_index == 0:
                    continue  # ignore background
                prediction_path = f"{dataset_path}det_test_{class_name}.txt"
                with open(prediction_path, "w") as f:
                    if results.size > 0:
                        sub = results[results[:, 1] == class_index, :]
                        for i in range(sub.shape[0]):
                            prob_box = sub[i, 2:]
                            image_id = dataset.ids[int(sub[i, 0])]
                            print(image_id + " " + " ".join([str(v) for v in prob_box]),file=f)
            aps = []
            print("\nAverage Precision Per-class:")
            print(f"Class names: {class_names}")
            for class_index, class_name in enumerate(class_names):
                if class_index == 0:
                    continue
                prediction_path = f"{dataset_path}det_test_{class_name}.txt"
                ap = compute_mAP_per_class(
                    true_case_stat[class_index],
                    all_gt_boxes[class_index],
                    all_difficult_cases[class_index],
                    prediction_path,
                    iou_threshold
                )
                aps.append(ap)
                print(f"{class_name}: {ap * 100:.2f}%")
            mean_ap = sum(aps) / len(aps) if len(aps) > 0 else 0.0
            print(f"\nMean Average Precision (mAP): {mean_ap * 100:.2f}%\n")
            return mean_ap * 100  # Return mAP percentage
        except Exception as e:
            print(f"An Exception occurred during evaluation: {e}")
            self.launch_evaluation_button.set_sensitive(True)
            self.stop_event.set()  # Signal the thread to stop
            self.training_back_button.set_sensitive(True)
            self.training_next_button.set_sensitive(True)
            GLib.idle_add(self.on_evaluation_error, str(e))
            return

    def on_evaluation_error(self, error_message):
        self.launch_evaluation_button.set_label("Launch evaluation")
        self.evaluation_status_label.set_text("An error occurred")
        print(f"An error occurred during evaluation: {error_message}")

    def perform_validation_during_training(self, dataset_path, labels_file_path, results, dataset, iou_threshold=0.45):
        iou_threshold = self.args.iou_threshold
        try:
            class_names = [name.strip() for name in open(labels_file_path).readlines()]
            if "BACKGROUND" not in class_names:
                class_names.insert(0, "BACKGROUND")
            true_case_stat, all_gt_boxes, all_difficult_cases = group_annotation_by_class(dataset)
            if len(results) > 0:
                results = np.concatenate(results)
            else:
                results = np.array([])
            for class_index, class_name in enumerate(class_names):
                if class_index == 0:
                    continue  # ignore background
                prediction_path = f"{dataset_path}det_val_{class_name}.txt"
                with open(prediction_path, "w") as f:
                    if results.size > 0:
                        sub = results[results[:, 1] == class_index, :]
                        for i in range(sub.shape[0]):
                            if i == 0:
                                print(max(sub[:, 0]))
                            prob_box = sub[i, 2:]
                            image_id = dataset.ids[int(sub[i, 0])]
                            print(image_id + " " + " ".join([str(v) for v in prob_box]),file=f)
            aps = []
            print("\nAverage Precision Per-class:")
            print(f"Class names: {class_names}")
            for class_index, class_name in enumerate(class_names):
                if class_index == 0:
                    continue
                prediction_path = f"{dataset_path}det_val_{class_name}.txt"
                ap = compute_mAP_per_class(
                    true_case_stat[class_index],
                    all_gt_boxes[class_index],
                    all_difficult_cases[class_index],
                    prediction_path,
                    iou_threshold
                )
                aps.append(ap)
                print(f"{class_name}: {ap * 100:.2f}%")
            mean_ap = sum(aps) / len(aps) if len(aps) > 0 else 0.0
            print(f"\nMean Average Precision (mAP): {mean_ap * 100:.2f}%\n")
            return mean_ap * 100  # Return mAP percentage
        except Exception as e:
            print(f"An Exception occurred during evaluation: {e}")
            self.launch_evaluation_button.set_sensitive(True)
            self.stop_event.set()  # Signal the thread to stop
            GLib.idle_add(self.on_evaluation_error, str(e))
            return


class OverlayWindow(Gtk.Window):
    """
    This class handles all the functions necessary
    to display overlayed information on top of the
    video stream and in side information boxes of
    the GUI
    """
    def __init__(self,args,app):
        """
        Setup instances of class and shared variables
        usefull for the application
        """
        Gtk.Window.__init__(self)
        self.app = app
        self.set_ui_param()
        self.exit_icon_path = RESOURCES_DIRECTORY + f"exit_{self.ui_icon_exit_width}x{self.ui_icon_exit_height}.png"
        self.overlay_ui_creation(args)


    def exit_icon_cb(self,eventbox, event):
        """
        Exit callback to close application
        """
        self.destroy()
        Gtk.main_quit()

    def bboxes_colors(self):
          """
          Create a list of unique color for each labels
          """
          bbcolor_list = []
          labels = self.app.nn.get_labels()
          for i in range(len(labels)):
              bbcolor = (random.random(), random.random(), random.random())
              bbcolor_list.append(bbcolor)
          return bbcolor_list

    def set_ui_param(self):
          """
          Setup all the UI parameter depending
          on the screen size
          """
          if self.app.window_height > self.app.window_width :
              window_constraint = self.app.window_width
          else :
              window_constraint = self.app.window_height

          self.ui_cairo_font_size = 23
          self.ui_cairo_font_size_label = 37
          self.ui_icon_exit_size = '50'
          self.ui_icon_st_size = '160'
          if window_constraint <= 272:
                 # Display 480x272
                 self.ui_icon_exit_width = 25
                 self.ui_icon_exit_height = 25
                 self.ui_cairo_font_size = 11
                 self.ui_cairo_font_size_label = 18
                 self.ui_icon_exit_size = '25'
                 self.ui_icon_st_size = '52'
          elif window_constraint <= 480:
                 #Display 800x480
                 self.ui_icon_exit_width = 50
                 self.ui_icon_exit_height = 50
                 self.ui_cairo_font_size = 16
                 self.ui_cairo_font_size_label = 29
                 self.ui_icon_exit_size = '50'
                 self.ui_icon_st_size = '80'
          elif window_constraint <= 600:
                 #Display 1024x600
                 self.ui_icon_exit_width = 50
                 self.ui_icon_exit_height = 50
                 self.ui_cairo_font_size = 19
                 self.ui_cairo_font_size_label = 32
                 self.ui_icon_exit_size = '50'
                 self.ui_icon_st_size = '120'
          elif window_constraint <= 720:
                 #Display 1280x720
                 self.ui_icon_exit_width = 50
                 self.ui_icon_exit_height = 50
                 self.ui_cairo_font_size = 23
                 self.ui_cairo_font_size_label = 38
                 self.ui_icon_exit_size = '50'
                 self.ui_icon_st_size = '160'
          elif window_constraint <= 1080:
                 #Display 1920x1080
                 self.ui_icon_exit_width = 50
                 self.ui_icon_exit_height = 50
                 self.ui_cairo_font_size = 33
                 self.ui_cairo_font_size_label = 48
                 self.ui_icon_exit_size = '50'
                 self.ui_icon_st_size = '160'

    def overlay_ui_creation(self,args):
        """
        Setup the Gtk UI of the overlay window
        """
        # remove the title bar
        self.set_decorated(False)

        allocation = self.app.main_window.inference_video_box.get_allocation()

        self.drawing_width = self.app.main_window.gtkwaylandsink.props.widget.get_allocated_width()
        self.drawing_height = self.app.main_window.gtkwaylandsink.props.widget.get_allocated_height()
        self.first_drawing_call = True
        GdkDisplay = Gdk.Display.get_default()
        monitor = Gdk.Display.get_monitor(GdkDisplay, 0)
        workarea = Gdk.Monitor.get_workarea(monitor)
        GdkScreen = Gdk.Screen.get_default()
        provider = Gtk.CssProvider()
        css_path = RESOURCES_DIRECTORY + "ODL_style.css"
        self.set_name("overlay_window")
        provider.load_from_path(css_path)
        Gtk.StyleContext.add_provider_for_screen(GdkScreen, provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        self.maximize()
        self.screen_width = workarea.width
        self.screen_height = workarea.height
        print(self.screen_width, self.screen_height)

        self.set_position(Gtk.WindowPosition.CENTER)
        self.connect('destroy', Gtk.main_quit)

        # Create the label
        self.inf_time_label = Gtk.Label(label=f"Inference time: {self.app.nn_inference_time:.0f}", name="inf-label")
        self.inf_time_label.set_justify(Gtk.Justification.CENTER)
        self.inf_time_label.set_halign(Gtk.Align.CENTER)
        self.inf_time_label.set_valign(Gtk.Align.START)

        self.inf_fps_label = Gtk.Label(label=f"Inference fps: {self.app.nn_inference_fps:.0f}", name="inf-label")
        self.inf_fps_label.set_justify(Gtk.Justification.CENTER)
        self.inf_fps_label.set_halign(Gtk.Align.CENTER)
        self.inf_fps_label.set_valign(Gtk.Align.START)

        # setup video box containing a transparent drawing area
        # to draw over the video stream
        self.video_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        self.video_box.set_name("gui_overlay_video")
        self.video_box.set_app_paintable(True)
        self.drawing_area = Gtk.DrawingArea()

        self.drawing_area.connect("draw", self.drawing)
        self.drawing_area.set_name("overlay_draw")
        self.drawing_area.set_app_paintable(True)
        self.video_box.pack_start(self.drawing_area, True, True, 0)

        # Inference tab navigation button
        inference_nav_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=5, valign=Gtk.Align.CENTER, name="nav-box", height_request=50)
        inference_buttons_grid = Gtk.Grid(margin_top=15, halign=Gtk.Align.CENTER, column_homogeneous=True)
        inference_buttons_grid.set_row_spacing(20)
        inference_buttons_grid.set_column_spacing(20)
        # Old model inference button
        old_model_button = Gtk.Button(label="Old model inference", name="button")
        old_model_button.set_halign(Gtk.Align.CENTER)
        old_model_button.connect("clicked", self.on_old_model_inference_button_clicked)
        self.old_model_button = old_model_button
        # Retrained model inference button
        retrained_model_button = Gtk.Button(label="Retrained model inference", name="button")
        retrained_model_button.set_halign(Gtk.Align.CENTER)
        retrained_model_button.connect("clicked", self.on_retrained_model_inference_button_clicked)
        self.retrained_model_button = retrained_model_button
        # Inference tab back button
        inference_back_button = Gtk.Button(label="< Back", name="button", margin_start=20, vexpand=False, valign=Gtk.Align.CENTER)
        inference_back_button.connect("clicked", self.on_inference_back_button_clicked)
        inference_buttons_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, valign=Gtk.Align.CENTER, margin_start=80, homogeneous=True, halign=Gtk.Align.CENTER)
        # Inference tab navigation box buttons
        inference_buttons_box.pack_start(old_model_button, True, False, 5)
        inference_buttons_box.pack_start(retrained_model_button, True, False, 5)
        inference_nav_box.pack_start(inference_back_button, False, False, 5)
        inference_nav_box.pack_start(inference_buttons_box, False, False, 0)


        ##############  Right container ##############
        # Create the exit icon widget
        exit_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=0)
        exit_icon_main = Gtk.Image()
        exit_icon_event_box = Gtk.EventBox()
        exit_icon_event_box.add(exit_icon_main)

        def exit_icon_cb(widget, event):
            print("Exit button clicked")
            Gtk.main_quit()

        exit_icon_event_box.connect("button_press_event", exit_icon_cb)
        exit_icon_main.set_from_file(self.exit_icon_path)
        exit_box.pack_end(exit_icon_event_box, False, False, 2)

        self.right_container = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0)
        self.right_container.set_homogeneous(False)
        self.right_container.pack_start(exit_box, False, False, 0)

        # Create a grid container for the volume buttons and text labels
        info_grid = Gtk.Grid()
        info_grid.set_halign(Gtk.Align.CENTER)
        info_grid.set_valign(Gtk.Align.CENTER)
        self.right_container.add(info_grid)

        # Create the main app logo
        logo0_sstr = f"{RESOURCES_DIRECTORY}ODL_st_icon_232px.png"
        logo_pixbuf = GdkPixbuf.Pixbuf.new_from_file(logo0_sstr)
        scaled_logo_pixbuf = logo_pixbuf.scale_simple(140, 140, GdkPixbuf.InterpType.BILINEAR)
        logo0 = Gtk.Image.new_from_pixbuf(scaled_logo_pixbuf)

        # Create the first label
        label0 = Gtk.Label(label="")
        info_sstr2 = "      Teacher-Student    \n      machine learning    \n"
        label_to_display2 = f'<span font="14" color="#FFFFFFFF"><b>{info_sstr2}</b></span>'
        label0.set_markup(label_to_display2)
        label0.set_justify(Gtk.Justification.CENTER)
        label0.set_margin_top(20)

        # Create the second label
        label1 = Gtk.Label(label="")
        info_sstr2 = " Powered by:\n"
        label_to_display2 = f'<span font="14" color="#FFD200FF"><b>{info_sstr2}</b></span>'
        label1.set_markup(label_to_display2)
        label1.set_justify(Gtk.Justification.CENTER)
        label1.set_margin_top(5)

        # Create the main app logo
        logo1_sstr = f"{RESOURCES_DIRECTORY}ODL_onnxruntime.png"
        logo_pixbuf = GdkPixbuf.Pixbuf.new_from_file(logo1_sstr)
        scaled_ort_logo_pixbuf = logo_pixbuf.scale_simple(120, 55, GdkPixbuf.InterpType.BILINEAR)
        logo1 = Gtk.Image.new_from_pixbuf(scaled_ort_logo_pixbuf)

        # Create the second label
        label2 = Gtk.Label(label="")
        info_sstr2 = " Use case:\n"
        label_to_display2 = f'<span font="14" color="#FFD200FF"><b>{info_sstr2}</b></span>'
        label2.set_markup(label_to_display2)
        label2.set_justify(Gtk.Justification.CENTER)
        label2.set_margin_top(20)

        # Create the main app logo
        logo2_sstr = f"{RESOURCES_DIRECTORY}ODL_object_detect.png"
        logo_pixbuf = GdkPixbuf.Pixbuf.new_from_file(logo2_sstr)
        scaled_ort_logo_pixbuf = logo_pixbuf.scale_simple(120, 90, GdkPixbuf.InterpType.BILINEAR)
        logo2 = Gtk.Image.new_from_pixbuf(scaled_ort_logo_pixbuf)

        # Attach widgets to the grid
        info_grid.attach(logo0, 0, 0, 1, 1)
        info_grid.attach(label0, 0, 1, 1, 1)
        info_grid.attach(label1, 0, 2, 1, 1)
        info_grid.attach(logo1, 0, 3, 1, 1)
        info_grid.attach(label2, 0, 4, 1, 1)
        info_grid.attach(logo2, 0, 5, 1, 1)

        # Set the background color of the right section to blue
        blue_color = Gdk.RGBA()
        blue_color.parse("#03234b")
        self.right_container.override_background_color(Gtk.StateFlags.NORMAL, blue_color)

        #setup main box which group the three previous boxes
        self.main_box = Gtk.Box()
        self.main_box.set_homogeneous(False)

        self.left_container = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=0, name="left-container")
        self.left_container.set_homogeneous(False)

        self.main_box.pack_start(self.left_container, True, True, 0)
        self.main_box.pack_end(self.right_container, False, True, 0)
        self.left_container.pack_start(self.video_box,True,True,0)

        self.left_container.pack_start(inference_nav_box, False, False, 0)
        self.add(self.main_box)

        return True

    def drawing(self, widget, cr):
        """
        Drawing callback used to draw with cairo on
        the drawing area
        """
        if self.first_drawing_call :
            self.first_drawing_call = False
            self.drawing_width = self.app.main_window.gtkwaylandsink.props.widget.get_allocated_width()
            self.drawing_height = self.app.main_window.gtkwaylandsink.props.widget.get_allocated_height()
            cr.set_font_size(self.ui_cairo_font_size_label)
            self.bbcolor_list = self.bboxes_colors()
            return False


        allocation = self.app.main_window.gtkwaylandsink.props.widget.get_allocation()
        widget_x, widget_y = allocation.x, allocation.y


        cr.set_line_width(4)
        cr.set_font_size(self.ui_cairo_font_size)

        width_scale = self.drawing_width / 300
        height_scale = self.drawing_height / 300

        # Define the clipping region to match the drawing area
        cr.rectangle(widget_x, widget_y, self.drawing_width, self.drawing_height)
        cr.clip()


        for i in range(np.array(self.app.nn_result_scores).size):

            # Scale NN outputs for the display before drawing
            y0 = int(self.app.nn_result_locations[i][1] ) * height_scale
            x0 = int(self.app.nn_result_locations[i][0] ) * width_scale
            y1 = int(self.app.nn_result_locations[i][3] ) * height_scale
            x1 = int(self.app.nn_result_locations[i][2] ) * width_scale
            accuracy = self.app.nn_result_scores[i] * 100
            x = x0 + widget_x
            y = y0
            width = (x1 - x0)
            height = (y1 - y0)
            label = self.app.nn.get_label(i,self.app.nn_result_classes)
            cr.set_source_rgb(255, 210, 0)
            cr.rectangle(int(x),int(y),width,height)
            cr.stroke()
            cr.move_to(x , (y - (self.ui_cairo_font_size/2)))
            text_to_display = label + " " + str(int(accuracy)) + "%"
            cr.show_text(text_to_display)

        # Draw the text on top of all frames
        cr.set_source_rgb(255, 255, 255)  # Set text color to black
        cr.set_font_size(25)
        inference_time_text = f"Inf. time: {self.app.nn_inference_time:.2f} ms - Inf. FPS: {self.app.nn_inference_fps:.3f}"
        cr.move_to(113, 30)
        cr.show_text(inference_time_text)

        return True

    def on_inference_back_button_clicked(self, button):
        self.app.main_window.notebook.set_current_page(3)
        self.app.main_window.pipeline_preview.set_state(Gst.State.PAUSED)
        self.app.main_window.pipeline_preview.set_state(Gst.State.READY)
        self.app.main_window.inference_video_box.hide()
        self.app.main_window.inference_video_box.remove(self.app.main_window.gtkwaylandsink.props.widget)
        self.hide()

    def on_old_model_inference_button_clicked(self, button):
        self.old_model_button.set_sensitive(False)
        self.retrained_model_button.set_sensitive(True)
        # Stop the pipeline
        self.app.main_window.pipeline_preview.set_state(Gst.State.PAUSED)
        self.app.main_window.pipeline_preview.get_state(Gst.CLOCK_TIME_NONE)

        input_model_path = INFERENCE_MODELS_DIRECTORY + "old_quant_model.onnx"
        if not os.path.exists(input_model_path):
            input_model_path = INFERENCE_MODELS_DIRECTORY + "inference_model.onnx"
        self.app.nn = ssd_mobilenetv2(model_path=input_model_path, labels_file_path=args.label_file,
                                      conf_thres=args.conf_threshold, iou_thres=args.iou_threshold,
                                      hw_acceleration=False)

        self.app.main_window.pipeline_preview.set_state(Gst.State.PLAYING)

    def on_retrained_model_inference_button_clicked(self, button):
        self.retrained_model_button.set_sensitive(False)
        self.old_model_button.set_sensitive(True)

        # Stop the pipeline
        self.app.main_window.pipeline_preview.set_state(Gst.State.PAUSED)
        self.app.main_window.pipeline_preview.get_state(Gst.CLOCK_TIME_NONE)

        input_model_path = INFERENCE_MODELS_DIRECTORY + "ssd_static_quant_fixed.onnx"
        if not os.path.exists(input_model_path):
            print("No retrained model available")
            return

        self.app.nn = ssd_mobilenetv2(model_path=input_model_path, labels_file_path= args.label_file,
                                      conf_thres=args.conf_threshold, iou_thres=args.iou_threshold,
                                      hw_acceleration=False)

        self.app.main_window.pipeline_preview.set_state(Gst.State.PLAYING)


class Application:
    """
    Class that handles the whole application
    """

    def __init__(self, args):

        self.window_width = 0
        self.window_height = 0

        self.frame_width = args.frame_width
        self.frame_height = args.frame_height
        self.framerate = args.framerate

        self.args = args

        self.get_display_resolution()


        #Test if a camera is connected
        check_camera_cmd = RESOURCES_DIRECTORY + "check_camera_preview.sh"
        check_camera = subprocess.run(check_camera_cmd)
        if check_camera.returncode==1:
            print("no camera connected")
            exit(1)

        ##### NN model initialization #####

        input_model_path = self.args.inference_model_path

        if not os.path.exists(input_model_path):
            input_model_path = INFERENCE_MODELS_DIRECTORY + "old_quant_model.onnx"
            if not os.path.exists(input_model_path):
                input_model_path = INFERENCE_MODELS_DIRECTORY + "inference_model.onnx"

        self.nn = ssd_mobilenetv2(model_path=input_model_path, labels_file_path=args.label_file,
                                  conf_thres=args.conf_threshold, iou_thres=args.iou_threshold,
                                  hw_acceleration=False)

        self.nn_input_width = self.nn.input_width
        self.nn_input_height = self.nn.input_height
        self.nn_input_channel = self.nn.input_shape[1]

        self.nn_result_locations=[]
        self.nn_result_scores=[]
        self.nn_result_classes=[]
        self.predictions = []

        self.nn_inference_time = 0.0
        self.nn_inference_fps = 0.0
        self.nn_result_label = 0

        self.main_window = MainWindow(args, self)
        self.overlay_window = OverlayWindow(args,self)

        if input_model_path == INFERENCE_MODELS_DIRECTORY + "ssd_static_quant_fixed.onnx":
            self.overlay_window.retrained_model_button.set_sensitive(False)
        else:
            self.overlay_window.retrained_model_button.set_sensitive(True)
            self.overlay_window.old_model_button.set_sensitive(False)

        self.main()

    def get_display_resolution(self):
        """
        Used to ask the system for the display resolution
        """

        cmd = "modetest -M stm -c > /tmp/display_resolution.txt"
        os.system(cmd)
        display_info_pattern = "#0"
        display_information = ""
        with open("/tmp/display_resolution.txt") as file:
            for line in file:
                if display_info_pattern in line:
                    display_information = line.split(display_info_pattern)[-1].strip()
        os.remove("/tmp/display_resolution.txt")
        for part in display_information.split():
            if 'x' in part:
                display_width, display_height = part.split('x')
                break
        display_width, display_height = display_information.split("x")
        display_height = display_height.split()[0]

        self.window_width = int(display_width)
        self.window_height = int(display_height) - 100
        print(f"Gtk Window resolution: {self.window_width} x {self.window_height}")
        self.info_box_width = self.window_width // 5
        self.main_box_width = self.window_width - self.info_box_width
        self.charts_box_height = (self.window_height - 5) // 2
        self.params_box_height = (self.window_height - 5) // 3 - 10
        self.chart_width = (self.window_height - 3 * 5) // 2
        self.scales_height = self.params_box_height - 5
        self.chart_height = (self.window_height - 3 * 5) // 2
        self.ui_icon_exit_width = 50
        self.ui_icon_exit_height = 50


    def update_ui(self):
        """
        refresh overlay UI
        """
        self.overlay_window.inf_time_label.set_text(f"Inference time: {self.nn_inference_time:.0f} ms")
        self.overlay_window.inf_fps_label.set_text(f"Inference fps: {self.nn_inference_fps:.0f}")
        self.main_window.queue_draw()
        self.overlay_window.queue_draw()


    def main(self):
        self.main_window.connect("delete-event", Gtk.main_quit)
        self.main_window.connect("destroy", Gtk.main_quit)
        self.main_window.show_all()

        self.overlay_window.connect("delete-event", Gtk.main_quit)
        self.overlay_window.hide()

        return True


if __name__ == '__main__':
    # add signal to catch CRTL+C
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    parser = argparse.ArgumentParser()

    parser.add_argument("--frame_width", default=640,
                        help="width of the camera frame (default is 640)")
    parser.add_argument("--frame_height", default=480,
                        help="height of the camera frame (default is 480)")
    parser.add_argument("--framerate", default=30,
                        help="framerate of the camera (default is 30fps)")
    parser.add_argument("-t", "--teacher_model", default="",
                        help="Onnx Teacher model to be used for annotation.")
    parser.add_argument("-l", "--label_file", default="",
                        help="name of file containing labels")
    parser.add_argument("--conf_threshold", default=0.60, type=float,
                        help="threshold of accuracy above which the boxes are displayed (default 0.60)")
    parser.add_argument("--iou_threshold", default=0.45, type=float,
                        help="threshold of intersection over union above which the boxes are displayed (default 0.45)")
    parser.add_argument("--nb_img_calib", type=int, default=20,
                        help="Number of images to consider for static quantization parameters calibration. Default = 20")
    parser.add_argument("--training_artifacts_path", default=ARTIFACTS_DIRECTORY,
                        help="Training artifacts directory path.")
    parser.add_argument("--inference_model_path", default=INFERENCE_MODELS_DIRECTORY + "ssd_static_quant_fixed.onnx",
                        help="Inference model path")


    args = parser.parse_args()

    try:
        application = Application(args)

    except Exception as exc:
        print("Main Exception: ", exc)

    Gtk.main()

    print("gtk main finished")
    print("application exited properly")
    os._exit(0)