#!/usr/bin/python3
#
# Copyright (c) 2020 STMicroelectronics. All rights reserved.
#
# This software component is licensed by ST under BSD 3-Clause license,
# the "License"; You may not use this file except in compliance with the
# License. You may obtain a copy of the License at:
#                        opensource.org/licenses/BSD-3-Clause
#

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk
from gi.repository import GObject
from gi.repository import Gdk
from gi.repository import GLib
from gi.repository import GdkPixbuf

import argparse
import platform
import numpy as np
import cv2
import signal
import ctypes

import time
from PIL import Image, ImageDraw

import sys
import os
import random
import platform
import operator
import collections
from multiprocessing import Process, Event, Array, Value
from timeit import default_timer as timer
from threading import Thread
import tflite_edgetpu_runtime.interpreter as tflite

EDGETPU_SHARED_LIB = "libedgetpu.so.1"

char_text_width = 6

class NeuralNetwork:
    """
    Class that handles Neural Network inference
    """
    def __init__(self, model_file, label_file, input_mean, input_std):
        """
        :param model_path: .tflite model to be executedname of file containing labels")
        :param label_file:  name of file containing labels
        :param input_mean: input_mean
        :param input_std: input standard deviation
        """
        def load_labels(filename):
            my_labels = []
            input_file = open(filename, 'r')
            for l in input_file:
                my_labels.append(l.strip())
            return my_labels

        self._model_file = model_file
        self._label_file = label_file
        self._input_mean = input_mean
        self._input_std  = input_std
        self._floating_model = False

        self._interpreter = tflite.Interpreter(self._model_file,
                                               experimental_delegates=[tflite.load_delegate(EDGETPU_SHARED_LIB)])
        self._interpreter.allocate_tensors()
        self._input_details  = self._interpreter.get_input_details()
        self._output_details = self._interpreter.get_output_details()

        # check the type of the input tensor
        if self._input_details[0]['dtype'] == np.float32:
            self._floating_model = True
            print("Floating point Tensorflow Lite Model")

        self._labels = load_labels(self._label_file)

    def __getstate__(self):
        return (self._model_file, self._label_file, self._input_mean,
                self._input_std, self._floating_model,
                self._input_details, self._output_details, self._labels)

    def __setstate__(self, state):
        self._model_file, self._label_file, self._input_mean, \
                self._input_std, self._floating_model, \
                self._input_details, self._output_details, self._labels = state

        self._interpreter = tflite.Interpreter(self._model_file,
                                            experimental_delegates=[tflite.load_delegate(EDGETPU_SHARED_LIB)])
        self._interpreter.allocate_tensors()

    def get_labels(self):
        return self._labels

    def get_img_size(self):
        """
        :return: size of NN input image size
        """
        # NxHxWxC, H:1, W:2, C:3
        return (int(self._input_details[0]['shape'][1]),
                int(self._input_details[0]['shape'][2]),
                int(self._input_details[0]['shape'][3]))

    def launch_inference(self, img):
        # add N dim
        input_data = np.expand_dims(img, axis=0)
        if self._floating_model:
            input_data = (np.float32(input_data) - self._input_mean) / self._input_std
        self._interpreter.set_tensor(self._input_details[0]['index'], input_data)
        start = time.perf_counter()
        self._interpreter.invoke()
        #print('Invoke func time : %.2fms' % ((time.perf_counter() - start) * 1000))

    def get_results(self):
        # display output results
        locations = self._interpreter.get_tensor(self._output_details[0]['index'])
        classes   = self._interpreter.get_tensor(self._output_details[1]['index'])
        scores    = self._interpreter.get_tensor(self._output_details[2]['index'])
        nb_detect = self._interpreter.get_tensor(self._output_details[3]['index'])
        return (locations, classes, scores)


