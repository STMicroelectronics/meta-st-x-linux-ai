#!/usr/bin/python3
# to debug this script:
#      python3 -m pdb ./demo_launcher.py
#
import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk
from gi.repository import GObject
from gi.repository import Gdk
from gi.repository import GLib
from gi.repository import GdkPixbuf

#import evdev
import subprocess
#import random
#import math
import os
import fcntl
import struct
#from collections import deque
from time import sleep, time

#
# For simulating UI on PC , please use
# the variable SIMULATE = 1
# If SIMULATE = 1 then
#    the picture/icon must be present on pictures directory
#
SIMULATE = 0


if SIMULATE > 0:
    DEMO_PATH = os.environ['HOME']+"/Desktop/launcher"
else:
    DEMO_PATH = "/usr/local/demo-ai"

# -------------------------------------------------------------------
# -------------------------------------------------------------------
# CONSTANT VALUES
#
SIMULATE_SCREEN_SIZE_WIDTH  = 800
SIMULATE_SCREEN_SIZE_HEIGHT = 480

ICON_SIZE_BIG = 120
ICON_SIZE_SMALL = 60


# -------------------------------------------------------------------
# -------------------------------------------------------------------
def _load_image_eventBox(parent, filename, label_text1, label_text2, scale_w, scale_h):
    # Create box for xpm and label
    box = Gtk.VBox(False, 0)
    # Create an eventBox
    eventBox = Gtk.EventBox()
    # Now on to the image stuff
    pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale(
            filename=filename,
            width=scale_w,
            height=scale_h,
            preserve_aspect_ratio=True)
    image = Gtk.Image.new_from_pixbuf(pixbuf)

    label = Gtk.Label()
    label.set_markup("<span font='12' color='#39A9DCFF'>%s\n</span>"
                     "<span font='12' color='#002052FF'>%s</span>" % (label_text1, label_text2))
    label.set_justify(Gtk.Justification.CENTER)
    label.set_line_wrap(True)

    # Pack the pixmap and label into the box
    box.pack_start(image, True, False, 0)
    box.pack_start(label, True, False, 0)

    # Add the image to the eventBox
    eventBox.add(box)

    return eventBox


def _load_image_Box(parent, filename1, filename2, label_text, scale_w, scale_h):
    box = Gtk.VBox(False, 0)
    pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale(
            filename=filename1,
            width=scale_w,
            height=scale_h,
            preserve_aspect_ratio=True)
    image = Gtk.Image.new_from_pixbuf(pixbuf)

    # Create a label for the button
    label0 = Gtk.Label() #for padding
    label1 = Gtk.Label()
    label1.set_markup("<span font='14' color='#FFFFFFFF'><b>%s</b></span>\n"
                      "<span font='10' color='#FFFFFFFF'>Dual Arm&#174; Cortex&#174;-A7</span>\n"
                      "<span font='10' color='#FFFFFFFF'>+</span>\n"
                      "<span font='10' color='#FFFFFFFF'>Copro Arm&#174; Cortex&#174;-M4</span>\n" % label_text)
    label1.set_justify(Gtk.Justification.CENTER)
    label1.set_line_wrap(True)

    eventBox = Gtk.EventBox()
    pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale(
        filename=filename2,
        width=scale_w,
        height=(scale_h/2),
        preserve_aspect_ratio=True)
    info = Gtk.Image.new_from_pixbuf(pixbuf)
    eventBox.add(info)

    label3 = Gtk.Label()
    label3.set_markup("<span font='10' color='#FFFFFFFF'><b>Python GTK launcher</b></span>\n")
    label3.set_justify(Gtk.Justification.CENTER)
    label3.set_line_wrap(True)

    # Pack the pixmap and label into the box
    box.pack_start(label0, True, False, 0)
    box.pack_start(image, True, False, 0)
    box.pack_start(label1, True, False, 0)
    box.pack_start(eventBox, True, False, 0)
    box.pack_start(label3, True, False, 0)

    return box

def _load_image_on_button(parent, filename, label_text, scale_w, scale_h):
    # Create box for xpm and label
    box = Gtk.HBox(False, 0)
    box.set_border_width(2)
    # print("[DEBUG] image: %s " % filename)
    # Now on to the image stuff
    pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale(
            filename=filename,
            width=scale_w,
            height=scale_h,
            preserve_aspect_ratio=True)
    image = Gtk.Image.new_from_pixbuf(pixbuf)

    # Create a label for the button
    label = Gtk.Label(label_text)

    # Pack the pixmap and label into the box
    box.pack_start(image, True, False, 3)

    image.show()
    label.show()
    return box

def tfl_mobilenet_start():
    cmd = ["%s/python/launch_python_label_tfl_mobilenet.sh" % DEMO_PATH]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    result = proc.stdout.read().decode('utf-8')
    return result

