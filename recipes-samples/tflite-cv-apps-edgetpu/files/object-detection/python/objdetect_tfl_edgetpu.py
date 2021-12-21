#!/usr/bin/python3
#
# Copyright (c) 2020 STMicroelectronics. All rights reserved.
#
# This software component is licensed by ST under BSD 3-Clause license,
# the "License"; You may not use this file except in compliance with the
# License. You may obtain a copy of the License at:
#
#     http://www.opensource.org/licenses/BSD-3-Clause

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk
from gi.repository import GObject
from gi.repository import Gdk
from gi.repository import GLib
from gi.repository import GdkPixbuf

import time
from PIL import Image, ImageDraw
import argparse
import numpy as np
import cv2
import re
import subprocess
import signal
import os
import random
import json
import ctypes
from multiprocessing import Array
from threading import Thread
import tflite_runtime.interpreter as tflite

char_text_width = 6

class VideoFrameCapture:
    """
    Class that handles video capture from device
    """
    def __init__(self, device=0, width=320, height=240, fps=15):
        """
        :param device: device index or video filename
        :param width:  width of the requested frame
        :param height: heigh of the requested frame
        :param fps:    framerate of the camera
        """
        self.cap = cv2.VideoCapture(device,cv2.CAP_V4L2)
        assert self.cap.isOpened()
        ret = self.cap.set(cv2.CAP_PROP_FPS, fps)
        ret = self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        ret = self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
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
            # Stop the thread if the camera is stopped
            if self.stopped:
                self.cap.release()
                return
            # Otherwise, grab the next frame from the cap
            (self.grabbed, self.frame) = self.cap.read()

    def read(self):
        # Return the most recent captured frame
        return self.frame

    def stop(self):
        # Indicate that the camera and thread should be stopped
        self.stopped = True

class NeuralNetwork:
    """
    Class that handles Neural Network inference
    """
    def __init__(self, model_file, label_file, perf):
        """
        :param model_path: .tflite model to be executedname of file containing labels")
        :param label_file:  name of file containing labels
        """
        def load_labels(filename):
            my_labels = []
            input_file = open(filename, 'r')
            for l in input_file:
                my_labels.append(l.strip())
            return my_labels

        self._model_file = model_file
        self._label_file = label_file
        self._floating_model = False

        if perf == 'max':
            self._lib_edgetpu = "libedgetpu-max.so.2"
        elif perf == 'high':
            self._lib_edgetpu = "libedgetpu-high.so.2"
        elif perf == 'med':
            self._lib_edgetpu = "libedgetpu-med.so.2"
        elif perf == 'low':
            self._lib_edgetpu = "libedgetpu-low.so.2"

        self._interpreter = tflite.Interpreter(self._model_file,
                                               experimental_delegates=[tflite.load_delegate(self._lib_edgetpu)])
        self._interpreter.allocate_tensors()
        self._input_details = self._interpreter.get_input_details()
        self._output_details = self._interpreter.get_output_details()

        # check the type of the input tensor
        if self._input_details[0]['dtype'] == np.float32:
            self._floating_model = True
            print("Floating point Tensorflow Lite Model")

        self._labels = load_labels(self._label_file)

    def __getstate__(self):
        return (self._model_file, self._label_file,
                self._input_details, self._output_details, self._labels)

    def __setstate__(self, state):
        self._model_file, self._label_file, \
                self._input_details, self._output_details, self._labels = state

        self._interpreter = tflite.Interpreter(self._model_file,
                                            experimental_delegates=[tflite.load_delegate(self._lib_edgetpu)])
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
        """
        This method launches inference using the invoke call
        :param img: the image to be inferenced
        """
        # add N dim
        input_data = np.expand_dims(img, axis=0)

        self._interpreter.set_tensor(self._input_details[0]['index'], input_data)
        self._interpreter.invoke()

    def get_results(self):
        # display output results
        locations = self._interpreter.get_tensor(self._output_details[0]['index'])
        classes   = self._interpreter.get_tensor(self._output_details[1]['index'])
        scores    = self._interpreter.get_tensor(self._output_details[2]['index'])
        nb_detect = self._interpreter.get_tensor(self._output_details[3]['index'])

        return (locations, classes, scores)


