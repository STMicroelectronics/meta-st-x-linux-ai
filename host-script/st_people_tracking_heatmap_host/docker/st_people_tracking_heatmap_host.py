#!/usr/bin/python3
#
# Copyright (c) 2025 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

import cv2
from PIL import Image
import numpy as np
import os
import socket
import signal
import time
import multiprocessing as mp
import customtkinter as ctk
import supervision

MAX_NUMBER_CLIENT = 10
SIZE_BUFFER = 8192
MAX_WIDTH = 1920
MAX_HEIGHT = 1080

width = 1280
height = 720
obj_number = 0
track_length = 30

def decode_process(queue_message, serversocket):
    while True:
        try:
            clientsocket, address = serversocket.accept()
            print(f"Connection from {address} has been established.")
            while True:
                data = clientsocket.recv(SIZE_BUFFER).decode('utf-8')
                message = "0"
                while '/B' in data and '/E' in data:
                    start = data.find('/B') + 2
                    end = data.find('/E')
                    if start < end:
                        message = data[start:end].strip()
                        data = data[end + 2:]
                    else:
                        break
                queue_message.put(message)
            clientsocket.close()
        except:
            # No incoming connection, continue with other tasks
            print("No incoming connection, doing other tasks...")
            time.sleep(2)  # Sleep for a while to avoid busy-waiting

def camera_process(queue_frame, frame_width, frame_height):
    gst_pipeline = (
        "udpsrc port=4999 ! "
        "application/x-rtp, encoding-name=H264, payload=96 ! "
        "rtph264depay ! "
        "avdec_h264 ! "
        "videoconvert ! "
        "appsink sync=false, max-buffer=1, drop=true"
    )
    cap = cv2.VideoCapture(gst_pipeline, cv2.CAP_GSTREAMER)

    if not cap.isOpened():
        print("Error: Unable to open the video stream.")
        exit()

    while True:
        ret, frame = cap.read()
        if ret:
            img = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            if frame_width.value != 0 and frame_height.value != 0:
                img = cv2.resize(img, (frame_width.value, frame_height.value))
            queue_frame.put(img)
        else:
            break
    cap.release()