class FrameCapture:
    """Camera object that controls video caping"""
    def __init__(self,_frame_width, _frame_height,_frame_rate, _device_addr):
        self.cap = cv2.VideoCapture(_device_addr)
        assert self.cap.isOpened()
        #ret = self.cap.set(cv2.CAP_PROP_FOURCC, cv2.VideoWriter_fourcc(*'MJPG'))
        ret = self.cap.set(cv2.CAP_PROP_FPS, _frame_rate)
        ret = self.cap.set(cv2.CAP_PROP_FRAME_WIDTH,_frame_width)
        ret = self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT,_frame_height)
        # Read first frame from the cap
        #assert self.cap.grab(), "Couldn't grab the VideoCapture"
        (self.grabbed, self.frame) = self.cap.read()

        # Variable to control when the camera is stopped
        self.stopped = False

    def start(self):
        # Start the thread that reads frames from the video cap
        Thread(target=self.update,args=()).start()
        return self

    def update(self):
        # Keep looping indefinitely until the thread is stopped
        while True:
            # If the camera is stopped, stop the thread
            if self.stopped:
                # Close camera resources
                self.cap.release()
                return
            # Otherwise, grab the next frame from the cap
            (self.grabbed, self.frame) = self.cap.read()

    def read(self):
        # Return the most recent frame
        return self.frame

    def stop(self):
        # Indicate that the camera and thread should be stopped
        self.stopped = True

