#!/usr/bin/python3
#
# Copyright (c) 2025 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

import gi
gi.require_version('Gtk', '3.0')
from gi.repository import Gtk, Gdk

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
        label_text = ("ST On-device learning for object detection out-of-the box application is a headless demo.\n"
                      "This means that a computer connected to the same network is needed to launch the application.\n"
                      "A jupyter notebook can be opened by opening this board's IP address on a PC browser on 8888 port.\n"
                      "For more information on how to run this application, please refer to the dedicated wiki article :  \n "
                      "https://wiki.st.com/stm32mpu/wiki/On-device_learning_for_object_detection_on_Jupyter-lab")
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

        # Show all widgets
        self.show_all()

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
        Gtk.main_quit()

# Create and run the application
win = MaximizedWindow()
win.connect("destroy", Gtk.main_quit)
Gtk.main()