class MainUIWindow(Gtk.Window):
    def __init__(self, args):
        Gtk.Window.__init__(self)

        # set the title bar
        self.headerbar = Gtk.HeaderBar()
        Gtk.HeaderBar.set_has_subtitle(self.headerbar, True)
        Gtk.HeaderBar.set_title(self.headerbar, "TensorFlow Lite Edge TPU")
        Gtk.HeaderBar.set_show_close_button(self.headerbar, False)
        self.set_titlebar(self.headerbar)

        # add close button
        self.close_button = Gtk.Button.new_with_label("Close")
        self.close_button.connect("clicked", self.close)
        self.headerbar.pack_end(self.close_button)

        if args.image == "":
            self.enable_camera_preview = True
        else:
            self.enable_camera_preview = False

        GdkDisplay = Gdk.Display.get_default()
        monitor = Gdk.Display.get_monitor(GdkDisplay, 0)
        workarea = Gdk.Monitor.get_workarea(monitor)

        self.maximize()
        self.screen_width = workarea.width
        self.screen_height = workarea.height

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
        self.label.set_xalign(0)
        self.label.set_yalign(0)
        self.label.set_line_wrap(True)
        self.label.set_line_wrap_mode(Gtk.WrapMode.WORD)
        self.hbox.pack_start(self.label, False, False, 15)

        self.timeout_id = GLib.timeout_add(50, self.on_timeout)

        # initialize the list of the file to be processed (used with the
        # --image parameter)
        self.files = []

        # initialize the list of inference/display time to process the average
        # (used with the --validation parameter)
        self.valid_inference_time = []
        self.valid_inference_fps = []
        self.valid_draw_count = 0

    def close(self, button):
        self.destroy()

    def on_timeout(self):
        self.progressbar.pulse()
        return True

    # Updating the labels and the inference infos displayed on the GUI interface - camera input
    def update_label_preview(self, inference_time, inference_fps):
        str_inference_time = str("{0:0.1f}".format(inference_time))
        str_inference_fps = str("{0:.1f}".format(inference_fps))

        self.label.set_markup("<span font='10' color='#002052FF'><b>inference @%sfps\n\n\n</b></span>"
                              "<span font='15' color='#002052FF'><b>inference time: %sms\n</b></span>"
                              % (str_inference_fps, str_inference_time))

        if args.validation:
            # reload the timeout
            GLib.source_remove(self.valid_timeout_id)
            self.valid_timeout_id = GLib.timeout_add(5000,
                                                     self.valid_timeout_callback)

            self.valid_draw_count = self.valid_draw_count + 1
            # stop the application after 50 draws
            if self.valid_draw_count > 50:
                avg_inf_fps = sum(self.valid_inference_fps) / len(self.valid_inference_fps)
                avg_inf_time = sum(self.valid_inference_time) / len(self.valid_inference_time)
                print("avg inference fps= " + str(avg_inf_fps))
                print("avg inference time= " + str(avg_inf_time) + " ms")
                GLib.source_remove(self.valid_timeout_id)
                self.destroy()
                os._exit(0)

    def update_label_still(self, inference_time):
        str_inference_time = str("{0:0.1f}".format(inference_time))

        self.label.set_markup("<span font='15' color='#002052FF'><b>inference time: %sms\n</b></span>"
                              % (str_inference_time))

    def get_object_location_y0(self, idx):
        if idx < args.maximum_detection:
            return round(float(self.result_locations[0][idx][0]), 9)
        return 0

    def get_object_location_x0(self, idx):
        if idx < args.maximum_detection:
            return round(float(self.result_locations[0][idx][1]), 9)
        return 0

    def get_object_location_y1(self, idx):
        if idx < args.maximum_detection:
            return round(float(self.result_locations[0][idx][2]), 9)
        return 0

    def get_object_location_x1(self, idx):
        if idx < args.maximum_detection:
            return round(float(self.result_locations[0][idx][3]), 9)
        return 0

    def get_label(self, idx):
        labels = self.nn.get_labels()
        if idx < args.maximum_detection:
            return labels[int(self.result_classes[0][idx])]
        return 0


    def update_frame(self, frame, args):
        img = Image.fromarray(frame)
        draw = ImageDraw.Draw(img)

        # draw rectangle around the 5 first detected object with a score greater
        # than 60%
        if self.result_scores[0][0] > args.threshold:
            y0 = int(self.get_object_location_y0(0) * frame.shape[0])
            x0 = int(self.get_object_location_x0(0) * frame.shape[1])
            y1 = int(self.get_object_location_y1(0) * frame.shape[0])
            x1 = int(self.get_object_location_x1(0) * frame.shape[1])
            label = self.get_label(0)
            accuracy = self.result_scores[0][0] * 100
            draw.rectangle([(x0, y0), (x1, y1)], outline=(0,0,255))
            draw.rectangle([(x0, y0), (x0 + ((len(label) + 4) * char_text_width), y0 + 14)], (0,0,255))
            draw.text((x0 + 2, y0 + 2), label + " " + str(int(accuracy)) + "%", (255,255,255))

        if self.result_scores[0][1] > args.threshold:
            y0 = int(self.get_object_location_y0(1) * frame.shape[0])
            x0 = int(self.get_object_location_x0(1) * frame.shape[1])
            y1 = int(self.get_object_location_y1(1) * frame.shape[0])
            x1 = int(self.get_object_location_x1(1) * frame.shape[1])
            label = self.get_label(1)
            accuracy = self.result_scores[0][1] * 100
            draw.rectangle([(x0, y0), (x1, y1)], outline=(255,0,0))
            draw.rectangle([(x0, y0), (x0 + ((len(label) + 4) * char_text_width), y0 + 14)], (255,0,0))
            draw.text((x0 + 2, y0 + 2), label + " " + str(int(accuracy)) + "%", (255,255,255))

        if self.result_scores[0][2] > args.threshold:
            y0 = int(self.get_object_location_y0(2) * frame.shape[0])
            x0 = int(self.get_object_location_x0(2) * frame.shape[1])
            y1 = int(self.get_object_location_y1(2) * frame.shape[0])
            x1 = int(self.get_object_location_x1(2) * frame.shape[1])
            label = self.get_label(2)
            accuracy = self.result_scores[0][2] * 100
            draw.rectangle([(x0, y0), (x1, y1)], outline=(0,255,0))
            draw.rectangle([(x0, y0), (x0 + ((len(label) + 4) * char_text_width), y0 + 14)], (0,255,0))
            draw.text((x0 + 2, y0 + 2), label + " " + str(int(accuracy)) + "%", (255,255,255))

        if self.result_scores[0][3] > args.threshold:
            y0 = int(self.get_object_location_y0(3) * frame.shape[0])
            x0 = int(self.get_object_location_x0(3) * frame.shape[1])
            y1 = int(self.get_object_location_y1(3) * frame.shape[0])
            x1 = int(self.get_object_location_x1(3) * frame.shape[1])
            label = self.get_label(3)
            accuracy = self.result_scores[0][3] * 100
            draw.rectangle([(x0, y0), (x1, y1)], outline=(255,255,0))
            draw.rectangle([(x0, y0), (x0 + ((len(label) + 4) * char_text_width), y0 + 14)], (255,255,0))
            draw.text((x0 + 2, y0 + 2), label + " " + str(int(accuracy)) + "%", (255,255,255))

        if self.result_scores[0][4] > args.threshold:
            y0 = int(self.get_object_location_y0(4) * frame.shape[0])
            x0 = int(self.get_object_location_x0(4) * frame.shape[1])
            y1 = int(self.get_object_location_y1(4) * frame.shape[0])
            x1 = int(self.get_object_location_x1(4) * frame.shape[1])
            label = self.get_label(4)
            accuracy = self.result_scores[0][4] * 100
            draw.rectangle([(x0, y0), (x1, y1)], outline=(0,255,255))
            draw.rectangle([(x0, y0), (x0 + ((len(label) + 4) * char_text_width), y0 + 14)], (0,255,255))
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
        """
        Returns a random filename, chosen among the files of the given path.
        """
        if len(self.files) == 0:
            self.files = os.listdir(path)

        if len(self.files) == 0:
            return '';

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
        json_file = json_file + '.json'
        name = []
        x0 = []
        y0 = []
        x1 = []
        y1 = []
        with open(args.image + "/" + json_file) as json_file:
            data = json.load(json_file)
            for obj in data['objects_info']:
                name.append(obj['name'])
                x0.append(obj['x0'])
                y0.append(obj['y0'])
                x1.append(obj['x1'])
                y1.append(obj['y1'])

        return name, x0, y0, x1, y1

    # GTK camera preview function
    def inference_camera(self):
        """
        This method runs an inference on an frame captured from the FrameCapture class
        and processed for the model interpreter input dimensions
        """
        cap_img = self.video_stream.read()
        frame = cap_img[:, :, ::-1].copy()

        #Process the image for inference and display and update the preview frame
        loop_stop = time.perf_counter();
        processing_start = time.perf_counter()
        frame_crop = frame[self.y1:self.y2, self.x1:self.x2]
        frame_crop_RGB_resize = cv2.resize(frame_crop, (self.input_shape[1], self.input_shape[0]))
        img = Image.fromarray(frame_crop_RGB_resize)

        # Run the inference
        start = time.perf_counter()
        self.nn.launch_inference(img)
        self.inference_time = time.perf_counter() - start

        #Computing the inference FPS to get inference performances
        self.loop_time  = loop_stop - self.loop_start
        self.loop_start = loop_stop
        self.total_time = self.total_time + self.loop_time
        if self.loop_count==5:
            self.inference_fps = self.loop_count / self.total_time
            self.loop_count = 0
            self.total_time = 0
        self.loop_count = self.loop_count + 1

        # Display the inference results on the GUI interface
        self.result_locations[:, :, :], self.result_classes[:, :], self.result_scores[:, :] = self.nn.get_results()
        #print("Inference fps %.5f" % self.inference_fps)
        inference_time = self.inference_time * 1000
        inference_fps  = self.inference_fps

        if args.validation:
            self.valid_inference_fps.append(round(inference_fps))
            self.valid_inference_time.append(round(inference_time, 4))

        self.update_label_preview(inference_time, inference_fps)
        self.update_frame(frame_crop, args)

        return True

    # GTK still picture function
    def inference_still_picture(self, button):
        """
        This method runs an inference on an input picture after getting the signal
        of the pressed button "Next Inference"
        """
        return self.process_picture()

    def process_picture(self):
        # get randomly a picture in the directory
        rfile = self.getRandomFile(args.image)
        #print("Picture ", args.image + "/" + rfile)
        img = Image.open(args.image + "/" + rfile)

        # display the picture in the screen
        prev_frame = cv2.resize(np.array(img), (self.picture_width, self.picture_height))

        #Resize the frame to fit in the model interpreter
        nn_frame = cv2.resize(np.array(img), (self.input_shape[0], self.input_shape[1]))

         #Run the inference and compute its time
        start = time.perf_counter()
        self.nn.launch_inference(nn_frame)
        inference_time = time.perf_counter() - start

        #Display the results on the GUI interface
        self.result_locations[:, :, :], self.result_classes[:, :], self.result_scores[:, :] = self.nn.get_results()
        inference_time = inference_time * 1000
        self.update_label_still(inference_time)
        self.update_frame(prev_frame, args)

        if args.validation:
            #  get file path without extension
            file_name_no_ext = os.path.splitext(rfile)[0]

            print("\nInput file: " + args.image + "/" + rfile)

            # retreive associated JSON file information
            expected_label, expected_x0, expected_y0, expected_x1, expected_y1 = self.load_valid_results_from_json_file(file_name_no_ext)

            # count number of object above 60% and compare it with he expected
            # validation result
            count = 0
            for i in range(0, 5):
                if self.result_scores[0][i] > args.threshold:
                    count = count + 1

            expected_count = len(expected_label)
            print("\texpect %s objects. Object detection inference found %s objects"
                  % (expected_count, count))
            if count != expected_count:
                print("Inference result not aligned with the expected validation result\n")
                self.destroy()
                os._exit(1)

            for i in range(0, count):
                label = self.get_label(i)
                x0 = self.get_object_location_x0(i)
                y0 = self.get_object_location_y0(i)
                x1 = self.get_object_location_x1(i)
                y1 = self.get_object_location_y1(i)

                print("\t{0:12} (x0 y0 x1 y1) {1:12}{2:12}{3:12}{4:12}  expected result: {5:12} (x0 y0 x1 y1) {6:12}{7:12}{8:12}{9:12}".format(label, x0, y0, x1, y1, expected_label[i], expected_x0[i], expected_y0[i], expected_x1[i], expected_y1[i]))
                if expected_label[i] != label:
                    print("Inference result not aligned with the expected validation result\n")
                    self.destroy()
                    os._exit(1)

                error_epsilon = 0.02
                if abs(x0 - float(expected_x0[i])) > error_epsilon or \
                   abs(y0 - float(expected_y0[i])) > error_epsilon or \
                   abs(x1 - float(expected_x1[i])) > error_epsilon or \
                   abs(y1 - float(expected_y1[i])) > error_epsilon:
                    print("Inference result not aligned with the expected validation result\n")
                    self.destroy()
                    os._exit(1)

            # store the inference time in a list so that we can compute the
            # average later on
            self.valid_inference_time.append(round(inference_time, 4))

        return True

    def validation_picture(self):
        # reload the timeout
        GLib.source_remove(self.valid_timeout_id)
        self.valid_timeout_id = GLib.timeout_add(5000,
                                                 self.valid_timeout_callback)

        # validation mode activated
        self.process_picture()

        # process all the file
        if len(self.files) == 0:
            avg_inf_time = sum(self.valid_inference_time) / len(self.valid_inference_time)
            print("\navg inference time= " + str(avg_inf_time) + " ms")
            GLib.source_remove(self.valid_timeout_id)
            self.destroy()
            os._exit(0)

        return True

    def valid_timeout_callback(self):
        # if timeout occurs that means that camera preview and the gtk is not
        # behaving as expected */
        print("Timeout: camera preview and/or gtk is not behaving has expected\n");
        self.destroy()
        os._exit(1)

    def main(self, args):
        self.nn                     = NeuralNetwork(args.model_file, args.label_file, args.perf)
        self.input_shape            = self.nn.get_img_size()
        self.labels                 = self.nn.get_labels()
        self.frame_rate             = 150

       # start a timeout timer in validation process to close application if
        # timeout occurs
        if args.validation:
            self.valid_timeout_id = GLib.timeout_add(35000,
                                                     self.valid_timeout_callback)

        if self.nn._floating_model:
            print("The model is not quantized! Please quantized it for egde tpu usage...")
            Gtk.main_quit()

        Gtk.HeaderBar.set_subtitle(self.headerbar, "quant model " + os.path.basename(args.model_file))

        self.result_locations_shared_array = Array(ctypes.c_float, 1 * args.maximum_detection * 4)
        self.result_locations = np.ctypeslib.as_array(self.result_locations_shared_array.get_obj())
        self.result_locations = self.result_locations.reshape(1, args.maximum_detection, 4)

        self.result_classes_shared_array = Array(ctypes.c_float, 1 * args.maximum_detection)
        self.result_classes = np.ctypeslib.as_array(self.result_classes_shared_array.get_obj())
        self.result_classes = self.result_classes.reshape(1, args.maximum_detection)

        self.result_scores_shared_array = Array(ctypes.c_float, 1 * args.maximum_detection)
        self.result_scores = np.ctypeslib.as_array(self.result_scores_shared_array.get_obj())
        self.result_scores = self.result_scores.reshape(1, args.maximum_detection)

        if self.enable_camera_preview:
            print("Running Inferences on camera input")

            #variable to compute inference framerate
            self.loop_count         = 1
            self.loop_time          = 0
            self.loop_start         = 0
            self.total_time         = 0
            self.inference_fps      = 0
            self.inference_time     = 0

            # initialize VideoFrameCapture object
            self.video_stream = VideoFrameCapture(int(args.video_device), float(args.frame_width), float(args.frame_height), float(args.framerate))
            self.video_stream.start()

            #Get displayed frame dimensions
            self.y1 = int(0)
            self.y2 = int(self.input_shape[0])
            self.x1 = int((self.input_shape[1] - self.input_shape[0]) / 2)
            self.x2 = int(self.x1 + self.input_shape[0])

            GLib.source_remove(self.timeout_id)
            self.progressbar.hide()
            GLib.idle_add(self.inference_camera)

        else :
            # hidde the progress bar
            GLib.source_remove(self.timeout_id)
            self.progressbar.hide()

            print("Running Inferences on picture input")
            print("Warning: First inference might take a long time to be ran because of \n the time that takes the EdgeTPU to load the model to its RAM Memory ")

            if not args.validation:
                self.button = Gtk.Button.new_with_label("Next inference")
                self.vbox.pack_start(self.button, False, False, 15)
                self.button.connect("clicked", self.inference_still_picture)
                self.button.show_all()
            else:
                GLib.idle_add(self.validation_picture)