class MainUIWindow(Gtk.Window):
    def __init__(self, args):
        Gtk.Window.__init__(self, title=os.path.basename(args.model_file))
        if args.image == "":
            self.enable_camera_preview = True
        else:
            self.enable_camera_preview = False
        self.maximize()
        self.screen_width = self.get_screen().get_width()
        self.screen_height = self.get_screen().get_height()

        if self.screen_width == 720:
            self.picture_width = 480
            self.picture_height = 360
        else:
            self.picture_width = 320
            self.picture_height = 240

        self.set_position(Gtk.WindowPosition.CENTER)
        self.connect('destroy', Gtk.main_quit)

        self.vbox = Gtk.VBox()
        self.add(self.vbox)

        self.progressbar = Gtk.ProgressBar()
        self.vbox.pack_start(self.progressbar, False, False, 15)

        self.hbox = Gtk.HBox()
        self.vbox.pack_start(self.hbox, False, False, 15)

        self.image = Gtk.Image()
        self.hbox.pack_start(self.image, False, False, 15)

        self.label = Gtk.Label()
        self.label.set_size_request(400, -1) # -1 to keep height automatic
        self.label.set_alignment(0, 0)
        self.label.set_line_wrap(True)
        self.label.set_line_wrap_mode(Gtk.WrapMode.WORD)
        self.hbox.pack_start(self.label, False, False, 15)

        self.timeout_id = GLib.timeout_add(50, self.on_timeout)

    def on_timeout(self):
        self.progressbar.pulse()
        return True

    def update_preview(self, inference_time, inference_fps):
        str_inference_time = str("{0:0.1f}".format(inference_time))
        str_inference_fps = str("{0:.1f}".format(inference_fps))

        self.label.set_markup("<span font='10' color='#002052FF'><b>inference @%sfps\n\n\n</b></span>"
                              "<span font='15' color='#002052FF'><b>inference time: %sms\n</b></span>"
                              % (str_inference_fps, str_inference_time))

    def update_still(self, inference_time):
        str_inference_time = str("{0:0.1f}".format(inference_time))

        self.label.set_markup("<span font='15' color='#002052FF'><b>inference time: %sms\n</b></span>"
                              % (str_inference_time))

    def update_frame(self, frame, labels):
        img = Image.fromarray(frame)
        draw = ImageDraw.Draw(img)

        if self.result_scores[0][0] > 0.5:
            y0 = int(self.result_locations[0][0][0] * frame.shape[0])
            y1 = int(self.result_locations[0][0][2] * frame.shape[0])
            x1 = int(self.result_locations[0][0][3] * frame.shape[1])
            x0 = int(self.result_locations[0][0][1] * frame.shape[1])
            label = labels[int(self.result_classes[0][0])]
            accuracy = self.result_scores[0][0] * 100
            draw.rectangle([(x0, y0), (x1, y1)], outline=(0,0,255))
            draw.rectangle([(x0, y0), (x0 + ((len(label) + 4) * char_text_width), y0 + 14)], (0,0,255))
            draw.text((x0 + 2, y0 + 2), label + " " + str(int(accuracy)) + "%", (255,255,255))

        if self.result_scores[0][1] > 0.5:
            y0 = int(self.result_locations[0][1][0] * frame.shape[0])
            x0 = int(self.result_locations[0][1][1] * frame.shape[1])
            y1 = int(self.result_locations[0][1][2] * frame.shape[0])
            x1 = int(self.result_locations[0][1][3] * frame.shape[1])
            label = labels[int(self.result_classes[0][1])]
            accuracy = self.result_scores[0][1] * 100
            draw.rectangle([(x0, y0), (x1, y1)], outline=(255,0,0))
            draw.rectangle([(x0, y0), (x0 + ((len(label) + 4) * char_text_width), y0 + 14)], (255,0,0))
            draw.text((x0 + 2, y0 + 2), label + " " + str(int(accuracy)) + "%", (255,255,255))

        if self.result_scores[0][2] > 0.5:
            y0 = int(self.result_locations[0][2][0] * frame.shape[0])
            x0 = int(self.result_locations[0][2][1] * frame.shape[1])
            y1 = int(self.result_locations[0][2][2] * frame.shape[0])
            x1 = int(self.result_locations[0][2][3] * frame.shape[1])
            label = labels[int(self.result_classes[0][2])]
            accuracy = self.result_scores[0][2] * 100
            draw.rectangle([(x0, y0), (x1, y1)], outline=(0,255,0))
            draw.rectangle([(x0, y0), (x0 + ((len(label) + 4) * char_text_width), y0 + 14)], (0,255,0))
            draw.text((x0 + 2, y0 + 2), label + " " + str(int(accuracy)) + "%", (255,255,255))

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

    # termination function
    def terminate(self):
        print("Main: termination")

    # get random file in a directory
    def getRandomFile(self, path):
        # Returns a random filename, chosen among the files of the given path.
        files = os.listdir(path)
        index = random.randrange(0, len(files))
        return files[index]

    # GTK still picture function
    def inference_picture(self, button):
        #input("Press Enter to process new inference...")
        GLib.source_remove(self.timeout_id)
        self.progressbar.hide()
        self.nn_inference_finish = False
        # get randomly a picture in the directory
        rfile = self.getRandomFile(args.image)
        print("Picture ", args.image + "/" + rfile)
        img = Image.open(args.image + "/" + rfile)
        prev_frame = cv2.resize(np.array(img), (self.picture_width, self.picture_height))
        array_base = Array(ctypes.c_uint8, self.input_shape[0] * self.input_shape[1] * self.input_shape[2])
        self.nn_img = np.ctypeslib.as_array(array_base.get_obj())
        self.nn_img = self.nn_img.reshape(self.input_shape[0], self.input_shape[1], self.input_shape[2])
        # update the preview frame
        self.update_frame(prev_frame, self.labels)
        # execute the inference
        nn_frame = cv2.resize(prev_frame, (self.nn_img.shape[1],  self.nn_img.shape[0]))
        self.nn_img[:, :, :] = nn_frame
        self.nn_inference_start = True
        #while not self.nn_processing_finished.value:
        #    pass
        if self.nn_inference_start == True:
            self.nn_inference_start = False
            start = time.perf_counter()
            self.nn.launch_inference(self.nn_img)
            #print("ok3\n")
            inference_time = time.perf_counter() - start
            #print("ok4\n")
            print('Inference time : %.8fms' % (inference_time * 1000))
            self.result_locations[:, :, :], self.result_classes[:, :], self.result_scores[:, :] = self.nn.get_results()
            inference_time = inference_time * 1000
            self.update_label_still(inference_time)
        return True


    # GTK camera preview function
    def inference_camera(self):
        cap_img = self.video_stream.read()
        frame = cap_img[:, :, ::-1].copy()
        loop_stop = time.perf_counter();
        processing_start = time.perf_counter()
        frame_crop = frame[self.y1:self.y2, self.x1:self.x2]
        #frame_crop_RGB = cv2.cvtColor(frame_crop,cv2.COLOR_BGR2RGB)
        frame_crop_RGB_resize = cv2.resize(frame_crop, (self.input_shape[1], self.input_shape[0]))
        self.nn_inference_start = False
        img = Image.fromarray(frame_crop_RGB_resize)
        start = time.perf_counter()
        self.nn.launch_inference(img)
        inference_time  = time.perf_counter() - start
        #print('Processing and Inference time : %.2fms' % ((time.perf_counter() - processing_start) * 1000))
        #print('Inference time : %.2fms' % ((time.perf_counter() - start) * 1000))
        self.loop_time  = loop_stop - self.loop_start
        self.loop_start = loop_stop
        self.total_time = self.total_time + self.loop_time
        if self.loop_count==5:
            self.inference_fps = self.loop_count / self.total_time
            self.loop_count = 0
            self.total_time = 0
        self.loop_count = self.loop_count + 1
        self.nn_inference_finish = True
        self.result_locations[:, :, :], self.result_classes[:, :], self.result_scores[:, :] = self.nn.get_results()
        print("Inference fps %.5f" % self.inference_fps)
        inference_time = inference_time * 1000
        inference_fps  = self.inference_fps
        self.update_preview(inference_time, inference_fps)
        # update the preview frame
        self.update_frame(frame_crop, self.labels)
        return True


    def main(self, args):
        self.nn                     = NeuralNetwork(args.model_file, args.label_file, float(args.input_mean), float(args.input_std))
        self.input_shape            = self.nn.get_img_size()
        self.labels                 = self.nn.get_labels()

        self.nn_inference_start     = True
        self.nn_inference_finish    = True
        self.frame_rate             = 150

        self.result_locations_shared_array = Array(ctypes.c_float, 1 * 10 * 4)
        self.result_locations = np.ctypeslib.as_array(self.result_locations_shared_array.get_obj())
        self.result_locations = self.result_locations.reshape(1, 10, 4)

        self.result_classes_shared_array = Array(ctypes.c_float, 1 * 10)
        self.result_classes = np.ctypeslib.as_array(self.result_classes_shared_array.get_obj())
        self.result_classes = self.result_classes.reshape(1, 10)

        self.result_scores_shared_array = Array(ctypes.c_float, 1 * 10)
        self.result_scores = np.ctypeslib.as_array(self.result_scores_shared_array.get_obj())
        self.result_scores = self.result_scores.reshape(1, 10)


        if self.enable_camera_preview:
            print("Running Inferences on camera input")
            #variable to compute inference framerate
            self.loop_count         = 1
            self.loop_time          = 0
            self.loop_start         = 0
            self.total_time         = 0
            self.inference_fps      = 0
            self.display_fps        = 0
            self.inference_time     = 0
            self.camera_not_started = True

            # initialize VideoFrameCapture object
            self.video_stream = FrameCapture(args.frame_width, args.frame_height, args.framerate, args.video_device)
            self.video_stream.start()

            self.camera_not_started = False

            self.y1 = int(0)
            self.y2 = int(self.input_shape[0])
            self.x1 = int((self.input_shape[1] - self.input_shape[0]) / 2)
            self.x2 = int(self.x1 + self.input_shape[0])

            GLib.source_remove(self.timeout_id)
            self.progressbar.hide()

            GLib.idle_add(self.inference_camera)

        else :
            print("Running Inferences on picture input")
            self.button = Gtk.Button.new_with_label("Next inference")
            self.vbox.pack_start(self.button, False, False, 15)
            self.button.show_all()
            self.button.connect("clicked", self.inference_picture)