def show_process(queue_frame, queue_message, frame_width, frame_height, serversocket, board_ip):
    def update_frame():
        global first_heat, heatmap_1h, heatmap_10m, heatmap_all

        # Max values to reach for heatmap to be red
        max_value_10m = 15000 # 25 fps * 60 sec * 10 min
        max_value_1h = max_value_10m * 6
        base_value_10m = 80 / 100 * max_value_10m
        base_value_1h = 80 / 100 * max_value_1h


        # Rescale automatically the UI and take into account the choosen preview resolution
        scaled_width = width_slide if width_slide <= video_frame.winfo_width() else video_frame.winfo_width()
        scaled_height = height_slide if height_slide <= video_frame.winfo_height() else video_frame.winfo_height()
        frame_width.value = scaled_width if scaled_width < MAX_WIDTH else MAX_WIDTH
        frame_height.value = scaled_height if scaled_height < MAX_HEIGHT else MAX_HEIGHT
        width, height = frame_width.value, frame_height.value

        # Recover frame from other process
        if not queue_frame.empty():
            frame = queue_frame.get()
            img = frame.copy()
            if img is None:
                print("Error: Unable to read from the video stream.")
                return

            # Recover message from other process
            message = queue_message.get()
            splitdata = message.split("#")

            # Reformat value to match supervison detections class
            obj_number = int(splitdata[0])
            detection = []
            trackerid = []
            for i in range(obj_number):
                trackid = int(splitdata[(i*65)+1])
                trackerid.append(trackid)
                bb_data = [
                    int(float(splitdata[(i*65)+2]) * width),
                    int(float(splitdata[(i*65)+3]) * height),
                    int(float(splitdata[(i*65)+4]) * width),
                    int(float(splitdata[(i*65)+5]) * height)
                ]
                detection.append(bb_data)

            # Supervision Annotators
            if detection != []:
                detections = supervision.Detections(xyxy=np.array(detection),tracker_id=np.array(trackerid), class_id=np.array([0 for val in detection]))

                if print_id == 1: # Box Annotator
                    img = box_annotator.annotate(
                            scene=img.copy(),
                            detections=detections,
                    )
                elif print_id == 2: # BoxCorner Annotator
                    img = corner_annotator.annotate(
                            scene=img.copy(),
                            detections=detections
                    )
                elif print_id == 3: # Color Annotator
                    img = color_annotator.annotate(
                            scene=img.copy(),
                            detections=detections,
                    )
                elif print_id == 4: # Triangle Annotator
                    img = triangle_annotator.annotate(
                            scene=img.copy(),
                            detections=detections
                    )
                elif print_id == 5: # Ellipse Annotator
                    img = ellipse_annotator.annotate(
                            scene=img.copy(),
                            detections=detections
                    )
                elif print_id == 6: #Blur Annotator
                    img = blur_annotator.annotate(
                            scene=img.copy(),
                            detections=detections
                    )
                elif print_id == 7: # Heatmap Annotator
                    """do nothing"""

                if track:
                    for i in range(obj_number):
                        points = []
                        id_loc_x = int(detections.xyxy[i][0]) if print_id != 4 else int(detections.xyxy[i][0]-15 + ((detections.xyxy[i][2] - detections.xyxy[i][0]) / 2))
                        id_loc_y = int(detections.xyxy[i][1]-6) if print_id != 4 else int(detections.xyxy[i][1]-15)
                        cv2.putText(img, "ID: "+str(trackerid[i]), (id_loc_x,id_loc_y), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (230,0,126), 1, cv2.LINE_AA)

                        if trace:
                            for j in range(0, track_length * 2, 2):
                                point = [int(float(splitdata[(i*65)+j+6]) * width), int(float(splitdata[(i*65)+j+7]) * height)]
                                if point != [0, 0]:
                                    points.append(point)
                            points_arr = np.array(points, np.int32).reshape((-1, 1, 2))
                            cv2.polylines(img, [points_arr], isClosed=False, color=(230,0,126), thickness=2)

                # Heatmap Generation
                for i in range(obj_number):
                    # Point to draw circle
                    x1 = int(float(splitdata[(i*65)+2]) * heatmap_1h.shape[1])
                    x2 = int(float(splitdata[(i*65)+4]) * heatmap_1h.shape[1])
                    y2 = int(float(splitdata[(i*65)+5]) * heatmap_1h.shape[0])
                    id_loc_x_heat = int(x1 + ((x2 - x1) / 2))
                    id_loc_y_heat = int(y2)

                    # Heatmap since the demo is launched
                    heatmap_all += cv2.circle(np.zeros((heatmap_all.shape[0], heatmap_all.shape[1]), dtype=np.float64),
                                                   center=(id_loc_x_heat, id_loc_y_heat),
                                                   radius=20,
                                                   color=(3,),
                                                   thickness=-1)

                    # Heatmap for last hour
                    heatmap_1h += cv2.circle(np.zeros((heatmap_1h.shape[0], heatmap_1h.shape[1]), dtype=np.float64),
                                                   center=(id_loc_x_heat, id_loc_y_heat),
                                                   radius=25,
                                                   color=(34,),
                                                   thickness=-1)

                    # Heatmap for Live
                    heatmap_10m += cv2.circle(np.zeros((heatmap_10m.shape[0], heatmap_10m.shape[1]), dtype=np.float64),
                                                   center=(id_loc_x_heat, id_loc_y_heat),
                                                   radius=25,
                                                   color=(28,),
                                                   thickness=-1)
            heatmap_1h -= 2
            heatmap_1h = np.clip(heatmap_1h, 0, max_value_1h)
            heatmap_10m -= 4
            heatmap_10m = np.clip(heatmap_10m, 0, max_value_10m)

            if heatmap_enable:
                hsv = np.zeros((heatmap_10m.shape[0], heatmap_10m.shape[1],3), dtype=np.uint8)
                hsv[..., 1] = 255
                hsv[..., 2] = 255

                if choose_heat == 0:
                    heatmap_10m_blur = cv2.blur(heatmap_10m.astype(np.float64), (15, 15))
                    hsv[..., 0] = (240 - (heatmap_10m_blur / base_value_10m * 240)) / 2
                    mask = heatmap_10m_blur

                if choose_heat == 1:
                    heatmap_1h_blur = cv2.blur(heatmap_1h.astype(np.float64), (15, 15))
                    hsv[..., 0] = (240 - (heatmap_1h_blur / base_value_1h * 240)) / 2
                    mask = heatmap_1h_blur

                if choose_heat == 2:
                    heatmap_all_blur = cv2.blur(heatmap_all.astype(np.float64), (15, 15))
                    max_value_all = slider_value / 100 * np.max(heatmap_all_blur)
                    hsv[..., 0] = (240 - (heatmap_all_blur / max_value_all * 240)) / 2
                    mask = heatmap_all_blur

                hsv[..., 0] = np.where(hsv[..., 0] > 120, 0, hsv[..., 0])
                heatmap_printed = cv2.cvtColor(hsv.astype(np.uint8), cv2.COLOR_HSV2RGB)
                heatmap_printed = cv2.resize(heatmap_printed, (img.shape[1], img.shape[0]), interpolation=cv2.INTER_LINEAR)

                mask = cv2.resize(mask, (img.shape[1], img.shape[0]), interpolation=cv2.INTER_NEAREST)
                mask = mask > 2

                img[mask] = cv2.addWeighted(heatmap_printed, 0.5, img, 0.5, 0)[mask]

            img_final = Image.fromarray(img)
            imgtk = ctk.CTkImage(img_final, size=(width, height))
            video_label.imgtk = imgtk
            video_label.configure(image=imgtk)

        video_label.after(10, update_frame)

    def button_click():
        global track
        track = not track
        print(f"Tracking activation: {track}")

    def button_click_trace():
        global trace
        trace = not trace
        print(f"Trace activation: {trace}")

    def button_heatmap_click():
        global heatmap_enable
        heatmap_enable = not heatmap_enable
        print(f"Heatmap enable: {heatmap_enable}")

    def print_textbox_content():
        global print_text
        print_text = not print_text
        if print_text:
            textbox.grid(row=4, column=0, padx=(10, 10), pady=(10, 10), sticky="nsew")
        else:
            textbox.grid_remove()

    def show_error_message(message):
        error_window = ctk.CTkToplevel(root)
        error_window.title("Error")
        error_window.geometry("600x300")

        label = ctk.CTkLabel(error_window, text=message, wraplength=250)
        label.pack(pady=20)

        button = ctk.CTkButton(error_window, text="OK", command=error_window.destroy)
        button.pack(pady=10)

    def start_button_click():
        if not close:
            host_entry_text = host_entry.get()
            board_entry_text = board_entry.get()
            with board_ip.get_lock():
                board_ip.value = board_entry_text.encode("utf-8")
            ssh_cmd = f"ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null root@{board_entry_text} 'python3 /usr/local/x-linux-ai/people-tracking-heatmap/stai_mpu_people_tracking_heatmap.py --host_ip {host_entry_text} -m /usr/local/x-linux-ai/people-tracking-heatmap/models/yolov8n_people/yolov8n_320_quant_pt_uf_od_coco-person-st.nb ' &"
            try:
                if host_entry_text == "" or board_entry_text == "":
                    show_error_message(f"Please set a correct IP address")
                else:
                    # Set server IP/Port
                    serversocket.bind((host_entry_text, 1237))
                    serversocket.setblocking(False)
                    serversocket.listen(MAX_NUMBER_CLIENT)

                    # Run command on the board
                    os.system(ssh_cmd)

                    # Disable buttons
                    host_entry.delete(0, ctk.END)
                    board_entry.delete(0, ctk.END)
                    host_entry.configure(state="disabled")
                    board_entry.configure(state="disabled")
                    start_button.configure(state="disabled")

                    # Change the tab view
                    tabview.set("Controls")

            except Exception as e:
                show_error_message(f"An error occurred: {e}. Please set a correct IP address")
        else:
            if board_ip.value != b'123456789101112':
                print("killing ssh process")
                os.system("kill $(ps aux | grep -E 'ssh root@[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+ python3 /usr/local/x-linux-ai/people-tracking-heatmap/stai_mpu_people_tracking_heatmap.py --host_ip' | awk '{print $2}')")
                os.system("ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null root@"+str(board_ip.value.decode('utf-8'))+" pkill -f '/usr/local/x-linux-ai/people-tracking-heatmap/stai_mpu_people_tracking_heatmap.py'")

    def on_segmented_button_click(value):
        global choose_heat
        if value == "Live":
            choose_heat = 0
        if value == "1 hour":
            choose_heat = 1
        if value == "Infinite":
            choose_heat = 2
        print(f"Choose heat value: {choose_heat}")

    def change_scaling_event(new_scaling: str):
        global width_slide, height_slide
        if "1920x1080" in new_scaling:
            width_slide, height_slide = 1920, 1080
        if "1280x720" in new_scaling:
            width_slide, height_slide = 1280, 720
        if "850x480" in new_scaling:
            width_slide, height_slide = 850, 480
        if "640x480" in new_scaling:
            width_slide, height_slide = 640, 480
        if "320x240" in new_scaling:
            width_slide, height_slide = 320, 240
        print(f"Resolution : {new_scaling}")

    # Function to handle radio button clicks
    def button_choose_print(value):
        global print_id
        print_id = value
        print(f"Button {print_id} clicked")

    def change_appearance_mode_event(new_appearance_mode: str):
        ctk.set_appearance_mode(new_appearance_mode)

    def on_slider_change(value):
        global slider_value
        slider_value = value
        print(f"Slider value: ", slider_value)

    # Add a button to exit fullscreen mode
    def exit_fullscreen(event=None):
        root.attributes("-fullscreen", False)

    # Function to enter fullscreen mode
    def enter_fullscreen(event=None):
        root.attributes("-fullscreen", True)

    # Initialize variables
    global width_slide, height_slide, print_id, video_label, track, first_heat, print_text, trace, slider_value
    global heatmap_1h, heatmap_10m, heatmap_all, heatmap_enable, choose_heat
    first_heat, heatmap_enable = True, False
    track, trace, print_text = True, True, True
    slider_value = 100
    choose_heat = 0
    heatmap_1h = np.full((height, width), 0, dtype=np.float64)
    heatmap_10m = np.full((height, width), 0, dtype=np.float64)
    heatmap_all = np.full((height, width), 0, dtype=np.float64)

    # Initialize the main window
    root = ctk.CTk()
    root.title("OpenCV Video Stream w/ Tkinter")

    # Initialize supervision annotators
    color = supervision.ColorPalette(colors=[supervision.Color(r=126, g=0, b=230)])
    box_annotator = supervision.BoxAnnotator(color=color)
    corner_annotator = supervision.BoxCornerAnnotator(color=color)
    color_annotator = supervision.ColorAnnotator(color=color)
    triangle_annotator = supervision.TriangleAnnotator(color=color)
    ellipse_annotator = supervision.EllipseAnnotator(color=color)
    blur_annotator = supervision.BlurAnnotator()

    # Create control frame
    control_frame = ctk.CTkFrame(root, corner_radius=0, fg_color="#03234B")
    control_frame.place(relwidth=0.2, relheight=1, relx=0, rely=0)
    control_frame.grid_columnconfigure(0,  weight=1)
    for i in range(20):
        control_frame.grid_rowconfigure(i,  weight=1)

    # Load the background image
    xliai_image = Image.open("./xlinuxai.png")
    xliai_image = xliai_image.resize((250, 90))
    background_photo_xliai = ctk.CTkImage(xliai_image, size=(250,90))

    # Add label to video frame with background image
    photo_xliai = ctk.CTkLabel(control_frame, text="", image=background_photo_xliai)
    photo_xliai.grid(row=0, column=0, padx=0, pady=10, sticky="n")

    # Add label to video frame with background image
    label_ppl_tracking = ctk.CTkLabel(control_frame, text="People Tracking\n&\nHeatmap", text_color="#FFD200", font=ctk.CTkFont(family="Arial", size=22, weight="bold"))
    label_ppl_tracking.grid(row=2, column=0, padx=0, pady=0, sticky="nwse")

    # Create textbox
    textbox = ctk.CTkTextbox(control_frame, width=250,  wrap=ctk.WORD, fg_color="#425a78")
    textbox.grid(row=4, column=0, padx=(10, 10), pady=(10, 10), sticky="nsew")
    text_to_insert = ("Application purpose:\n"
    "This application shows the advanced edge AI and connectivity capabilities of the STM32MP2 MPU.\n\n"
    "Hardware:\n"
    "STM32MP2 microprocessor series with up to dual Arm Cortex®-A35 and Cortex®-M33 cores and a GPU/NPU accelerator up to 1.35 TOPS\n\n"
    "App in detail:\n"
    "   - Scene capture with a MIPI CSI2 camera.\n"
    "   - Images processed by the internal ISP\n"
    "   - Inference at the edge on NPU\n"
    "   - YoloV8n trained for person detection + Tracking (ByteTrack)\n"
    "   - H.264 video encoding including AI results to the PC\n"
    "   - The PC used for display only")
    textbox.insert("0.0",  text_to_insert)
    textbox.configure(state=ctk.DISABLED)

    # Create tabview
    tabview = ctk.CTkTabview(control_frame, width=250, fg_color="#425a78", segmented_button_fg_color="#566e8c",
                             segmented_button_selected_hover_color="#566e8c", segmented_button_unselected_color="#566e8c")
    tabview.grid(row=7, column=0, padx=(10, 10), pady=(10, 10), sticky="nsew")

    # Connection tab
    tabview.add("Connection")
    tabview.tab("Connection").grid_columnconfigure(0, weight=1)
    tabview.tab("Connection").grid_rowconfigure(0, weight=1)
    tabview.tab("Connection").grid_rowconfigure(1, weight=1)
    tabview.tab("Connection").grid_rowconfigure(2, weight=1)
    tabview.tab("Connection").grid_rowconfigure(3, weight=1)
    tabview.tab("Connection").grid_rowconfigure(4, weight=1)

    # Customization tab
    tabview.add("Customization")
    tabview.tab("Customization").grid_columnconfigure(0, weight=1)
    tabview.tab("Customization").grid_rowconfigure(0, weight=1)
    tabview.tab("Customization").grid_rowconfigure(1, weight=1)
    tabview.tab("Customization").grid_rowconfigure(2, weight=1)
    tabview.tab("Customization").grid_rowconfigure(3, weight=1)

    # Customization tab
    tabview.add("Controls")
    tabview.tab("Controls").grid_columnconfigure(0, weight=1)
    tabview.tab("Controls").grid_rowconfigure(0, weight=1)
    tabview.tab("Controls").grid_rowconfigure(1, weight=1)
    tabview.tab("Controls").grid_rowconfigure(2, weight=1)
    tabview.tab("Controls").grid_rowconfigure(3, weight=1)
    tabview.tab("Controls").grid_rowconfigure(4, weight=1)
    tabview.tab("Controls").grid_rowconfigure(5, weight=1)
    tabview.tab("Controls").grid_rowconfigure(6, weight=1)
    tabview.tab("Controls").grid_rowconfigure(7, weight=1)


    # Connection tab configuration
    # Add entry to choose host ip
    host_ip_label = ctk.CTkLabel(tabview.tab("Connection"), text="Host IP:", anchor="w")
    host_ip_label.grid(row=0, column=0, sticky="s")
    host_entry = ctk.CTkEntry(tabview.tab("Connection"), placeholder_text="Host PC IP", fg_color="#2e4664", border_color="#38506e")
    host_entry.grid(row=1, column=0, sticky="nsew")

    # Add entry to choose board ip
    board_ip_label = ctk.CTkLabel(tabview.tab("Connection"), text="Board IP:", anchor="w")
    board_ip_label.grid(row=2, column=0, sticky="s")
    board_entry = ctk.CTkEntry(tabview.tab("Connection"), placeholder_text="Board IP", fg_color="#2e4664", border_color="#38506e")
    board_entry.grid(row=3, column=0, sticky="nsew")
    # Add button to start the demo
    global close
    close = False
    start_button = ctk.CTkButton(tabview.tab("Connection"), text="Start", command=start_button_click, fg_color="#566e8c")
    start_button.grid(row=4, column=0, pady=10, sticky="nwse")

    # Cutomization tab configuration
    # Add menu to select appearance mode
    ctk.set_appearance_mode("dark")
    theme_optionemenu = ctk.CTkOptionMenu(tabview.tab("Customization"), width=250, values=["Dark", "Light", "System"],
                                          command=change_appearance_mode_event, anchor="center", fg_color="#2e4664")
    theme_optionemenu.grid(row=1, column=0)
    theme_optionemenu_label = ctk.CTkLabel(tabview.tab("Customization"), text="Selected Theme:", anchor="w")
    theme_optionemenu_label.grid(row=0, column=0, sticky="sw")

    # Add menu to select preview resolution
    scaling_optionemenu = ctk.CTkOptionMenu(tabview.tab("Customization"), width=250, values=["1920x1080", "1280x720", "850x480", "640x480", "320x240"],
                                            command=change_scaling_event, anchor="center", fg_color="#2e4664")
    scaling_optionemenu.grid(row=3, column=0)
    scaling_optionemenu.set("1280x720")
    width_slide, height_slide = 1280, 720
    scaling_optionemenu_label = ctk.CTkLabel(tabview.tab("Customization"), text="Video Preview Scaling:", anchor="w")
    scaling_optionemenu_label.grid(row=2, column=0, sticky="sw")

    # Print text box
    text_button_label = ctk.CTkLabel(tabview.tab("Customization"), text="Disable text:", anchor="w")
    text_button_label.grid(row=4, column=0, sticky="w")
    button_t = ctk.CTkSwitch(tabview.tab("Customization"), text="Print/Remove text", command=print_textbox_content, fg_color="#2e4664")
    button_t.grid(row=5, column=0, padx=10, pady=10, sticky="nsew")

    # Controls tab configuration
    # Label control tracking button
    control_track_label = ctk.CTkLabel(tabview.tab("Controls"), text="Control tracking:", anchor="w")
    control_track_label.grid(row=0, column=0, padx=(10, 10), pady=(10, 10), sticky="w")

    # Label control heatmap buttons
    control_track_label = ctk.CTkLabel(tabview.tab("Controls"), text="Control heatmap:", anchor="w")
    control_track_label.grid(row=3, column=0, padx=(10, 10), pady=(10, 10), sticky="w")
    # Label control heatmap buttons
    control_track_label = ctk.CTkLabel(tabview.tab("Controls"), text="Infinite heatmap scaling:", anchor="w")
    control_track_label.grid(row=6, column=0, padx=(10, 10), pady=(10, 10), sticky="w")

    # Add button to control frame
    # Track untrack
    button = ctk.CTkSwitch(tabview.tab("Controls"), text="Untrack/Track", command=button_click, fg_color="#2e4664")
    button.grid(row=1, column=0, padx=10, pady=10, sticky="nwse")
    button.select()
    # Trace untrace
    button = ctk.CTkSwitch(tabview.tab("Controls"), text="Enable trace", command=button_click_trace, fg_color="#2e4664")
    button.grid(row=2, column=0, padx=10, pady=10, sticky="nwse")
    button.select()
    # Enable heatmap
    button_h = ctk.CTkSwitch(tabview.tab("Controls"), text="Enable heatmap", command=button_heatmap_click, fg_color="#2e4664")
    button_h.grid(row=4, column=0, padx=10, pady=10, sticky="nwse")
    # Slider for inifinite heatmap
    slider_1 = ctk.CTkSlider(tabview.tab("Controls"), from_=50, to=100, number_of_steps=10, command=on_slider_change)
    slider_1.grid(row=7, column=0, padx=10, pady=10, sticky="ew")
    slider_1.set(100)


    # Segmented button to choose heatmap mode
    seg_button = ctk.CTkSegmentedButton(tabview.tab("Controls"), values=["Live", "1 hour", "Infinite"],
                                        fg_color="#2e4664", unselected_color="#566e8c", command=on_segmented_button_click)
    seg_button.grid(row=5, column=0, padx=10, pady=10, sticky="nwse")
    seg_button.set("Live")

    # Create video frame
    video_frame = ctk.CTkFrame(root, corner_radius=10, fg_color="#03234B")
    video_frame.place(relwidth=0.80, relheight=1, relx=0.2, rely=0)

    # Add label to video frame
    video_label = ctk.CTkLabel(video_frame, text="", fg_color="#03234B")
    video_label.place(relwidth=1, relheight=0.8, relx=0, rely=0)

    # Create new frame at the bottom right
    bottom_right_frame = ctk.CTkFrame(video_frame, corner_radius=10, fg_color="#425a78")
    bottom_right_frame.place(relwidth=0.865, relheight=0.15, relx=0.0675, rely=0.82)
    bottom_right_frame.grid_rowconfigure(0,  weight=1)
    for i in range(7):
        bottom_right_frame.grid_columnconfigure(i,  weight=1)

    # Variable to store the selected button value
    selected_button = ctk.IntVar(value=1)
    print_id = 1
    name_list = [None, "Box", "Box Corner", "Color", "Triangle", "Ellipse", "Blur", "No Box"]
    # Add 6 buttons to the new frame
    for i in range(1, 8):
        button = ctk.CTkRadioButton(bottom_right_frame, text=name_list[i], variable=selected_button, value=i, command=lambda i=i: button_choose_print(i),
                                    text_color="#FFD200", font=ctk.CTkFont(family="Arial", size=16, weight="bold"))
        button.grid(row=0, column=i-1, padx=10, pady=10, sticky="nwse")

    # Call the update_frame function
    update_frame()

    # Set the window to fullscreen
    root.attributes("-fullscreen", True)
    # Bind the escape key to exit fullscreen
    root.bind("<Escape>", exit_fullscreen)
    # Bind the F11 key to enter fullscreen mode
    root.bind("<F11>", enter_fullscreen)

    # Start the main loop
    root.mainloop()

    # Closing the app
    close = True
    start_button_click()
    cam_process.terminate()
    msg_process.terminate()
    os._exit(0)



