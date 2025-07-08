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

import numpy as np
import argparse
import signal
import os
import random
import json
import subprocess
import os.path
from os import path
import cv2
import math
from PIL import Image
from timeit import default_timer as timer
from yolov8n_pose_pp import NeuralNetwork

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
        self.pipeline_preview = Gst.Pipeline.new("Pose Estimation")

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
            if(self.app.loading_nn):
                self.app.loading_nn = False
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
        if arr is not None :
            start_time = timer()
            self.nn.launch_inference(arr)
            stop_time = timer()
            self.app.nn_inference_time = stop_time - start_time
            self.app.nn_inference_fps = (1000/(self.app.nn_inference_time*1000))
            self.app.keypoint_loc, self.app.keypoint_edges, self.app.edge_colors = self.app.nn.get_results()
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
    pictures using OpenCV
    """

    def __init__(self,args,app):
        """
        Setup instances of class and shared variables
        useful for the application
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
        if window_constraint <= 272:
               # Display 480x272
               self.ui_cairo_font_size = 11
               self.ui_cairo_font_size_label = 18
               self.ui_icon_exit_size = '25'
               self.ui_icon_st_size = '52'
        elif window_constraint <= 480:
               #Display 800x480
               self.ui_cairo_font_size = 16
               self.ui_cairo_font_size_label = 29
               self.ui_icon_exit_size = '50'
               self.ui_icon_st_size = '80'
        elif window_constraint <= 600:
               #Display 1024x600
               self.ui_cairo_font_size = 19
               self.ui_cairo_font_size_label = 32
               self.ui_icon_exit_size = '50'
               self.ui_icon_st_size = '120'
        elif window_constraint <= 720:
               #Display 1280x720
               self.ui_cairo_font_size = 23
               self.ui_cairo_font_size_label = 38
               self.ui_icon_exit_size = '50'
               self.ui_icon_st_size = '160'
        elif window_constraint <= 1080:
               #Display 1920x1080
               self.ui_cairo_font_size = 33
               self.ui_cairo_font_size_label = 48
               self.ui_icon_exit_size = '50'
               self.ui_icon_st_size = '160'

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
        # setup info_box containing inference results
        if self.app.enable_camera_preview == True:
            # camera preview mode
            self.info_box = Gtk.VBox()
            self.info_box.set_name("gui_main_stbox")
            self.st_icon_path = RESOURCES_DIRECTORY + 'PE_st_icon_' + self.ui_icon_st_size + 'px' + '.png'
            self.st_icon = Gtk.Image.new_from_file(self.st_icon_path)
            self.st_icon_event = Gtk.EventBox()
            self.st_icon_event.add(self.st_icon)
            self.info_box.pack_start(self.st_icon_event,False,False,2)
            self.inf_time = Gtk.Label()
            self.inf_time.set_justify(Gtk.Justification.CENTER)
            self.info_box.pack_start(self.inf_time,False,False,2)
            info_sstr = "  disp.fps :     " + "\n" + "  inf.fps :     " + "\n" + "  inf.time :     " + "\n"
            self.inf_time.set_markup("<span font=\'%d\' color='#FFFFFFFF'><b>%s\n</b></span>" % (self.ui_cairo_font_size,info_sstr))

        else :
            # still picture mode
            self.info_box = Gtk.VBox()
            self.info_box.set_name("gui_main_stbox")
            self.st_icon_path = RESOURCES_DIRECTORY + 'PE_st_icon_next_inference_' + self.ui_icon_st_size + 'px' + '.png'
            self.st_icon = Gtk.Image.new_from_file(self.st_icon_path)
            self.st_icon_event = Gtk.EventBox()
            self.st_icon_event.add(self.st_icon)
            self.info_box.pack_start(self.st_icon_event,False,False,20)
            self.inf_time = Gtk.Label()
            self.inf_time.set_justify(Gtk.Justification.CENTER)
            self.info_box.pack_start(self.inf_time,False,False,2)
            info_sstr = "  inf.fps :     " + "\n" + "  inf.time :     " + "\n"
            self.inf_time.set_markup("<span font=\'%d\' color='#FFFFFFFF'><b>%s\n</b></span>" % (self.ui_cairo_font_size,info_sstr))

        # setup video box containing gst stream in camera preview mode
        # and a openCV picture in still picture mode
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
        # setup the exit box which contains the exit button
        self.exit_box = Gtk.VBox()
        self.exit_box.set_name("gui_main_exit")
        self.exit_icon_path = RESOURCES_DIRECTORY + 'exit_' + self.ui_icon_exit_size + 'x' +  self.ui_icon_exit_size + '.png'
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

    def exit_icon_cb(self,eventbox, event):
        """
        Exit callback to close application
        """
        self.destroy()
        Gtk.main_quit()

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
               self.ui_cairo_font_size = 11
               self.ui_cairo_font_size_label = 18
               self.ui_icon_exit_size = '25'
               self.ui_icon_st_size = '52'
        elif window_constraint <= 480:
               #Display 800x480
               self.ui_cairo_font_size = 16
               self.ui_cairo_font_size_label = 29
               self.ui_icon_exit_size = '50'
               self.ui_icon_st_size = '80'
        elif window_constraint <= 600:
               #Display 1024x600
               self.ui_cairo_font_size = 19
               self.ui_cairo_font_size_label = 32
               self.ui_icon_exit_size = '50'
               self.ui_icon_st_size = '120'
        elif window_constraint <= 720:
               #Display 1280x720
               self.ui_cairo_font_size = 23
               self.ui_cairo_font_size_label = 38
               self.ui_icon_exit_size = '50'
               self.ui_icon_st_size = '160'
        elif window_constraint <= 1080:
               #Display 1920x1080
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
            self.st_icon_path = RESOURCES_DIRECTORY + 'PE_st_icon_' + self.ui_icon_st_size + 'px' + '.png'
            self.st_icon = Gtk.Image.new_from_file(self.st_icon_path)
            self.st_icon_event = Gtk.EventBox()
            self.st_icon_event.add(self.st_icon)
            self.info_box.pack_start(self.st_icon_event,False,False,2)
            self.inf_time = Gtk.Label()
            self.inf_time.set_justify(Gtk.Justification.CENTER)
            self.info_box.pack_start(self.inf_time,False,False,2)
            info_sstr = "  disp.fps :     " + "\n" + "  inf.fps :     " + "\n" + "  inf.time :     \n"
            self.inf_time.set_markup("<span font=\'%d\' color='#FFFFFFFF'><b>%s\n</b></span>" % (self.ui_cairo_font_size,info_sstr))
        else :
            # still picture mode
            self.info_box = Gtk.VBox()
            self.info_box.set_name("gui_overlay_stbox")
            self.st_icon_path = RESOURCES_DIRECTORY + 'PE_st_icon_next_inference_' + self.ui_icon_st_size + 'px' + '.png'
            self.st_icon = Gtk.Image.new_from_file(self.st_icon_path)
            self.st_icon_event = Gtk.EventBox()
            self.st_icon_event.add(self.st_icon)
            self.st_icon_event.connect("button_press_event",self.still_picture)
            self.info_box.pack_start(self.st_icon_event,False,False,20)
            self.inf_time = Gtk.Label()
            self.inf_time.set_justify(Gtk.Justification.CENTER)
            self.info_box.pack_start(self.inf_time,False,False,2)
            info_sstr = "  inf.fps :     " + "\n" + "  inf.time :     \n"
            self.inf_time.set_markup("<span font=\'%d\' color='#FFFFFFFF'><b>%s\n</b></span>" % (self.ui_cairo_font_size,info_sstr))

        # setup video box containing a transparent drawing area
        # to draw over the video stream
        self.video_box = Gtk.HBox()
        self.video_box.set_name("gui_overlay_video")
        self.video_box.set_app_paintable(True)
        self.drawing_area = Gtk.DrawingArea()
        self.drawing_area.connect("draw", self.drawing)
        self.drawing_area.set_name("overlay_draw")
        self.drawing_area.set_app_paintable(True)
        self.video_box.pack_start(self.drawing_area, True, True, 0)

        # setup the exit box which contains the exit button
        self.exit_box = Gtk.VBox()
        self.exit_box.set_name("gui_overlay_exit")
        self.exit_icon_path = RESOURCES_DIRECTORY + 'exit_' + self.ui_icon_exit_size + 'x' +  self.ui_icon_exit_size + '.png'
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
        if self.app.first_drawing_call :
            self.app.first_drawing_call = False
            self.drawing_width = widget.get_allocated_width()
            self.drawing_height = widget.get_allocated_height()
            cr.set_font_size(self.ui_cairo_font_size)
            if self.app.enable_camera_preview == False :
                self.app.still_picture_next = True
                if args.validation:
                    GLib.idle_add(self.app.process_picture)
                else:
                    self.app.process_picture()
            return False
        if(self.app.loading_nn):
            # waiting screen
            text = "Loading NN model"
            cr.set_font_size(self.ui_cairo_font_size*3)
            xbearing, ybearing, width, height, xadvance, yadvance = cr.text_extents(text)
            cr.move_to((self.drawing_width/2-width/2),(self.drawing_height/2))
            cr.text_path(text)
            cr.set_source_rgb(0.012,0.137,0.294)
            cr.fill_preserve()
            cr.set_source_rgb(1, 1, 1)
            cr.set_line_width(0.2)
            cr.stroke()
            return True
        else :
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

            # Compute the keypoint coordinates (width/height) ratio
            ar_w = preview_width / self.app.nn_input_width
            ar_h = preview_height / self.app.nn_input_height

            cr.set_line_width(2)
            keypoint_edges = self.app.keypoint_edges
            colors = self.app.edge_colors

            for i in range (len(keypoint_edges)):
                cr.set_source_rgb(colors[i][2], colors[i][1], colors[i][0])
                cr.move_to(int(keypoint_edges[i][0][0]*ar_w+offset), int(keypoint_edges[i][0][1]*ar_h+vertical_offset))
                cr.set_line_width(5)
                cr.line_to(int(keypoint_edges[i][1][0]*ar_w+offset), int(keypoint_edges[i][1][1]*ar_h+vertical_offset))
                cr.stroke()

            return True

    def still_picture(self,  widget, event):
        """
        ST icon cb which trigger a new inference
        """
        self.app.still_picture_next = True
        return self.app.process_picture()

