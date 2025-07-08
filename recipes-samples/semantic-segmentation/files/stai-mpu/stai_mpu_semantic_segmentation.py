#!/usr/bin/python3
#
# Copyright (c) 2024 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

import gi
gi.require_version('Gtk', '3.0')
gi.require_version('Gst', '1.0')
from gi.repository import Gtk
from gi.repository import Gdk
from gi.repository import GLib
from gi.repository import GdkPixbuf
from gi.repository import Gst

import sys
import numpy as np
import argparse
import signal
import os
import random
import json
import subprocess
import re
import time
import os.path
import math
from os import path
import cv2
from PIL import Image
import PIL
from deeplab_v3_pp import NeuralNetwork
from timeit import default_timer as timer

np.set_printoptions(threshold=np.inf)
#init gstreamer
Gst.init(None)
Gst.init_check(None)
#init gtk
Gtk.init(None)
Gtk.init_check(None)

#path definition
RESOURCES_DIRECTORY = os.path.abspath(os.path.dirname(__file__)) + "/../resources/"

class GstWidget(Gtk.Box):
    """
    Class that handles Gstreamer pipeline using gtkwaylandsink and appsink
    """
    def __init__(self, app, nn):
         super().__init__()
         # connect the gtkwidget with the realize callback
         self.connect('realize', self._on_realize)
         self.instant_fps = 0
         self.app = app
         self.nn = nn
         self.cpt_frame = 0
         self.isp_first_config = True

    def _on_realize(self, widget):
        if(args.camera_src == "LIBCAMERA"):
            self.camera_dual_pipeline_creation()
            self.pipeline_preview.set_state(Gst.State.PLAYING)
        else:
            print("Camera source used not supported use LIBCAMERA \n")

    def camera_dual_pipeline_creation(self):
        """
        creation of the gstreamer pipeline when gstwidget is created dedicated to camera stream
        (in dual camera pipeline mode)
        """
        # gstreamer pipeline creation
        self.pipeline_preview = Gst.Pipeline.new("Semantic Segmentation")

        # creation of the source element
        self.libcamerasrc = Gst.ElementFactory.make("libcamerasrc", "libcamera")
        if not self.libcamerasrc:
            raise Exception("Could not create Gstreamer camera source element")

        # creation of the libcamerasrc caps for the pipelines for camera
        caps = "video/x-raw,width=" + str(self.app.frame_width) + ",height=" + str(self.app.frame_height) + ",format=RGB16, framerate=" + str(args.framerate)+ "/1"
        print("Main pipe configuration: ", caps)
        caps_src = Gst.Caps.from_string(caps)

        # creation of the libcamerasrc caps for the pipelines for nn
        caps = "video/x-raw,width=" + str(self.app.nn_input_width) + ",height=" + str(self.app.nn_input_height) + ",format=RGB, framerate=" + str(args.framerate)+ "/1"
        print("Aux pipe configuration:  ", caps)
        caps_src0 = Gst.Caps.from_string(caps)

        # creation of the queues elements
        queue  = Gst.ElementFactory.make("queue", "queue")
        queue.set_property("leaky", 2)  # 0: no leak, 1: upstream (oldest), 2: downstream (newest)
        queue.set_property("max-size-buffers", 1)  # Maximum number of buffers in the queue

        # Only one buffer in the queue0 to get 30fps on the display preview pipeline
        queue0 = Gst.ElementFactory.make("queue", "queue0")
        queue0.set_property("leaky", 2)  # 0: no leak, 1: upstream (oldest), 2: downstream (newest)
        queue0.set_property("max-size-buffers", 1)  # Maximum number of buffers in the queue

        # creation of the gtkwaylandsink element to handle the gestreamer video stream
        gtkwaylandsink = Gst.ElementFactory.make("gtkwaylandsink")
        self.pack_start(gtkwaylandsink.props.widget, True, True, 0)
        gtkwaylandsink.props.widget.show()
        self.videoscale = Gst.ElementFactory.make("videoscale", "videoscale")
        # creation and configuration of the fpsdisplaysink element to measure display fps
        fpsdisplaysink = Gst.ElementFactory.make("fpsdisplaysink", "fpsmeasure")
        fpsdisplaysink.set_property("signal-fps-measurements", True)
        fpsdisplaysink.set_property("fps-update-interval", 2000)
        fpsdisplaysink.set_property("text-overlay", False)
        fpsdisplaysink.set_property("video-sink", gtkwaylandsink)
        fpsdisplaysink.connect("fps-measurements",self.get_fps_display)

        # creation and configuration of the appsink element
        self.appsink = Gst.ElementFactory.make("appsink", "appsink")
        self.appsink.set_property("emit-signals", True)
        self.appsink.set_property("sync", False)
        self.appsink.set_property("max-buffers", 1)
        self.appsink.set_property("drop", True)
        self.appsink.connect("new-sample", self.new_sample)

        videorate = Gst.ElementFactory.make("videorate", "videorate")
        videorate0 = Gst.ElementFactory.make("videorate", "videorate2")

        # check if all elements were created
        if not all([self.pipeline_preview, self.libcamerasrc, queue, queue0, fpsdisplaysink, gtkwaylandsink, self.appsink]):
            print("Not all elements could be created. Exiting.")
            return False

        # add all elements to the pipeline
        self.pipeline_preview.add(self.libcamerasrc)
        self.pipeline_preview.add(videorate)
        self.pipeline_preview.add(videorate0)
        self.pipeline_preview.add(queue)
        self.pipeline_preview.add(queue0)
        self.pipeline_preview.add(fpsdisplaysink)
        self.pipeline_preview.add(self.appsink)

        # linking elements together
        #
        #              | src   --> videorate --> queue  [caps_src] --> fpsdisplaysink (connected to gtkwaylandsink)
        # libcamerasrc |
        #              | src_0 --> videorate0 --> queue0 [caps_src0] --> videoscale --> appsink
        #

        # display pipeline
        self.libcamerasrc.link(videorate)
        videorate.link(queue)
        queue.link_filtered(fpsdisplaysink, caps_src)

        # NN pipeline
        src_request_pad_template = self.libcamerasrc.get_pad_template("src_%u")
        src_request_pad0 = self.libcamerasrc.request_pad(src_request_pad_template, None, None)
        src_request_pad0.link(videorate0.get_static_pad("sink"))
        videorate0.link(queue0)
        queue0.link_filtered(self.appsink, caps_src0)

        queue_sink_pad = queue.get_static_pad("sink")
        queue0_sink_pad = queue0.get_static_pad("sink")

        # view-finder
        src_pad = self.libcamerasrc.get_static_pad("src")
        src_pad.set_property("stream-role", 3)
        # still-capture
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
            self.app.draw_inference = True
            self.app.update_ui()

    def preprocess_buffer(self,sample):
        """
        conversion of the gstreamer frame buffer into numpy array
        """
        buf = sample.get_buffer()
        if(args.debug):
            buf_size = buf.get_size()
            buff = buf.extract_dup(0, buf.get_size())
            f=open("/home/weston/NN_sample_dump.raw", "wb")
            f.write(buff)
            f.close()
        caps = sample.get_caps()
        #get gstreamer buffer size
        buffer_size = buf.get_size()
        #determine the shape of the numpy array
        number_of_column = caps.get_structure(0).get_value('width')
        number_of_lines = caps.get_structure(0).get_value('height')
        channels = 3
        buffer = np.frombuffer(buf.extract_dup(0, buf.get_size()), dtype=np.uint8)

        #DCMIPP pixelpacker has a constraint on the output resolution that should be multiple of 16.
        # the allocated buffer may contains stride to handle the DCMIPP Hw constraints/
        #The following code allow to handle both cases by anticipating the size of the
        #allocated buffer according to the NN resolution
        if (self.app.nn_input_width % 16 != 0):
            # Calculate the nearest upper multiple of 16
            upper_multiple = ((self.app.nn_input_width // 16) + 1) * 16
            # Calculate the stride and offset
            stride = upper_multiple * channels
            offset = stride - (number_of_column * channels)
        else :
            # Calculate the stride and offset
            stride = number_of_column * channels
            offset = 0
        num_lines = len(buffer) // stride
        arr = np.empty((number_of_column, number_of_lines, channels), dtype=np.uint8)
        # Fill the processed buffer properly depending on stride and offset
        for i in range(num_lines):
            start_index = i * stride
            end_index = start_index + (stride - offset)
            line_data = buffer[start_index:end_index]
            arr[i] = line_data.reshape((number_of_column, channels))
        return arr

    def new_sample(self,*data):
        """
        recover video frame from appsink
        and run inference
        """
        sample = self.appsink.emit("pull-sample")
        arr = self.preprocess_buffer(sample)
        if(args.debug):
            cv2.imwrite("/home/weston/NN_cv_sample_dump.png",arr)
        self.last_picture = arr.copy()
        if (args.validation):
            if (arr is not None):
                start_time = timer()
                self.nn.launch_inference(arr)
                stop_time = timer()
                self.app.nn_inference_time = stop_time - start_time
                self.app.nn_inference_fps = (1000/(self.app.nn_inference_time*1000))
                self.app.unique_label, self.app.nn_seg_map = self.app.nn.get_results()
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

class MainWindow(Gtk.Window):
    """
    This class handles all the functions necessary
    to display video stream in GTK GUI or still
    pictures using OpenCVS
    """

    def __init__(self,args,app):
        """
        Setup instances of class and shared variables
        usefull for the application
        """
        Gtk.Window.__init__(self)
        self.app = app
        self.main_ui_creation(args)

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
        self.ui_icon_label_size = '64'
        if window_constraint <= 272:
               # Display 480x272
               self.ui_cairo_font_size = 11
               self.ui_cairo_font_size_label = 18
               self.ui_icon_exit_size = '25'
               self.ui_icon_st_size = '52'
               self.ui_icon_label_size = '32'
        elif window_constraint <= 480:
               #Display 800x480
               self.ui_cairo_font_size = 16
               self.ui_cairo_font_size_label = 29
               self.ui_icon_exit_size = '50'
               self.ui_icon_st_size = '80'
               self.ui_icon_label_size = '64'
        elif window_constraint <= 600:
               #Display 1024x600
               self.ui_cairo_font_size = 16
               self.ui_cairo_font_size_label = 32
               self.ui_icon_exit_size = '50'
               self.ui_icon_st_size = '120'
               self.ui_icon_label_size = '64'
        elif window_constraint <= 720:
               #Display 1280x720
               self.ui_cairo_font_size = 23
               self.ui_cairo_font_size_label = 38
               self.ui_icon_exit_size = '50'
               self.ui_icon_st_size = '160'
               self.ui_icon_label_size = '64'
        elif window_constraint <= 1080:
               #Display 1920x1080
               self.ui_cairo_font_size = 33
               self.ui_cairo_font_size_label = 48
               self.ui_icon_exit_size = '50'
               self.ui_icon_st_size = '160'
               self.ui_icon_label_size = '64'

    def main_ui_creation(self,args):
        """
        Setup the Gtk UI of the main window
        """
        # remove the title bar
        self.set_decorated(False)

        self.first_drawing_call = True
        GdkDisplay = Gdk.Display.get_default()
        monitor = Gdk.Display.get_monitor(GdkDisplay, 0)
        workarea = Gdk.Monitor.get_workarea(monitor)

        GdkScreen = Gdk.Screen.get_default()
        provider = Gtk.CssProvider()
        css_path = RESOURCES_DIRECTORY + "Default.css"
        self.set_name("main_window")
        provider.load_from_path(css_path)
        Gtk.StyleContext.add_provider_for_screen(GdkScreen, provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        self.maximize()
        self.screen_width = workarea.width
        self.screen_height = workarea.height

        self.set_position(Gtk.WindowPosition.CENTER)
        self.connect('destroy', Gtk.main_quit)
        self.set_ui_param()
        # setup info_box containing inference results and ST_logo which is a
        # "next inference" button in still picture mode
        if self.app.enable_camera_preview == True:
            # camera preview mode
            self.info_box = Gtk.VBox()
            self.info_box.set_name("gui_main_stbox")
            self.st_icon_path = RESOURCES_DIRECTORY + 'SEMSEG_st_icon_' + self.ui_icon_st_size + 'px' + '.png'
            self.st_icon = Gtk.Image.new_from_file(self.st_icon_path)
            self.st_icon_event = Gtk.EventBox()
            self.st_icon_event.add(self.st_icon)
            self.info_box.pack_start(self.st_icon_event,False,False,2)
            self.inf_time = Gtk.Label()
            self.inf_time.set_justify(Gtk.Justification.CENTER)
            self.info_box.pack_start(self.inf_time, False, False,10)
            info_sstr = "    disp.fps :      " + "\n" + "  inf.fps :     " + "\n" + "  inf.time :    " + "\n"
            self.inf_time.set_markup("<span font=\'%d\' color='#FFFFFFFF'><b>%s\n</b></span>" % (self.ui_cairo_font_size,info_sstr))
            self.labels_to_display = Gtk.Label()
            self.labels_to_display.set_justify(Gtk.Justification.CENTER)
            self.info_box.pack_start(self.labels_to_display,False,False,2)
            self.rules = Gtk.Label()
            self.rules.set_justify(Gtk.Justification.CENTER)
            self.info_box.pack_start(self.rules,False,False,10)
            rules_sstr = " Click on " + "\n" + " camera preview " + "\n" + " to run " + "\n" + " segmentation " + "\n"
            self.rules.set_markup("<span font=\'%d\' color='#000000'><b>%s\n</b></span>" % (self.ui_cairo_font_size,rules_sstr))
            self.label_icon_path = RESOURCES_DIRECTORY + 'label_icon_' + self.ui_icon_label_size + 'x' + self.ui_icon_label_size + '.png'
            self.label_icon = Gtk.Image.new_from_file(self.label_icon_path)
            self.label_icon_event = Gtk.EventBox()
            self.label_icon_event.add(self.label_icon)
            self.info_box.pack_start(self.label_icon_event,False,False,10)
        else :
            # still picture mode
            self.info_box = Gtk.VBox()
            self.info_box.set_name("gui_main_stbox")
            self.st_icon_path = RESOURCES_DIRECTORY + 'SEMSEG_st_icon_next_inference_' + self.ui_icon_st_size + 'px' + '.png'
            self.st_icon = Gtk.Image.new_from_file(self.st_icon_path)
            self.st_icon_event = Gtk.EventBox()
            self.st_icon_event.add(self.st_icon)
            self.info_box.pack_start(self.st_icon_event,False,False,20)
            self.inf_time = Gtk.Label()
            self.inf_time.set_justify(Gtk.Justification.CENTER)
            self.info_box.pack_start(self.inf_time,False,False,2)
            info_sstr = "  inf.fps :     " + "\n" + "  inf.time :     " + "\n"
            self.inf_time.set_markup("<span font=\'%d\' color='#FFFFFFFF'><b>%s\n</b></span>" % (self.ui_cairo_font_size,info_sstr))
            self.labels_to_display = Gtk.Label()
            self.labels_to_display.set_justify(Gtk.Justification.CENTER)
            self.info_box.pack_start(self.labels_to_display,False,False,2)
            self.label_icon_path = RESOURCES_DIRECTORY + 'label_icon_' + self.ui_icon_label_size + 'x' + self.ui_icon_label_size + '.png'
            self.label_icon = Gtk.Image.new_from_file(self.label_icon_path)
            self.label_icon_event = Gtk.EventBox()
            self.label_icon_event.add(self.label_icon)
            self.info_box.pack_start(self.label_icon_event,False,False,2)

        # setup video box containing gst stream in camera previex mode
        # and a openCV picture in still picture mode
        # An overlay is used to keep a gtk drawing area on top of the video stream
        self.video_box = Gtk.HBox()
        self.video_box.set_name("gui_main_video")
        if self.app.enable_camera_preview == True:
            # camera preview => gst stream
            self.video_widget = self.app.gst_widget
            self.video_widget.set_app_paintable(True)
            self.video_box.pack_start(self.video_widget, True, True, 0)
        else :
            # still picture => openCV picture
            self.image = Gtk.Image()
            self.video_box.pack_start(self.image, True, True, 0)
        # # setup the exit box which contains the exit button
        self.exit_box = Gtk.VBox()
        self.exit_box.set_name("gui_main_exit")
        self.exit_icon_path = RESOURCES_DIRECTORY + 'exit_' + self.ui_icon_exit_size + 'x' + self.ui_icon_exit_size + '.png'
        self.exit_icon = Gtk.Image.new_from_file(self.exit_icon_path)
        self.exit_icon_event = Gtk.EventBox()
        self.exit_icon_event.add(self.exit_icon)
        self.exit_box.pack_start(self.exit_icon_event,False,False,2)

        # setup main box which group the three previous boxes
        self.main_box =  Gtk.HBox()
        self.exit_box.set_name("gui_main")
        self.main_box.pack_start(self.info_box,False,False,0)
        self.main_box.pack_start(self.video_box,True,True,0)
        self.main_box.pack_start(self.exit_box,False,False,0)
        self.add(self.main_box)
        return True

    def update_frame(self, frame):
        """
        update frame in still picture mode
        """
        img = Image.fromarray(frame)
        data = img.tobytes()
        data = GLib.Bytes.new(data)
        pixbuf = GdkPixbuf.Pixbuf.new_from_bytes(data,
                                                 GdkPixbuf.Colorspace.RGB,
                                                 False,
                                                 8,
                                                 frame.shape[1],
                                                 frame.shape[0],
                                                 frame.shape[2] * frame.shape[1])
        self.image.set_from_pixbuf(pixbuf.copy())

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
        self.overlay_ui_creation(args)
        self.previous_touch_event = 0
        self.display_help = False

    def exit_icon_cb(self,eventbox, event):
        """
        Exit callback to close application
        """
        self.destroy()
        Gtk.main_quit()

    def label_icon_event_cb(self,eventbox, event):
        """
        Exit callback to close application
        """
        if event.type == Gdk.EventType.BUTTON_PRESS:
            if (self.display_help):
                self.display_help = False
                self.app.update_ui()
            else :
                self.display_help = True
                self.app.update_ui()


    def touch_event_cb(self, widget, event):
        """
        Touch event callback to stop camera stream and run inference on last camera frame
        """
        if event.touch.type == Gdk.EventType.TOUCH_BEGIN:
            state = self.app.gst_widget.pipeline_preview.get_state(Gst.CLOCK_TIME_NONE)
            if (state.state == Gst.State.PLAYING):
                self.app.gst_widget.pipeline_preview.set_state(Gst.State.PAUSED)
                self.app.draw_inference=True
                if (self.app.gst_widget.last_picture is not None):
                    start_time = timer()
                    self.app.nn.launch_inference(self.app.gst_widget.last_picture)
                    stop_time = timer()
                    self.app.nn_inference_time = stop_time - start_time
                    self.app.nn_inference_fps = (1000/(self.app.nn_inference_time*1000))
                    self.app.unique_label, self.app.nn_seg_map = self.app.nn.get_results()
                self.app.update_ui()
            elif (state.state == Gst.State.PAUSED):
                self.app.gst_widget.pipeline_preview.set_state(Gst.State.PLAYING)
                self.app.draw_inference = False
                self.app.update_ui()

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
        self.ui_icon_label_size = '64'
        if window_constraint <= 272:
               # Display 480x272
               self.ui_cairo_font_size = 11
               self.ui_cairo_font_size_label = 18
               self.ui_icon_exit_size = '25'
               self.ui_icon_st_size = '52'
               self.ui_icon_label_size = '32'
        elif window_constraint <= 480:
               #Display 800x480
               self.ui_cairo_font_size = 16
               self.ui_cairo_font_size_label = 29
               self.ui_icon_exit_size = '50'
               self.ui_icon_st_size = '80'
               self.ui_icon_label_size = '64'
        elif window_constraint <= 600:
               #Display 1024x600
               self.ui_cairo_font_size = 16
               self.ui_cairo_font_size_label = 32
               self.ui_icon_exit_size = '50'
               self.ui_icon_st_size = '120'
               self.ui_icon_label_size = '64'
        elif window_constraint <= 720:
               #Display 1280x720
               self.ui_cairo_font_size = 23
               self.ui_cairo_font_size_label = 38
               self.ui_icon_exit_size = '50'
               self.ui_icon_st_size = '160'
               self.ui_icon_label_size = '64'
        elif window_constraint <= 1080:
               #Display 1920x1080
               self.ui_cairo_font_size = 33
               self.ui_cairo_font_size_label = 48
               self.ui_icon_exit_size = '50'
               self.ui_icon_st_size = '160'
               self.ui_icon_label_size = '64'

    def overlay_ui_creation(self,args):
        """
        Setup the Gtk UI of the overlay window
        """
        # remove the title bar
        self.set_decorated(False)

        self.first_drawing_call = True
        GdkDisplay = Gdk.Display.get_default()
        monitor = Gdk.Display.get_monitor(GdkDisplay, 0)
        workarea = Gdk.Monitor.get_workarea(monitor)

        GdkScreen = Gdk.Screen.get_default()
        provider = Gtk.CssProvider()
        css_path = RESOURCES_DIRECTORY + "Default.css"
        self.set_name("overlay_window")
        provider.load_from_path(css_path)
        Gtk.StyleContext.add_provider_for_screen(GdkScreen, provider, Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION)
        self.maximize()
        self.screen_width = workarea.width
        self.screen_height = workarea.height

        self.set_position(Gtk.WindowPosition.CENTER)
        self.connect('destroy', Gtk.main_quit)
        self.set_ui_param()

        # setup info_box containing inference results and ST_logo which is a
        # "next inference" button in still picture mode
        if self.app.enable_camera_preview == True:
            # camera preview mode
            self.info_box = Gtk.VBox()
            self.info_box.set_name("gui_overlay_stbox")
            self.st_icon_path = RESOURCES_DIRECTORY + 'SEMSEG_st_icon_' + self.ui_icon_st_size + 'px' + '.png'
            self.st_icon = Gtk.Image.new_from_file(self.st_icon_path)
            self.st_icon_event = Gtk.EventBox()
            self.st_icon_event.add(self.st_icon)
            self.info_box.pack_start(self.st_icon_event,False,False,2)
            self.inf_time = Gtk.Label()
            self.inf_time.set_justify(Gtk.Justification.CENTER)
            self.info_box.pack_start(self.inf_time,False,False,10)
            info_sstr = "    disp.fps :      " + "\n" + "  inf.fps :     " + "\n" + "  inf.time :    " + "\n"
            self.inf_time.set_markup("<span font=\'%d\' color='#FFFFFFFF'><b>%s\n</b></span>" % (self.ui_cairo_font_size,info_sstr))
            self.labels_to_display = Gtk.Label()
            self.labels_to_display.set_justify(Gtk.Justification.CENTER)
            self.info_box.pack_start(self.labels_to_display,False,False,2)
            self.rules = Gtk.Label()
            self.rules.set_justify(Gtk.Justification.CENTER)
            self.info_box.pack_start(self.rules,False,False,10)
            rules_sstr = " Click on " + "\n" + " camera preview " + "\n" + " to run " + "\n" + " segmentation " + "\n"
            self.rules.set_markup("<span font=\'%d\' color='#E6007E'><b>%s\n</b></span>" % (self.ui_cairo_font_size,rules_sstr))
            self.label_icon_path = RESOURCES_DIRECTORY + 'label_icon_' + self.ui_icon_label_size + 'x' + self.ui_icon_label_size + '.png'
            self.label_icon = Gtk.Image.new_from_file(self.label_icon_path)
            self.label_icon_event = Gtk.EventBox()
            self.label_icon_event.add(self.label_icon)
            self.label_icon_event.connect("button_press_event",self.label_icon_event_cb)
            self.info_box.pack_start(self.label_icon_event,False,False,10)
        else :
            # still picture mode
            self.info_box = Gtk.VBox()
            self.info_box.set_name("gui_overlay_stbox")
            self.st_icon_path = RESOURCES_DIRECTORY + 'SEMSEG_st_icon_next_inference_' + self.ui_icon_st_size + 'px' + '.png'
            self.st_icon = Gtk.Image.new_from_file(self.st_icon_path)
            self.st_icon_event = Gtk.EventBox()
            self.st_icon_event.add(self.st_icon)
            self.st_icon_event.connect("button_press_event",self.still_picture)
            self.info_box.pack_start(self.st_icon_event,False,False,20)
            self.inf_time = Gtk.Label()
            self.inf_time.set_justify(Gtk.Justification.CENTER)
            self.info_box.pack_start(self.inf_time,False,False,2)
            info_sstr = "  inf.fps :     " + "\n" + "  inf.time :     " + "\n"
            self.inf_time.set_markup("<span font=\'%d\' color='#FFFFFFFF'><b>%s\n</b></span>" % (self.ui_cairo_font_size,info_sstr))
            self.labels_to_display = Gtk.Label()
            self.labels_to_display.set_justify(Gtk.Justification.CENTER)
            self.info_box.pack_start(self.labels_to_display,False,False,2)
            self.label_icon_path = RESOURCES_DIRECTORY + 'label_icon_' + self.ui_icon_label_size + 'x' + self.ui_icon_label_size + '.png'
            self.label_icon = Gtk.Image.new_from_file(self.label_icon_path)
            self.label_icon_event = Gtk.EventBox()
            self.label_icon_event.add(self.label_icon)
            self.label_icon_event.connect("button_press_event",self.label_icon_event_cb)
            self.info_box.pack_start(self.label_icon_event,False,False,2)

        # setup video box containing a transparent drawing area
        # to draw over the video stream
        self.video_box = Gtk.HBox()
        self.video_box.set_name("gui_overlay_video")
        self.video_box.set_app_paintable(True)
        self.drawing_area = Gtk.DrawingArea()
        self.drawing_area.connect("draw", self.drawing)
        self.drawing_area.connect("touch-event", self.touch_event_cb)
        self.drawing_area.add_events(Gdk.EventMask.TOUCH_MASK)
        self.drawing_area.set_name("overlay_draw")
        self.drawing_area.set_app_paintable(True)
        self.video_box.pack_start(self.drawing_area, True, True, 0)

        # # setup the exit box which contains the exit button
        self.exit_box = Gtk.VBox()
        self.exit_box.set_name("gui_overlay_exit")
        self.exit_icon_path = RESOURCES_DIRECTORY + 'exit_' + self.ui_icon_exit_size + 'x' + self.ui_icon_exit_size+ '.png'
        self.exit_icon = Gtk.Image.new_from_file(self.exit_icon_path)
        self.exit_icon_event = Gtk.EventBox()
        self.exit_icon_event.add(self.exit_icon)
        self.exit_icon_event.connect("button_press_event",self.exit_icon_cb)
        self.exit_box.pack_start(self.exit_icon_event,False,False,2)

        # setup main box which group the three previous boxes
        self.main_box =  Gtk.HBox()
        self.exit_box.set_name("gui_overlay")
        self.main_box.pack_start(self.info_box,False,False,0)
        self.main_box.pack_start(self.video_box,True,True,0)
        self.main_box.pack_start(self.exit_box,False,False,0)
        self.add(self.main_box)
        return True

    def drawing(self, widget, cr):
        """
        Drawing callback used to draw with cairo on
        the drawing area
        """
        if self.first_drawing_call :
            self.first_drawing_call = False
            self.drawing_width = widget.get_allocated_width()
            self.drawing_height = widget.get_allocated_height()
            cr.set_font_size(self.ui_cairo_font_size)
            self.draw = True
            if self.app.enable_camera_preview == False :
                self.app.still_picture_next = True
                self.app.draw_inference = True
                if args.validation:
                    GLib.idle_add(self.app.process_picture)
                else:
                    self.app.process_picture()
            return False

        if (self.app.draw_inference):
            self.display_help = False
            #recover the widget size depending of the information to display
            self.drawing_width = widget.get_allocated_width()
            self.drawing_height = widget.get_allocated_height()

            #adapt the drawing overlay depending on the image/camera stream displayed
            if self.app.enable_camera_preview == True:
                preview_ratio = float(args.frame_width)/float(args.frame_height)
                preview_height = self.drawing_height
                preview_width =  preview_ratio * preview_height
                if preview_width >= self.drawing_width:
                    offset = 0
                    preview_width = self.drawing_width
                    preview_height = preview_width / preview_ratio
                    vertical_offset = (self.drawing_height - preview_height)/2
                else :
                    offset = (self.drawing_width - preview_width)/2
                    vertical_offset = 0
            else :
                preview_width = self.app.frame_width
                preview_height = self.app.frame_height
                preview_ratio = preview_width / preview_height
                offset = int((self.drawing_width - preview_width)/2)
                vertical_offset = (self.drawing_height - preview_height)/2
                if args.validation:
                    self.app.still_picture_next = True
                    self.app.draw_inference = True

            #load the segmentation bitmap as a picture
            img = Image.fromarray(self.app.nn_seg_map,'RGBA')
            size = (int(preview_width),int(preview_height))
            img = img.resize(size)
            img_alpha = img.copy()
            img_alpha.save("/home/weston/bitmap.png","PNG")

            #load the bitmap to display it as overlay
            pixbuf = GdkPixbuf.Pixbuf.new_from_file('/home/weston/bitmap.png')
            img = Gdk.cairo_set_source_pixbuf(cr, pixbuf.copy(),int(offset), int(vertical_offset))
            cr.paint()
            if (self.app.enable_camera_preview == False):
                self.app.draw_inference = False
        else:
            if (self.display_help):
                cr.rectangle(0, 0, self.drawing_width, self.drawing_height)
                cr.set_source_rgba(3,35,75,0.25)
                cr.fill()
                #determine labels to display
                labels = self.app.nn.get_labels()
                label_map = np.arange(len(labels)).reshape(len(labels), 1)
                color_map = self.app.nn.colors_map[label_map]
                offset_x = self.drawing_width/6
                offset_y = self.drawing_height/8
                #display labels
                for i in range(len(labels)):
                    if (i < 10):
                        label = labels[i]
                        text = str(label)
                        cr.set_font_size(self.ui_cairo_font_size*1.5)
                        xbearing, ybearing, width, height, xadvance, yadvance = cr.text_extents(text)
                        cr.move_to(offset_x,offset_y+((self.ui_cairo_font_size*1.5)*i))
                        cr.text_path(text)
                        cr.set_source_rgba(color_map[i][0][0]/255, color_map[i][0][1]/255, color_map[i][0][2]/255,1)
                        cr.fill_preserve()
                        cr.set_source_rgb(1, 1, 1)
                        cr.set_line_width(0.1)
                        cr.stroke()
                    else :
                        label = labels[i]
                        text = str(label)
                        cr.set_font_size(self.ui_cairo_font_size*1.5)
                        xbearing, ybearing, width, height, xadvance, yadvance = cr.text_extents(text)
                        cr.move_to(3*offset_x,offset_y+((self.ui_cairo_font_size*1.5)*(i-10)))
                        cr.text_path(text)
                        cr.set_source_rgba(color_map[i][0][0]/255, color_map[i][0][1]/255, color_map[i][0][2]/255,1)
                        cr.fill_preserve()
                        cr.set_source_rgb(1, 1, 1)
                        cr.set_line_width(0.1)
                        cr.stroke()
            else :
                self.app.main_window.label_icon.show()
                self.label_icon.show()
                self.app.main_window.inf_time.hide()
                self.inf_time.hide()
                self.app.main_window.rules.show()
                self.rules.show()
                self.labels_to_display.hide()
        return True

    def still_picture(self,  widget, event):
        """
        ST icon cb which trigger a new inference
        """
        self.app.still_picture_next = True
        self.app.draw_inference = True
        return self.app.process_picture()

class Application:
    """
    Class that handles the whole application
    """
    def __init__(self, args):
        #init variables uses :
        self.exit_app = False
        self.first_call = True
        self.loading_nn = True
        self.draw_inference = False
        self.window_width = 0
        self.window_height = 0
        self.get_display_resolution()

        #preview dimensions and fps
        self.frame_width = args.frame_width
        self.frame_height = args.frame_height
        self.framerate = args.framerate

        #instantiate the Neural Network class
        self.nn = NeuralNetwork(args.model_file, args.label_file, float(args.input_mean), float(args.input_std))
        self.shape = self.nn.get_img_size()
        self.nn_input_width = self.shape[1]
        self.nn_input_height = self.shape[0]
        self.nn_input_channel = self.shape[2]
        self.nn_inference_time = 0.0
        self.nn_inference_fps = 0.0
        self.unique_label = []
        self.nn_seg_map = np.zeros((self.nn_input_width,self.nn_input_height,3))

        #if args.image is empty -> camera preview mode else still picture
        if args.image == "":
            print("camera preview mode activate")
            self.enable_camera_preview = True
            #Test if a camera is connected
            check_camera_cmd = RESOURCES_DIRECTORY + "check_camera_preview.sh"
            check_camera = subprocess.run(check_camera_cmd)
            if check_camera.returncode==1:
                print("no camera connected")
                exit(1)
        else:
            print("still picture mode activate")
            self.enable_camera_preview = False
            self.still_picture_next = False

        # initialize the list of the file to be processed (used with the
        # --image parameter)
        self.files = []
        # initialize the list of inference/display time to process the average
        # (used with the --validation parameter)
        self.valid_inference_time = []
        self.valid_inference_fps = []
        self.valid_preview_fps = []
        self.valid_draw_count = 0

        #instantiate the Gstreamer pipeline
        self.gst_widget = GstWidget(self,self.nn)
        #instantiate the main window
        self.main_window = MainWindow(args,self)
        #instantiate the overlay window
        self.overlay_window = OverlayWindow(args,self)
        self.main()

    def get_display_resolution(self):
        """
        Used to ask the system for the display resolution
        """
        cmd = "modetest -M stm -c > /tmp/display_resolution.txt"
        subprocess.run(cmd,shell=True)
        display_info_pattern = "#0"
        display_information = ""
        display_resolution = ""
        display_width = ""
        display_height = ""

        f = open("/tmp/display_resolution.txt", "r")
        for line in f :
            if display_info_pattern in line:
                display_information = line
        display_information_splited = display_information.split()
        for i in display_information_splited :
            if "x" in i :
                display_resolution = i
        display_resolution = display_resolution.replace('x',' ')
        display_resolution = display_resolution.split()
        display_width = display_resolution[0]
        display_height = display_resolution[1]

        print("display resolution is : ",display_width, " x ", display_height)
        self.window_width = int(display_width)
        self.window_height = int(display_height)
        return 0

    def valid_timeout_callback(self):
        """
        if timeout occurs that means that camera preview and the gtk is not
        behaving as expected */
        """
        print("Timeout: camera preview and/or gtk is not behaving has expected\n")
        Gtk.main_quit()
        os._exit(1)

    # get random file in a directory
    def getRandomFile(self, path):
        """
        Returns a random filename, chosen among the files of the given path.
        """
        if len(self.files) == 0:
            self.files = os.listdir(path)

        if len(self.files) == 0:
            return ''

        # remove .csv file
        item_to_remove = []
        for item in self.files:
            if item.endswith(".csv"):
                item_to_remove.append(item)

        for item in item_to_remove:
            self.files.remove(item)

        index = random.randrange(0, len(self.files))
        file_path = self.files[index]
        self.files.pop(index)
        return file_path

    def load_valid_results_from_csv_file(self,csv_file):
        """
        Load csv files containing expected results for the validation mode
        """
        csv_file = csv_file + '.csv'
        print("csv file name ",csv_file)
        load_validation_data = np.loadtxt(csv_file,delimiter=',',dtype=np.dtype(int))
        return load_validation_data

    def update_label_still(self, inference_time):
        """
        update inference results in still picture mode
        """
        str_inference_time = str("{0:0.1f}".format(inference_time)) + " ms"
        inference_fps = 1000/inference_time
        str_inference_fps = str("{0:.1f}".format(inference_fps)) + " fps"
        #determine labels to display
        labels = self.nn.get_labels()
        label_map = np.arange(len(labels)).reshape(len(labels), 1)
        color_map = self.nn.colors_map[label_map]
        label_sstr = ""
        if (self.draw_inference):
            #display labels
            for i in range(len(self.unique_label)):
                if (i != 0):
                    label = labels[self.unique_label[i]]
                    text = str(label)
                    R = hex(color_map[self.unique_label[i]][0][0])
                    G = hex(color_map[self.unique_label[i]][0][1])
                    B = hex(color_map[self.unique_label[i]][0][2])
                    R = R.removeprefix('0x')
                    G = G.removeprefix('0x')
                    B = B.removeprefix('0x')
                    R = R.upper()
                    G = G.upper()
                    B = B.upper()
                    if (len(R)==1):
                        R = "0" + R
                    if (len(G)==1):
                        G = "0" + G
                    if (len(B)==1):
                        B = "0" + B
                    label_sstr += "<span font=\'" + str(self.overlay_window.ui_cairo_font_size) + "\' color='#" + str(R) + str(G) + str(B) + "'><b>" +  text + "\n</b></span>"
            label_sstr = "<span font=\'" + str(self.overlay_window.ui_cairo_font_size) + "\' color='#FFFFFF'><b>Labels : \n</b></span>" + label_sstr
            info_sstr = "  inf.fps :     " + "\n" + str_inference_fps + "\n" + "  inf.time :     " + "\n"  + str_inference_time + "\n"
            self.overlay_window.inf_time.set_markup("<span font=\'%d\' color='#FFFFFF'><b>%s\n</b></span>" % (self.overlay_window.ui_cairo_font_size,info_sstr))
            self.overlay_window.labels_to_display.set_markup(label_sstr)
            self.main_window.label_icon.hide()
            self.overlay_window.label_icon.hide()
            self.main_window.inf_time.show()
            self.overlay_window.inf_time.show()
            self.overlay_window.labels_to_display.show()


    # Updating the labels and the inference infos displayed on the GUI interface - camera input
    def update_label_preview(self):
        """
        Updating the labels and the inference infos displayed on the GUI interface - camera input
        """
        inference_time = self.nn_inference_time * 1000
        inference_fps = self.nn_inference_fps
        display_fps = self.gst_widget.instant_fps

        if (args.validation) and (inference_time != 0) and (self.valid_draw_count > 5):
            self.valid_preview_fps.append(round(self.gst_widget.instant_fps))
            self.valid_inference_time.append(round(self.nn_inference_time * 1000, 4))

        str_inference_time = str("{0:0.1f}".format(inference_time)) + " ms"
        str_display_fps = str("{0:.1f}".format(display_fps)) + " fps"
        str_inference_fps = str("{0:.1f}".format(inference_fps)) + " fps"

        #determine labels to display
        labels = self.nn.get_labels()
        label_map = np.arange(len(labels)).reshape(len(labels), 1)
        color_map = self.nn.colors_map[label_map]
        label_sstr = ""
        if (self.draw_inference):
            #display labels
            for i in range(len(self.unique_label)):
                if (i != 0):
                    label = labels[self.unique_label[i]]
                    text = str(label)
                    R = hex(color_map[self.unique_label[i]][0][0])
                    G = hex(color_map[self.unique_label[i]][0][1])
                    B = hex(color_map[self.unique_label[i]][0][2])
                    R = R.removeprefix('0x')
                    G = G.removeprefix('0x')
                    B = B.removeprefix('0x')
                    R = R.upper()
                    G = G.upper()
                    B = B.upper()
                    if (len(R)==1):
                        R = "0" + R
                    if (len(G)==1):
                        G = "0" + G
                    if (len(B)==1):
                        B = "0" + B
                    label_sstr += "<span font=\'" + str(self.overlay_window.ui_cairo_font_size) + "\' color='#" + str(R) + str(G) + str(B) + "'><b>" +  text + "\n</b></span>"
            label_sstr = "<span font=\'" + str(self.overlay_window.ui_cairo_font_size) + "\' color='#FFFFFF'><b>Labels : \n</b></span>" + label_sstr
            info_sstr = "  inf.fps :     " + "\n" + str_inference_fps + "\n" + "      inf.time :        " + "\n"  + str_inference_time + "\n"
            self.overlay_window.inf_time.set_markup("<span font=\'%d\' color='#FFFFFF'><b>%s\n</b></span>" % (self.overlay_window.ui_cairo_font_size,info_sstr))
            self.overlay_window.labels_to_display.set_markup(label_sstr)
            self.overlay_window.rules.hide()
            self.main_window.label_icon.hide()
            self.overlay_window.label_icon.hide()
            self.main_window.inf_time.show()
            self.overlay_window.inf_time.show()
            self.overlay_window.labels_to_display.show()

        if args.validation:
            # reload the timeout
            GLib.source_remove(self.valid_timeout_id)
            self.valid_timeout_id = GLib.timeout_add(50000,
                                                     self.valid_timeout_callback)

            self.valid_draw_count = self.valid_draw_count + 1
            # stop the application after a certain amount of draws
            if self.valid_draw_count > int(args.val_run):
                avg_prev_fps = sum(self.valid_preview_fps) / len(self.valid_preview_fps)
                avg_inf_time = sum(self.valid_inference_time) / len(self.valid_inference_time)
                avg_inf_fps = (1000/avg_inf_time)
                print("avg display fps= " + str(avg_prev_fps))
                print("avg inference fps= " + str(avg_inf_fps))
                print("avg inference time= " + str(avg_inf_time) + " ms")
                GLib.source_remove(self.valid_timeout_id)
                Gtk.main_quit()
                return True

    def process_picture(self):
        """
        Still picture inference function
        Load the frame, launch inference and
        call functions to refresh UI
        """
        if self.exit_app:
            Gtk.main_quit()
            return False
        print("process picture enterred")
        if self.still_picture_next and self.overlay_window.draw :
            print("get a random file")
            # get randomly a picture in the directory
            rfile = self.getRandomFile(args.image)
            img = Image.open(args.image + "/" + rfile)

            # recover drawing box size and picture size
            screen_width = self.overlay_window.drawing_width
            screen_height = self.overlay_window.drawing_height
            picture_width, picture_height = img.size

            #adapt the frame to the screen with with the preservation of the aspect ratio
            width_ratio = float(screen_width/picture_width)
            height_ratio = float(screen_height/picture_height)

            if width_ratio >= height_ratio :
                self.frame_height = height_ratio * picture_height
                self.frame_width = height_ratio * picture_width
            else :
                self.frame_height = width_ratio * picture_height
                self.frame_width = width_ratio * picture_width

            self.frame_height = int(self.frame_height)
            self.frame_width = int(self.frame_width)
            prev_frame = cv2.resize(np.array(img), (self.frame_width, self.frame_height))

            # update the preview frame
            self.main_window.update_frame(prev_frame)
            self.draw = False

            #resize the frame to feed the NN model
            nn_frame = cv2.resize(np.array(img), (self.nn_input_width, self.nn_input_height))

            if (self.first_call):
            # execute a first inference to trigger model compilation
                self.nn.launch_inference(nn_frame)
                if(self.loading_nn):
                    self.loading_nn = False
                self.first_call = False

            start_time = timer()
            self.nn.launch_inference(nn_frame)
            stop_time = timer()
            self.still_picture_next = False
            self.nn_inference_time = stop_time - start_time
            self.nn_inference_fps = (1000/(self.nn_inference_time*1000))
            self.unique_label, self.nn_seg_map= self.nn.get_results()

            # write information onf the GTK UI
            inference_time = self.nn_inference_time * 1000

            if args.validation and inference_time != 0:
                # reload the timeout
                self.draw_inference = True
                GLib.source_remove(self.valid_timeout_id)
                self.valid_timeout_id = GLib.timeout_add(50000,
                                                        self.valid_timeout_callback)

                #prepare inference results to compare with validation results
                np_array_seg = self.nn_seg_map.copy()
                np_array_seg = np.transpose(np_array_seg,(2,0,1))
                np_array_seg = np_array_seg.reshape(4,-1)
                np_array_seg = np.asarray(np_array_seg)
                #  get file path without extension
                file_name_no_ext = os.path.splitext(rfile)[0]

                print("\nInput file: " + args.image + "/" + rfile)

                input_file = args.image + "/" + file_name_no_ext
                # retreive associated CSV file information
                seg_map_expected = self.load_valid_results_from_csv_file(input_file)
                if not(np.array_equal(seg_map_expected,np_array_seg)):
                    print("Inference result mismatch with validation results")
                    os._exit(5)
                self.valid_inference_time.append(round(self.nn_inference_time * 1000, 4))

                # process all the file
                if len(self.files) == 0:
                   avg_inf_time = sum(self.valid_inference_time) / len(self.valid_inference_time)
                   avg_inf_time = round(avg_inf_time,4)
                   print("avg inference time= " + str(avg_inf_time) + " ms")
                   self.exit_app = True
            self.update_label_still(inference_time)
            self.main_window.queue_draw()
            self.overlay_window.queue_draw()
            return True
        else :
            return False

    def update_ui(self):
        """
        refresh overlay UI
        """
        self.update_label_preview()
        self.main_window.queue_draw()
        self.overlay_window.queue_draw()


    def main(self):
        """
        main function
        """
        self.main_window.connect("delete-event", Gtk.main_quit)
        self.main_window.show_all()
        self.overlay_window.connect("delete-event", Gtk.main_quit)
        self.overlay_window.show_all()
        # start a timeout timer in validation process to close application if
        # timeout occurs
        if args.validation:
            self.valid_timeout_id = GLib.timeout_add(50000,
                                                     self.valid_timeout_callback)
        return True

if __name__ == '__main__':
    # add signal to catch CRTL+C
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    #Tensorflow Lite NN intitalisation
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--image", default="", help="image directory with image to be classified")
    parser.add_argument("-v", "--video_device", default="", help="video device ex: video0")
    parser.add_argument("--frame_width", default=640, help="width of the camera frame (default is 320)")
    parser.add_argument("--frame_height", default=480, help="height of the camera frame (default is 240)")
    parser.add_argument("--framerate", default=15, help="framerate of the camera (default is 15fps)")
    parser.add_argument("-m", "--model_file", default="", help=".tflite/nbg/onnx model to be executed")
    parser.add_argument("-l", "--label_file", default="", help="name of file containing labels")
    parser.add_argument("--input_mean", default=127.5, help="input mean")
    parser.add_argument("--input_std", default=127.5, help="input standard deviation")
    parser.add_argument("--validation", action='store_true', help="enable the validation mode")
    parser.add_argument("--val_run", default=50, help="set the number of draws in the validation mode")
    parser.add_argument("--camera_src", default="LIBCAMERA", help="use V4L2SRC for MP1x and LIBCAMERA for MP2x")
    parser.add_argument("--debug", default=False, action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()

    try:
        application = Application(args)

    except Exception as exc:
        print("Main Exception: ", exc )

    Gtk.main()
    #remove bitmap.png file before closing app
    file = 'bitmap.png'
    location = "/home/weston"
    path = os.path.join(location,file)
    os.remove(path)
    print("gtk main finished")
    print("application exited properly")
    os._exit(0)
