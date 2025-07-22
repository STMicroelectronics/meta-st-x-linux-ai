#!/usr/bin/python3
#
# Copyright (c) 2024 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

import gi
import signal
import argparse
import os
import time
import socket
import numpy as np
import subprocess
import serial
import copy

from gi.repository import Gst, GLib
from supervision.detection.core import Detections
from supervision.annotators.utils import Trace
from supervision.tracker.byte_tracker.core import ByteTrack
from os import path
from timeit import default_timer as timer
from PIL import Image
from yolov8_pp_annotator import NeuralNetwork, Trace, BoxAnnotator, TraceAnnotator

# Initialize GLib and GStreamer
gi.require_version('Gst', '1.0')
GLib.threads_init()
Gst.init(None)
Gst.init_check(None)

# Create a GLib main loop
loop = GLib.MainLoop()

class GstPipeline():
    """
    Class that handles Gstreamer pipeline and appsink
    """
    def __init__(self, app, nn):
        self.instant_fps = 0
        self.app = app
        self.nn = nn
        self.inference_result_save = []
        # gstreamer pipeline creation
        self.pipeline = Gst.Pipeline()

        # creation of the source element
        self.libcamerasrc = Gst.ElementFactory.make("libcamerasrc", "libcamera")
        if not self.libcamerasrc:
            raise Exception("Could not create Gstreamer camera source element")

        # creation of the libcamerasrc caps for the 2 pipelines
        caps = f"video/x-raw,width={args.frame_width},height={args.frame_height},format=YUY2,interlace-mode=progressive,framerate=29/1"
        print("Main pipe configuration: ", caps)
        caps_src = Gst.Caps.from_string(caps)

        caps1 = "video/x-raw,width=" + str(self.app.nn_input_width) + ",height=" + str(self.app.nn_input_height) + ",format=BGR"
        print("Aux pipe configuration:  ", caps1)
        caps_src0 = Gst.Caps.from_string(caps1)

        # creation of the queues elements
        self.queuejpeg  = Gst.ElementFactory.make("queue", "queuev4l2jpegenc")
        # Only one buffer in the queue0 to get 30fps on the display preview pipeline
        self.queueuvc = Gst.ElementFactory.make("queue", "queueuvcsink")

        # Only one buffer in the queue0 to get 30fps on the display preview pipeline
        self.queue0 = Gst.ElementFactory.make("queue", "queue0")
        self.queue0.set_property("leaky", 2)  # 0: no leak, 1: upstream (oldest), 2: downstream (newest)
        self.queue0.set_property("max-size-buffers", 1)  # Maximum number of buffers in the queue

        # creation of display pipeline elements
        self.v4l2jpegenc = Gst.ElementFactory.make("v4l2jpegenc", "v4l2jpegenc")
        self.v4l2jpegenc.set_property("output-io-mode", 5)

        device_name = self.get_video_device_for_uvc()
        self.uvcsink = Gst.ElementFactory.make("uvcsink", "uvcsink")
        v4l2sink = self.uvcsink.get_child_by_name("v4l2sink")
        v4l2sink.set_property("device", device_name)

        # creation and configuration of the appsink element for nn pipeline
        self.videoconvert = Gst.ElementFactory.make("videoconvert", "videoconvert")

        self.appsink = Gst.ElementFactory.make("appsink", "appsink")
        self.appsink.set_property("emit-signals", True)
        self.appsink.set_property("sync", False)
        self.appsink.set_property("max-buffers", 1)
        self.appsink.set_property("drop", True)
        self.appsink.connect("new-sample", self.new_sample)

        # Add all elements to the pipeline
        self.pipeline.add(self.libcamerasrc)

        ## Display
        self.pipeline.add(self.uvcsink)
        self.pipeline.add(self.v4l2jpegenc)
        self.pipeline.add(self.queuejpeg)
        self.pipeline.add(self.queueuvc)

        ## NN
        self.pipeline.add(self.queue0)
        self.pipeline.add(self.videoconvert)
        self.pipeline.add(self.appsink)

        # linking elements together
        ##
        ##              | src   --> queuejpeg  [caps_src]  --> v4l2jpegenc output-io-mode=5 --> queueuvc --> uvcsink v4l2sink::device=/dev/video7
        ## libcamerasrc |
        ##              | src_0 --> queue0 [caps_src0] --> videoconvert --> appsink
        ##

        # Display pipe
        self.queuejpeg.link_filtered(self.v4l2jpegenc, caps_src)
        self.v4l2jpegenc.link(self.queueuvc)
        self.queueuvc.link(self.uvcsink)

        # NN pipe
        self.queue0.link_filtered(self.videoconvert, caps_src0)
        self.videoconvert.link(self.appsink)

        # libcamera pad
        self.src_pad = self.libcamerasrc.get_static_pad("src")
        self.src_request_pad_template = self.libcamerasrc.get_pad_template("src_%u")
        self.src_request_pad0 = self.libcamerasrc.request_pad(self.src_request_pad_template, None, None)
        queue_sink_pad = self.queuejpeg.get_static_pad("sink")
        queue0_sink_pad = self.queue0.get_static_pad("sink")

        # view-finder
        self.src_request_pad0.set_property("stream-role", 3)

        # video-recording
        self.src_pad.set_property("stream-role", 2)

        self.src_pad.link(queue_sink_pad)
        self.src_request_pad0.link(queue0_sink_pad)

        os.system('v4l2-ctl -d /dev/v4l-subdev3 -c gamma_correction=1')
        os.system('v4l2-ctl -d /dev/v4l-subdev4 -c gamma_correction=1')

        # getting pipeline bus
        self.bus = self.pipeline.get_bus()
        self.bus.add_signal_watch()
        self.bus.connect('message::error', self.msg_error_cb)
        self.bus.connect('message::eos', self.msg_eos_cb)
        self.bus.connect('message::info', self.msg_info_cb)
        self.bus.connect('message::application', self.msg_application_cb)
        self.bus.connect('message::state-changed', self.msg_state_changed_cb)

        # set pipeline playing mode
        self.pipeline.set_state(Gst.State.PLAYING)

    def get_video_device_for_uvc(self):
        cmd = "v4l2-ctl --list-devices"
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True)

        # Parse the output
        lines = result.stdout.splitlines()
        device_name = None
        for i, line in enumerate(lines):
            if "(gadget." in line:
                # The next line should contain the device path
                if i + 1 < len(lines):
                    device_name = lines[i + 1].strip()
                    break

        return device_name

    def msg_eos_cb(self, bus, message):
        print('eos message -> {}'.format(message))

    def msg_info_cb(self, bus, message):
        print('info message -> {}'.format(message))

    def msg_error_cb(self, bus, message):
        print('error message -> {}'.format(message.parse_error()))

    def msg_state_changed_cb(self, bus, message):
        oldstate,newstate,pending = message.parse_state_changed()
        if (oldstate == Gst.State.NULL) and (newstate == Gst.State.READY):
            Gst.debug_bin_to_dot_file(self.pipeline, Gst.DebugGraphDetails.ALL,"pipeline_py_NULL_READY")

    def msg_application_cb(self, bus, message):
        if message.get_structure().get_name() == 'inference-done':
            self.app.nn._nn_finished = True
            if(self.app.loading_nn):
                self.app.loading_nn = False

    def gst_to_nparray(self,sample):
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
        arr = np.ndarray(
            (number_of_lines,
             number_of_column,
             channels),
            buffer=buf.extract_dup(0, buf.get_size()),
            dtype=np.uint8)
        return arr

    def new_sample(self,*data):
        """
        recover video frame from appsink
        and run inference
        """
        global image_arr
        sample = self.appsink.emit("pull-sample")
        arr = self.gst_to_nparray(sample)
        if arr is not None :
            self.nn.launch_inference(arr)
            self.app.nn_result_locations = self.nn.get_results()

            nn_result_copy = copy.deepcopy(self.app.nn_result_locations)
            nn_result_copy = np.array(nn_result_copy)

            if len(nn_result_copy):
                self.app.nn_detections.xyxy = nn_result_copy[:,:4]
                self.app.nn_detections.confidence = nn_result_copy[:,4]
                self.app.nn_detections.class_id = nn_result_copy[:,5].astype(np.int32)
            else:
                self.app.nn_detections = self.app.nn_detections.empty()

            self.app.nn_detections = self.app.nn_tracker.update_with_detections(self.app.nn_detections)

            # Insert BOF (Begin of Frame) / space behind
            message = "/B "
            message_info = ""
            counter_detection = 0

            detections = self.app.nn_detections
            self.app.nn_trace.put(detections)
            for detection_idx in range(len(detections)):
                counter_detection +=1
                # Scale NN outputs for the display before drawing
                x0, y0, x1, y1 = detections.xyxy[detection_idx]
                trackerid = detections.tracker_id[detection_idx].astype(int)
                xy = self.app.nn_trace.get(tracker_id=trackerid)

                passed = False
                found = False
                list_index = 0
                pourcent_x = 2/100
                pourcent_y = 2/100
                if self.inference_result_save == []:
                    self.inference_result_save.append([trackerid, x0, y0, x1, y1])
                elif self.inference_result_save != []:
                    for i in range (len(self.inference_result_save)):
                        list =  self.inference_result_save[i]
                        if list[0] == trackerid:
                            found = True
                            list_index = i
                            if list[1] < (x0+pourcent_x*x0) and list[1] > (x0-pourcent_x*x0):
                                if list[2] < (y0+pourcent_y*y0) and list[2] > (y0-pourcent_y*y0):
                                    if list[3] < (x1+pourcent_x*x1) and list[3] > (x1-pourcent_x*x1):
                                        if list[4] < (y1+pourcent_y*y1) and list[4] > (y1-pourcent_y*y1):
                                            passed = True

                    if not found:
                        self.inference_result_save.append([trackerid, x0, y0, x1, y1])
                    else:
                        if not passed:
                            del self.inference_result_save[list_index]
                        else:
                            trackerid = self.inference_result_save[list_index][0]
                            x0 = self.inference_result_save[list_index][1]
                            y0 = self.inference_result_save[list_index][2]
                            x1 = self.inference_result_save[list_index][3]
                            y1 = self.inference_result_save[list_index][4]

                # Send to serial
                message_info += "#" + str(trackerid)
                message_info += "#" + str(format(x0, ".3f")) + "#" + str(format(y0, ".3f"))
                message_info += "#" + str(format(x1, ".3f")) + "#" + str(format(y1, ".3f"))

                for i in range(30):
                    if i < len(xy):
                        message_info += "#" + str(format(xy[i][0], ".3f"))
                        message_info += "#" + str(format(xy[i][1], ".3f"))
                    else:
                        message_info += "#0#0"

            # Compose message + EOF
            message += str(counter_detection) + message_info + "/E"

            try:
                ser.write(message.encode('utf-8'))
            except serial.SerialException as e:
                print("unable to transmit message through serial")
                struc = Gst.Structure.new_empty("inference-done")
                msg = Gst.Message.new_application(None, struc)
                self.bus.post(msg)
                loop.quit()
                return Gst.FlowReturn.ERROR

            struc = Gst.Structure.new_empty("inference-done")
            msg = Gst.Message.new_application(None, struc)
            self.bus.post(msg)
        return Gst.FlowReturn.OK