class Application:
    """
    Class that handles the whole application
    """
    def __init__(self, args):
        #init variables uses :
        self.exit_app = False
        self.dcmipp_camera = False
        self.first_drawing_call = True
        self.loading_nn = True
        self.first_call = True
        self.window_width = 0
        self.window_height = 0
        self.get_display_resolution()

        #preview dimensions and fps
        self.frame_width = args.frame_width
        self.frame_height = args.frame_height
        self.framerate = args.framerate

        #instantiate the Neural Network class
        self.nn = NeuralNetwork(args.model_file,
                                float(args.input_mean),
                                float(args.input_std),
                                float(args.conf_threshold),
                                float(args.iou_threshold),
                                bool(args.normalize)
                                )
        self.shape = self.nn.get_img_size()
        self.nn_input_width = self.shape[1]
        self.nn_input_height = self.shape[0]
        self.nn_input_channel = self.shape[2]
        self.nn_inference_time = 0.0
        self.nn_inference_fps = 0.0
        self.nn_result_label = 0
        self.label_to_display = ""

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
        behaving as expected
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

        # remove .json file
        item_to_remove = []
        for item in self.files:
            if item.endswith(".json"):
                item_to_remove.append(item)

        for item in item_to_remove:
            self.files.remove(item)

        index = random.randrange(0, len(self.files))
        file_path = self.files[index]
        self.files.pop(index)
        return file_path

    def load_valid_results_from_json_file(self, json_file):
        """
        Load json files containing expected results for the validation mode
        """
        json_file = json_file + '.json'
        keypoint_locs = []
        with open(args.image + "/" + json_file) as json_file:
            data = json.load(json_file)
            for obj in data['Keypoints_coordinates']:
                number_of_kp = obj['number_of_kp']
                number_of_kp = int(number_of_kp)
                for i in range(0,number_of_kp):
                    xi = "x" + str(i)
                    yi = "y" + str(i)
                    coordinates = []
                    coordinates.append(obj[xi])
                    coordinates.append(obj[yi])
                    keypoint_locs.append(coordinates)
        return keypoint_locs

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

        info_sstr = "  disp.fps :     " + "\n" + str_display_fps + "\n" + "  inf.fps :     " + "\n" + str_inference_fps + "\n" + "  inf.time :     " + "\n"  + str_inference_time + "\n"

        self.overlay_window.inf_time.set_markup("<span font=\'%d\' color='#FFFFFFFF'><b>%s\n</b></span>" % (self.overlay_window.ui_cairo_font_size,info_sstr))


        if args.validation:
            # reload the timeout
            GLib.source_remove(self.valid_timeout_id)
            self.valid_timeout_id = GLib.timeout_add(10000,
                                                     self.valid_timeout_callback)

            self.valid_draw_count = self.valid_draw_count + 1
            # stop the application after defined amount of draws
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
        return True

    def update_label_still(self, inference_time):
        """
        update inference results in still picture mode
        """
        str_inference_time = str("{0:0.1f}".format(inference_time)) + " ms"
        inference_fps = 1000/inference_time
        str_inference_fps = str("{0:.1f}".format(inference_fps)) + " fps"
        info_sstr ="  inf.fps :     " + "\n" + str_inference_fps + "\n" + "  inf.time :     " + "\n"  + str_inference_time + "\n"
        self.overlay_window.inf_time.set_markup("<span font=\'%d\' color='#FFFFFFFF'><b>%s\n</b></span>" % (self.main_window.ui_cairo_font_size,info_sstr))

    def process_picture(self):
        """
        Still picture inference function
        Load the frame, launch inference and
        call functions to refresh UI
        """
        if self.exit_app:
            Gtk.main_quit()
            return False

        if self.still_picture_next:
            # get randomly a picture in the directory
            rfile = self.getRandomFile(args.image)
            img = Image.open(args.image + rfile)
            print("file name : ",rfile)

            # recover drawing box size and picture size
            screen_width = self.overlay_window.drawing_width
            screen_height = self.overlay_window.drawing_height
            picture_width, picture_height = img.size
            print(picture_width, screen_width)

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
            self.keypoint_locs, self.keypoint_edges, self.edge_colors = self.nn.get_results()

            # write information onf the GTK UI
            inference_time = self.nn_inference_time * 1000

            if args.validation and inference_time != 0:
                # reload the timeout
                GLib.source_remove(self.valid_timeout_id)
                self.valid_timeout_id = GLib.timeout_add(100000,
                                                         self.valid_timeout_callback)
                # get file name
                file_name = os.path.basename(rfile)
                # remove the extension
                file_name = os.path.splitext(file_name)[0]
                # store the inference time in a list so that we can compute the
                # average later on
                # retreive associated JSON file information
                keypoints_locs_expected = []
                keypoints_locs_expected = self.load_valid_results_from_json_file(file_name)

                #compare the number of keypoints found in the picture vs expected
                key_found = len(self.keypoint_locs)
                expected_key = len(keypoints_locs_expected)
                print("\texpect %s keypoints. NN model inference found %s keypoints" % (expected_key, key_found))
                if key_found != expected_key :
                   print("Number of keypoints not aligned with the expected validation results\n")
                   os._exit(5)

                found = False
                valid_count = 0

                for i in range(0, key_found):
                    for j in range(0,expected_key):
                            error_epsilon = 0.1
                            x_percent = (self.keypoint_locs[i][0] - float(keypoints_locs_expected[j][0])) / float(keypoints_locs_expected[j][0])
                            y_percent = (self.keypoint_locs[i][1] - float(keypoints_locs_expected[j][1])) / float(keypoints_locs_expected[j][1])
                            if abs(x_percent) <= error_epsilon and abs(y_percent) <= error_epsilon:
                                    found = True
                                    if found :
                                        valid_count += 1
                                        found = False
                                        break
                            elif j>=expected_key-1:
                              print("No match found for keypoint: %s; %s" % (self.keypoint_locs[i][0], self.keypoint_locs[i][1]))
                              os._exit(5)
                    print("X values found {}, expected {}".format(self.keypoint_locs[i][0], float(keypoints_locs_expected[j][0])))
                    print("Y values found {}, expected {}".format(self.keypoint_locs[i][1], float(keypoints_locs_expected[j][1])))

                if (valid_count != expected_key) :
                   print("Inference result not aligned with the expected validation results\n")
                   os._exit(5)
                valid_count = 0

                # store the inference time in a list so that we can compute the
                # average later on
                self.valid_inference_time.append(round(self.nn_inference_time * 1000, 4))

                # process all the file
                if len(self.files) == 0:
                    avg_inf_time = sum(self.valid_inference_time) / len(self.valid_inference_time)
                    print("\navg inference time= " + str(avg_inf_time) + " ms")
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
        self.main_window.connect("delete-event", Gtk.main_quit)
        self.main_window.show_all()
        self.overlay_window.connect("delete-event", Gtk.main_quit)
        self.overlay_window.show_all()
        # start a timeout timer in validation process to close application if
        # timeout occurs
        if args.validation:
            self.valid_timeout_id = GLib.timeout_add(100000,
                                                     self.valid_timeout_callback)
        return True

if __name__ == '__main__':
    # add signal to catch CRTL+C
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    #Tensorflow Lite NN intitalisation
    parser = argparse.ArgumentParser()
    parser.add_argument("-m", "--model_file", default="", help="NN model to be executed")
    parser.add_argument("-i", "--image", default="", help="image directory with image to be classified")
    parser.add_argument("-v", "--video_device", default="", help="video device ex: video0")
    parser.add_argument("--conf_threshold", default=0.75, help="Confidence threshold")
    parser.add_argument("--iou_threshold", default=0.5, help="IoU threshold, used to compute NMS")
    parser.add_argument("--frame_width", default=640, help="width of the camera frame (default is 640)")
    parser.add_argument("--frame_height", default=480, help="height of the camera frame (default is 480)")
    parser.add_argument("--framerate", default=15, help="framerate of the camera (default is 15fps)")
    parser.add_argument("--input_mean", default=255, help="input mean")
    parser.add_argument("--input_std", default=0, help="input standard deviation")
    parser.add_argument("--normalize", default=False, help="input standard deviation")
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
    print("gtk main finished")
    print("application exited properly")
    os._exit(0)