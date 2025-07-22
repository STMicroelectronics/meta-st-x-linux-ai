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
from gi.repository import Gtk, Gdk

import subprocess
import os
import signal
import time
import sys

class MaximizedWindow(Gtk.Window):
    def __init__(self):
        super().__init__()

        # Set the window to maximized
        self.maximize()

        # Create a drawing area widget
        drawing_area = Gtk.DrawingArea()
        drawing_area.connect("draw", self.on_draw)

        # Create an overlay to put text on the drawing area
        overlay = Gtk.Overlay()
        overlay.add(drawing_area)

        # Create a label for the text
        label_text = (
            "ST people tracking and heatmap out-of-the box application is an headless demo.\n"
            "This signify that a computer is needed to launch the application.\n"
            "For more information please refer to the dedicated wiki article : https://wiki.st.com/stm32mpu/wiki/People_tracking_heatmap"
        )
        self.label = Gtk.Label(label=label_text)
        self.label.set_name("custom_label")
        self.label.set_line_wrap(True)
        self.label.set_line_wrap_mode(Gtk.WrapMode.WORD)

        # Position the label
        overlay.add_overlay(self.label)
        overlay.set_overlay_pass_through(self.label, False)
        self.label.set_halign(Gtk.Align.CENTER)
        self.label.set_valign(Gtk.Align.CENTER)

        # Add the overlay to the window
        self.add(overlay)

        # Apply CSS to style the label
        self.update_label_style()

        # Connect the size-allocate signal to dynamically adjust the font size
        self.connect("size-allocate", self.on_size_allocate)

        # Connect the button-press-event signal to close the window on touch
        self.connect("button-press-event", self.on_button_press)

        # Connect destroy to cleanup
        self.connect("destroy", self.on_destroy)

        # Start the subprocess
        self.process = self.start_people_tracking()

        # Show all widgets
        self.show_all()
    
    def start_people_tracking(self):
        cmd = [
            "python3",
            "/usr/local/x-linux-ai/people-tracking-heatmap/stai_mpu_people_tracking_heatmap.py",
            "-m", "/usr/local/x-linux-ai/people-tracking-heatmap/models/yolov8n_people/yolov8n_320_quant_pt_uf_od_coco-person-st.nb",
            "--frame_width", "1280",
            "--frame_height", "720"
        ]
        proc = subprocess.Popen(cmd, preexec_fn=os.setsid)
        return proc

    def on_draw(self, widget, cr):
        # Set the background color to blue (HEX: #03234B)
        cr.set_source_rgb(0.012, 0.137, 0.294)
        cr.paint()

    def on_size_allocate(self, widget, allocation):
        # Dynamically adjust the font size based on the window size
        width = allocation.width
        height = allocation.height
        font_size = min(width, height) // 30  # Adjust the divisor to control the font size
        self.update_label_style(font_size)

    def update_label_style(self, font_size=40):
        css = f"""
        #custom_label {{
            font-size: {font_size}px;
            color: white;
            font-weight: bold;
        }}
        """
        style_provider = Gtk.CssProvider()
        style_provider.load_from_data(css.encode('utf-8'))
        Gtk.StyleContext.add_provider_for_screen(
            Gdk.Screen.get_default(),
            style_provider,
            Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
        )

    def on_button_press(self, widget, event):
        # Close the window when it is touched (clicked)
        self.close()

    def on_destroy(self, widget):
        if hasattr(self, 'process') and self.process and self.process.poll() is None:
            pgid = os.getpgid(self.process.pid)
            try:
                os.killpg(pgid, signal.SIGTERM)
                for _ in range(10):
                    if self.process.poll() is not None:
                        break
                    time.sleep(0.5)
                else:
                    os.killpg(pgid, signal.SIGKILL)
            except Exception as e:
                print(f"Exception while killing process group: {e}")
        Gtk.main_quit()

# Create and run the application
win = MaximizedWindow()
Gtk.main()
sys.exit(0)