def destroy_window(gtkobject):
    gtkobject.terminate()
    print("destroy")


if __name__ == '__main__':
    # add signal to catch CRTL+C
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    # Check if the Edge TPU is connected
    edge_tpu = False
    device_re = re.compile(".+?ID\s(?P<id>\w+)", re.I)
    lsusb = subprocess.check_output("lsusb").decode("utf-8")
    for i in lsusb.split('\n'):
        if i:
            info = device_re.match(i)
            if info:
                d = info.groupdict()
                if '1a6e' in d.values() or '18d1' in d.values():
                    edge_tpu = True

    if not edge_tpu:
        print("Edge TPU is not plugged!")
        print("Please connect the Edge TPU and try again.")
        os._exit(1)

    #Tensorflow Lite NN intitalisation
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--image", default="", help="image directory with image to be classified")
    parser.add_argument("-v", "--video_device", default=0, help="video device (default /dev/video0)")
    parser.add_argument("--frame_width", default=320, help="width of the camera frame (default is 320)")
    parser.add_argument("--frame_height", default=240, help="height of the camera frame (default is 240)")
    parser.add_argument("--framerate", default=30, help="framerate of the camera (default is 15fps)")
    parser.add_argument("-m", "--model_file", default="detect_edgetpu.tflite", help=".tflite model to be executed")
    parser.add_argument("-l", "--label_file", default="labels.txt", help="name of file containing labels")
    parser.add_argument("-p", "--perf", default='high', choices= ['max', 'high', 'med', 'low'], help="Select the performance of the Coral EdgeTPU")
    parser.add_argument("--validation", action='store_true', help="enable the validation mode")
    parser.add_argument("--maximum_detection", default=10, type=int, help="Adjust the maximum number of object detected in a frame accordingly to your NN model (default is 10)")
    parser.add_argument("--threshold", default=0.60, type=float, help="threshold of accuracy above which the boxes are displayed (default 0.60)")
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