# -------------------------------------------------------------------
# -------------------------------------------------------------------
class MainUIWindow(Gtk.Window):
    def __init__(self):
        Gtk.Window.__init__(self, title="Demo Launcher")
        self.set_decorated(False)
        self.modify_bg(Gtk.StateType.NORMAL, Gdk.Color(65535, 65535, 65535))
        if SIMULATE > 0:
            self.screen_width = SIMULATE_SCREEN_SIZE_WIDTH
            self.screen_height = SIMULATE_SCREEN_SIZE_HEIGHT
        else:
            #self.fullscreen()
            self.maximize()
            self.screen_width = self.get_screen().get_width()
            self.screen_height = self.get_screen().get_height()

        if self.screen_width == 720:
            self.icon_size = ICON_SIZE_BIG
            self.board_name = "Evaluation board"
        else:
            self.icon_size = ICON_SIZE_SMALL
            self.board_name = "Discovery kit"

        self.set_default_size(self.screen_width, self.screen_height)
        print("[DEBUG] screen size: %dx%d" % (self.screen_width, self.screen_height))
        self.set_position(Gtk.WindowPosition.CENTER)
        self.connect('destroy', Gtk.main_quit)

        self.previous_click_time=0

        # page for basic information
        self.create_page_icon()

    def display_message(self, message):
        dialog = Gtk.Dialog("Error", self, 0, (Gtk.STOCK_OK, Gtk.ResponseType.OK))
        dialog.set_decorated(False)
        width, height = self.get_size()
        dialog.set_default_size(width, height)
        rgba = Gdk.RGBA(0.31, 0.32, 0.31, 0.8)
        dialog.override_background_color(0,rgba)

        label0 = Gtk.Label() #for padding

        label1 = Gtk.Label()
        label1.set_markup(message)
        label1.set_justify(Gtk.Justification.CENTER)
        label1.set_line_wrap(True)

        label2 = Gtk.Label() #for padding

        # Create a centering alignment object
        align = Gtk.Alignment()
        align.set(0.5, 0, 0, 0)

        dialog.vbox.pack_start(label0, True, False, 0)
        dialog.vbox.pack_start(label1, True, True,  0)
        dialog.vbox.pack_start(align,  True, True,  0)
        dialog.vbox.pack_start(label2, True, False, 0)

        dialog.action_area.reparent(align)
        dialog.show_all()

        dialog.run()
        print("INFO dialog closed")

        dialog.destroy()

    # Button event of main screen
    def highlight_eventBox(self, widget, event):
        ''' highlight the eventBox widget '''
        print("[highlight_eventBox start]")
        rgba = Gdk.RGBA(0.0, 0.0, 0.0, 0.1)
        widget.override_background_color(0,rgba)
        self.button_exit.hide()
        print("[highlight_eventBox stop]\n")

    def tfl_mobilenet_event(self, widget, event):
        print("[tfl_mobilenet_event start]")
        if os.path.exists("/dev/video0"):
            tfl_mobilenet_start()
        else:
            print("[WARNING] camera not detected\n")
            self.display_message("<span font='15' color='#FFFFFFFF'>Webcam is not connected:\n/dev/video0 doesn't exist\n</span>")
        print("[tfl_mobilenet_event stop]\n")
        rgba = Gdk.RGBA(0.0, 0.0, 0.0, 0.0)
        widget.override_background_color(0,rgba)
        self.button_exit.show()

    def create_page_icon(self):
        page_main = Gtk.HBox(False, 0)
        page_main.set_border_width(0)

        # create a grid of icon
        icon_grid = Gtk.Grid(column_homogeneous=True, row_homogeneous=True)
        icon_grid.set_column_spacing(20)
        icon_grid.set_row_spacing(20)

        # STM32MP1 Logo and info area
        logo_info_area = _load_image_Box(self, "%s/media/ST11249_Module_STM32MP1_alpha.png" % DEMO_PATH, "%s/media/ST7079_AI_neural_white.png" % DEMO_PATH, self.board_name, -1, 160)
        rgba = Gdk.RGBA(0.31, 0.32, 0.31, 1.0)
        logo_info_area.override_background_color(0,rgba)

        # Button: tfl_mobilenet icon
        eventBox_tfl_mobilenet = _load_image_eventBox(self, "%s/media/TensorFlowLogo.png" % DEMO_PATH, "TensorFlow Lite", "Mobilenet v1 (quant)", -1, self.icon_size)
        eventBox_tfl_mobilenet.connect("button_release_event", self.tfl_mobilenet_event)
        eventBox_tfl_mobilenet.connect("button_press_event", self.highlight_eventBox)

        empty_box1 = Gtk.VBox(False, 0)
        empty_box2 = Gtk.VBox(False, 0)

        icon_grid.attach(logo_info_area, 3, 0, 1, 3)

        icon_grid.attach(empty_box1, 0, 1, 1, 1)
        icon_grid.attach(eventBox_tfl_mobilenet, 1, 1, 1, 1)
        icon_grid.attach(empty_box2, 2, 1, 1, 1)

        page_main.add(icon_grid)

        overlay = Gtk.Overlay()
        overlay.add(page_main)
        self.button_exit = Gtk.Button()
        self.button_exit.connect("clicked", Gtk.main_quit)
        self.button_exit_image = _load_image_on_button(self, "%s/media/close_70x70_white.png" % DEMO_PATH, "Exit", -1, 50)
        self.button_exit.set_halign(Gtk.Align.END)
        self.button_exit.set_valign(Gtk.Align.START)
        self.button_exit.add(self.button_exit_image)
        self.button_exit.set_relief(Gtk.ReliefStyle.NONE)
        overlay.add_overlay(self.button_exit)
        self.add(overlay)

        self.show_all()


lock_handle = None
lock_file_path = '/var/lock/ai_demo_launcher.lock'

def file_is_locked(file_path):
    global lock_handle
    lock_handle= open(file_path, 'w')
    try:
        fcntl.lockf(lock_handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
        return False
    except IOError:
        return True

# -------------------------------------------------------------------
# -------------------------------------------------------------------
# Main
if __name__ == "__main__":
    # add signal to catch CRTL+C
    import signal
    signal.signal(signal.SIGINT, signal.SIG_DFL)

    if file_is_locked(lock_file_path):
        print("[ERROR] another instance is running exiting now\n")
        exit(0)

    try:
        win = MainUIWindow()
        win.connect("delete-event", Gtk.main_quit)
        win.show_all()
    except Exception as exc:
        print("Main Exception: ", exc )

    Gtk.main()