def destroy_window(gtkobject):
    gtkobject.terminate()
    print("destroy")


if __name__ == '__main__':
    # add signal to catch CRTL+C
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    #Tensorflow Lite NN intitalisation
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--image", default="", help="image directory with image to be classified")
    parser.add_argument("-v", "--video_device", default=0, help="video device (default /dev/video0)")
    parser.add_argument("--frame_width", default=320, help="width of the camera frame (default is 320)")
    parser.add_argument("--frame_height", default=240, help="height of the camera frame (default is 240)")
    parser.add_argument("--framerate", default=30, help="framerate of the camera (default is 15fps)")
    parser.add_argument("-m", "--model_file", default="detect_edgetpu.tflite", help=".tflite model to be executed")
    parser.add_argument("-l", "--label_file", default="labels.txt", help="name of file containing labels")
    parser.add_argument("--input_mean", default=127.5, help="input mean")
    parser.add_argument("--input_std", default=127.5, help="input standard deviation")
    parser.add_argument("--top_k", type = int, default = 5, help=" The top_k classes to show")
    args = parser.parse_args()

    try:
        win = MainUIWindow(args)
        win.connect("delete-event", Gtk.main_quit)
        win.connect("destroy", destroy_window)
        win.show_all()
        thread = Thread(target =  win.main, args = (args,))
        thread.daemon = True
        thread.start()
    except Exception as exc:
        print("Main Exception: ", exc )

    Gtk.main()
