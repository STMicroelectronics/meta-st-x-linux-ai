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
from PIL import Image
import argparse
import numpy as np
import cv2
import re
import subprocess
import signal
import os
import random
from threading import Thread
import tflite_runtime.interpreter as tflite

class VideoFrameCapture:
    """Camera class that controls video caping"""
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

        if perf == 'std':
            self._lib_edgetpu = "libedgetpu-std.so.2"
        elif perf == 'max':
            self._lib_edgetpu = "libedgetpu-max.so.2"

        self._interpreter = tflite.Interpreter(self._model_file,
                                               experimental_delegates=[tflite.load_delegate(self._lib_edgetpu)])
        self._interpreter.allocate_tensors()
        self._input_details  = self._interpreter.get_input_details()
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
        """
        This method can print and return the top_k results of the inference
        """
        output_data = self._interpreter.get_tensor(self._output_details[0]['index'])
        results = np.squeeze(output_data)

        top_k = results.argsort()[-5:][::-1]
        #for i in top_k:
        #    print('{0:08.6f}'.format(float(results[i]*100/255.0))+":", self._labels[i])
        #print("\n")

        return (results[top_k[0]]/255.0, top_k[0])

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
    def update_label_preview(self, label, accuracy, inference_time, inference_fps):
        str_accuracy = str("{0:.0f}".format(accuracy))
        str_inference_time = str("{0:0.1f}".format(inference_time))
        str_inference_fps = str("{0:.1f}".format(inference_fps))

        self.progressbar.show()
        self.progressbar.set_show_text(True)
        self.progressbar.set_fraction(accuracy / 100)
        self.label.set_markup("<span font='10' color='#002052FF'><b>inference @%sfps\n\n\n</b></span>"
                              "<span font='15' color='#002052FF'><b>inference time: %sms\n</b></span>"
                              "<span font='15' color='#002052FF'><b>accuracy:       %s&#37;\n\n</b></span>"
                              "<span font='15' color='#002052FF'><b>%s</b></span>"
                              % (str_inference_fps, str_inference_time, str_accuracy, label))

        if args.validation:
            # reload the timeout
            GLib.source_remove(self.valid_timeout_id)
            self.valid_timeout_id = GLib.timeout_add(5000,
                                                     self.valid_timeout_callback)

            self.valid_draw_count = self.valid_draw_count + 1
            # stop the application after 100 draws
            if self.valid_draw_count > 100:
                # remove the first inference statistic value which is not
                # representative due to the long starting process of the EdgeTPU
                self.valid_inference_fps.pop(0)
                self.valid_inference_time.pop(0)

                avg_inf_fps = sum(self.valid_inference_fps) / len(self.valid_inference_fps)
                avg_inf_time = sum(self.valid_inference_time) / len(self.valid_inference_time)
                print("avg inference fps= " + str(avg_inf_fps))
                print("avg inference time= " + str(avg_inf_time) + " ms")
                GLib.source_remove(self.valid_timeout_id)
                self.destroy()
                os._exit(0)

    # Updating the labels and the inference infos displayed on the GUI interface - picture input
    def update_label_still(self, label, accuracy, inference_time):
        str_accuracy = str("{0:.2f}".format(accuracy))
        str_inference_time = str("{0:0.1f}".format(inference_time))

        self.progressbar.show()
        self.progressbar.set_show_text(True)
        self.progressbar.set_fraction(accuracy / 100)

        self.label.set_markup("<span font='15' color='#002052FF'><b>inference time: %sms\n</b></span>"
                              "<span font='15' color='#002052FF'><b>accuracy:       %s&#37;\n\n</b></span>"
                              "<span font='15' color='#002052FF'><b>%s</b></span>"
                              % (str_inference_time, str_accuracy, label))

    # Updating the frame displayed on the GUI interface
    def update_frame(self, frame):
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

        index = random.randrange(0, len(self.files))
        file_path = self.files[index]
        self.files.pop(index)
        return file_path

    # GTK camera preview function
    def inference_camera(self):
        """
        This method runs an inference on an frame captured from the FrameCapture class
        and processed for the model interpreter input dimensions
        """
        cap_img = self.video_stream.read()
        frame = cap_img[:, :, ::-1].copy()

        # Processing the frame for the display
        frame_crop = frame[self.y1:self.y2, self.x1:self.x2]
        # Processing the frame to fit in the model interpreter input dimensions
        frame_crop_RGB_resize = cv2.resize(frame_crop, (self.input_shape[1], self.input_shape[0]))
        img = Image.fromarray(frame_crop_RGB_resize)
        self.update_frame(frame_crop_RGB_resize)

        # Run the inference
        loop_stop = time.perf_counter();
        start = time.perf_counter()
        self.nn.launch_inference(img)
        self.inference_time  = time.perf_counter() - start

        #Computing the inference FPS to get inference performances
        self.loop_time  = loop_stop - self.loop_start
        self.loop_start = loop_stop
        self.total_time = self.total_time + self.loop_time
        if self.loop_count==5:
            self.inference_fps = self.loop_count / self.total_time
            self.loop_count = 0
            self.total_time = 0
        self.loop_count = self.loop_count + 1

        # #Display the inference results on the GUI interface
        self.result_accuracy ,self.result_label = self.nn.get_results()
        #print("Inference fps %.5f" % self.inference_fps)
        label          = self.labels[self.result_label]
        accuracy       = self.result_accuracy * 100
        inference_time = self.inference_time * 1000
        inference_fps  = self.inference_fps

        if args.validation:
            self.valid_inference_fps.append(round(inference_fps))
            self.valid_inference_time.append(round(inference_time, 4))

        self.update_label_preview(str(label), accuracy, inference_time, inference_fps)

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
        #print("Picture: ", args.image + "/" + rfile)
        img = Image.open(args.image + "/" + rfile)

        #Resize the preview frame
        prev_frame = cv2.resize(np.array(img), (self.picture_width, self.picture_height))

        # update the preview frame
        self.update_frame(prev_frame)

        #Run the inference and compute its time
        nn_frame = cv2.resize(np.array(img), (self.input_shape[0], self.input_shape[1]))
        start = time.perf_counter()
        self.nn.launch_inference(nn_frame)
        inference_time = time.perf_counter() - start
        #print('Inference time : %.8fms' % (inference_time * 1000))

        #Display the results on the GUI interface
        self.result_accuracy ,self.result_label = self.nn.get_results()
        label          = self.labels[self.result_label]
        accuracy       = self.result_accuracy * 100
        inference_time = inference_time * 1000
        self.update_label_still(str(label), accuracy, inference_time)

        if args.validation:
            # get file name
            file_name = os.path.basename(rfile)
            # remove the extension
            file_name = os.path.splitext(file_name)[0]
            # remove eventual '_'
            file_name = file_name.rsplit('_')[0]
            # store the inference time in a list so that we can compute the
            # average later on
            self.valid_inference_time.append(round(inference_time, 4))
            print("name extract from the picture file: {0:32} label {1}".format(file_name, str(label)))
            if file_name != str(label):
                print("Inference result mismatch the file name")
                os._exit(1);

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
            # remove the first inference statistic value which is not
            # representative due to the long starting process of the EdgeTPU
            self.valid_inference_time.pop(0)

            avg_inf_time = sum(self.valid_inference_time) / len(self.valid_inference_time)
            print("avg inference time= " + str(avg_inf_time) + " ms")
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

        # start a timeout timer in validation process to close application if
        # timeout occurs
        if args.validation:
            self.valid_timeout_id = GLib.timeout_add(35000,
                                                     self.valid_timeout_callback)

        if self.nn._floating_model:
            print("The model is not quantized! Please quantized it for egde tpu usage...")
            Gtk.main_quit()

        Gtk.HeaderBar.set_subtitle(self.headerbar, "quant model " + os.path.basename(args.model_file))

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
        exit()

    #Tensorflow Lite NN intitalisation
    parser = argparse.ArgumentParser()
    parser.add_argument("-i", "--image", default="", help="image directory with image to be classified")
    parser.add_argument("-v", "--video_device", default=0, help="video device (default /dev/video0)")
    parser.add_argument("--frame_width", default=320, help="width of the camera frame (default is 320)")
    parser.add_argument("--frame_height", default=240, help="height of the camera frame (default is 240)")
    parser.add_argument("--framerate", default=30, help="framerate of the camera (default is 15fps)")
    parser.add_argument("-m", "--model_file", default="", help=".tflite model to be executed")
    parser.add_argument("-l", "--label_file", default="", help="name of file containing labels")
    parser.add_argument("-p", "--perf", default='std', choices= ['std', 'max'], help="Select the performance of the Coral EdgeTPU")
    parser.add_argument("--validation", action='store_true', help="enable the validation mode")
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