class Application:
    """
    Class that handles the whole application
    """
    def __init__(self, args):
        #init variables uses :
        self.loading_nn = True
        self.camera_caps = "video/x-raw, format=RGB16, width=640, height=480"

        #instantiate the Neural Network class
        self.nn = NeuralNetwork(args.model_file, float(args.input_mean), float(args.input_std), args.maximum_detection, args.threshold, args.iou_threshold)
        self.shape = self.nn.get_img_size()
        self.nn_input_width = self.shape[1]
        self.nn_input_height = self.shape[0]
        self.nn_input_channel = self.shape[2]
        self.nn_inference_time = 0.0
        self.nn_inference_fps = 0.0
        self.nn_result_locations = []

        #instantiate the detections
        self.nn_detections = Detections.empty()
        #instantiate the bounding box annotator
        self.nn_annotator_boundingbox = BoxAnnotator()
        self.nn_annotator_trace = TraceAnnotator()
        #instantiate the tracker
        self.nn_tracker = ByteTrack()
        self.nn_trace = Trace(max_size=30) # trace lasts 30 frames

        #instantiate the Gstreamer pipeline
        self.gst_pipeline = GstPipeline(self, self.nn)
        self.main()

    def main(self):
        return True

if __name__ == '__main__':
    # add signal to catch CRTL+C
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    # Parser initialization
    parser = argparse.ArgumentParser()
    parser.add_argument("--frame_width", default=1280, help="width of the camera frame (default is 1280)")
    parser.add_argument("--frame_height", default=720, help="height of the camera frame (default is 720)")
    parser.add_argument("-m", "--model_file", default="", help=".nb model to be executed")
    parser.add_argument("--input_mean", default=127.5, help="input mean")
    parser.add_argument("--input_std", default=127.5, help="input standard deviation")
    parser.add_argument("--num_threads", default=None, help="Select the number of threads used by tflite interpreter to run inference")
    parser.add_argument("--maximum_detection", default=10, type=int, help="Adjust the maximum number of object detected in a frame accordingly to your NN model (default is 10)")
    parser.add_argument("--threshold", default=0.40, type=float, help="threshold of accuracy above which the boxes are displayed (default 0.60)")
    parser.add_argument("--iou_threshold", default=0.50, type=float, help="intersection over union threshold (default 0.50)")
    parser.add_argument("--debug", default=False, action='store_true', help=argparse.SUPPRESS)
    args = parser.parse_args()

    # Set serial com port and baudrate
    PORT, BAUDRATE = "/dev/ttyGS0", 9600

    while True:
        try:
            ser = serial.Serial(PORT, BAUDRATE, timeout=1)
            print(f"Connected to {PORT} at {BAUDRATE} baud.")

            try:
                application = Application(args)
                loop.run()
                application.gst_pipeline.pipeline.set_state(Gst.State.NULL)
                del application

            except Exception as exc:
                print("Main Exception: ", exc )
                loop.quit()

        except serial.SerialException as e:
            print(f"connection to com port {PORT} is failing -> ", str(e))
            time.sleep(5)

    # Start the main loop
    loop.run()
    print("application exited properly")
    loop.stop()
    os._exit(0)