if __name__ == "__main__":
    # Setup the socket server
    serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    # Variables
    board_ip = mp.Array("c", b"123456789101112", lock=True)
    frame_width = mp.Value('i', 0)
    frame_height = mp.Value('i', 0)
    queue_frame = mp.Queue(maxsize=1)
    queue_message = mp.Queue(maxsize=1)

    def signal_handler(sig, frame):
        print("Destroying process...")
        cam_process.terminate()
        cam_process.join()
        msg_process.terminate()
        msg_process.join()
        shw_process.terminate()
        shw_process.join()
        cv2.destroyAllWindows()
        serversocket.close()
        if board_ip.value != b'123456789101112':
            print("killing ssh process in handler")
            os.system("kill $(ps aux | grep -E 'ssh root@[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+ python3 /usr/local/x-linux-ai/people-tracking-heatmap/stai_mpu_people_tracking_heatmap.py --host_ip' | awk '{print $2}')")
            os.system("ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null root@"+str(board_ip.value.decode('utf-8'))+" pkill -f '/usr/local/x-linux-ai/people-tracking-heatmap/stai_mpu_people_tracking_heatmap.py'")
        print("Program ended")

    # Run signal handler
    signal.signal(signal.SIGINT, signal_handler)

    # Setup & start procs
    cam_process = mp.Process(target=camera_process, args=(queue_frame, frame_width, frame_height,))
    cam_process.start()

    msg_process = mp.Process(target=decode_process, args=(queue_message, serversocket))
    msg_process.start()

    shw_process = mp.Process(target=show_process, args=(queue_frame, queue_message, frame_width, frame_height, serversocket, board_ip))
    shw_process.start()

    cam_process.join()
    msg_process.join()
    shw_process.join()
