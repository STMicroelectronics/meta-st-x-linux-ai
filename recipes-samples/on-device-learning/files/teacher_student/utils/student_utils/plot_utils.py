#!/usr/bin/python3
#
# Copyright (c) 2025 STMicroelectronics.
# All rights reserved.
#
# This software is licensed under terms that can be found in the LICENSE file
# in the root directory of this software component.
# If no LICENSE file comes with this software, it is provided AS-IS.

import matplotlib.pyplot as plt
from gi.repository import GdkPixbuf
import numpy as np

# Figures params
FIGSIZE_LOSS = (5, 4)
FIGSIZE_MAP = (5, 4)
FIGSIZE_EVAL = (6, 3)
DPI = 50

def generate_default_loss_plot():
    plt.figure(figsize=FIGSIZE_LOSS)
    plt.xlabel('Epochs', fontsize=14)
    plt.ylabel('Loss', fontsize=12)
    plt.title('Validation Loss', fontsize=18, fontweight="bold", color='#03234b')
    plt.xlim(0, 5)
    plt.ylim(0, 1)
    plt.plot([], [])  # No data initially
    plt.tight_layout()
    plt.savefig('loss_curve.png', dpi=DPI)
    plt.close()

def generate_default_map_plot():
    plt.figure(figsize=FIGSIZE_MAP)
    plt.xlabel('Epochs', fontsize=14)
    plt.ylabel('mAP (%)', fontsize=12)
    plt.title('Validation mAP', fontsize=18, fontweight="bold", color='#03234b')
    plt.xlim(0, 5)
    plt.ylim(0, 100)
    plt.plot([], [])
    plt.tight_layout()
    plt.savefig('map_curve.png', dpi=DPI)
    plt.close()

def generate_default_evaluation_plot():
    categories_order = ['fp32_before', 'fp32_after', 'uint8_after']
    # Define colors for new and old data
    new_data_color = '#FFD200'  # Current color for new data
    old_data_color = 'gray'     # Gray color for old data
    plt.figure(figsize=FIGSIZE_EVAL)
    plt.ylabel('mAP (%)', fontsize=14)
    plt.title('Evaluation Metrics', fontsize=18, fontweight="bold", color='#03234b')
    plt.xticks(rotation=0, fontsize=16)
    plt.ylim(0, 100)
    plt.bar(categories_order, [0, 0, 0])
    # Add a legend
    plt.legend(handles=[
        plt.Line2D([0], [0], color=new_data_color, lw=4, label='New Data'),
        plt.Line2D([0], [0], color=old_data_color, lw=4, label='Old Data')
    ], fontsize=12, loc='best')

    plt.tight_layout()
    plt.savefig('evaluation_bar_plot.png', dpi=DPI)
    plt.close()



# -------------------------------------------------------------------------
# Plot update methods – called via GLib.idle_add so they run in the GTK main loop.
# -------------------------------------------------------------------------

def update_loss_curve_plot(loss_values, epochs_scale, loss_curve_image):
    plt.figure(figsize=FIGSIZE_LOSS)
    plt.plot(loss_values, label='Training Loss', color='#FFD200', marker='.')
    # Add shaded area under the curve
    plt.fill_between(range(len(loss_values)), loss_values, 0, color='#FFD200', alpha=0.1)
    plt.xlim(0, int(epochs_scale.get_value() - 1))
    plt.ylim(bottom=0)
    if int(epochs_scale.get_value()) <= 10:
        x_ticks = np.arange(0, int(epochs_scale.get_value()))
    else:
        x_ticks = np.arange(0, int(epochs_scale.get_value()), step=5)
    plt.xticks(x_ticks)
    plt.xlabel('Epochs', fontsize=14)
    plt.ylabel('Loss', fontsize=12)
    plt.title('Loss Curve', fontsize=18, fontweight="bold", color='#03234b')
    plt.tight_layout()
    plt.savefig('loss_curve.png', dpi=DPI)
    plt.close()
    pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale('loss_curve.png',
                                                    width=int(FIGSIZE_LOSS[0]*DPI),
                                                    height=int(FIGSIZE_LOSS[1]*DPI),
                                                    preserve_aspect_ratio=False)
    loss_curve_image.set_from_pixbuf(pixbuf)
    return loss_curve_image


def update_map_curve_plot(map_values, epochs_scale, map_curve_image):
    print(f"Map values: {map_values}")
    plt.figure(figsize=FIGSIZE_MAP)
    plt.plot(map_values, label='Training mAP', color='#425a78', marker='None')
    plt.fill_between(range(len(map_values)), map_values, 0, color='#425a78', alpha=0.1)
    plt.xlim(0, int(epochs_scale.get_value() - 1))
    plt.ylim(0, 100)
    if int(epochs_scale.get_value()) <= 10:
        x_ticks = np.arange(0, int(epochs_scale.get_value()))
    else:
        x_ticks = np.arange(0, int(epochs_scale.get_value()), step=5)
    plt.xticks(x_ticks)
    plt.xlabel('Epochs', fontsize=14)
    plt.ylabel('mAP (%)', fontsize=12)
    plt.title('Training mAP Curve', fontsize=18, fontweight="bold", color='#03234b')
    plt.tight_layout()
    plt.savefig('map_curve.png', dpi=DPI)
    plt.close()
    pixbuf = GdkPixbuf.Pixbuf.new_from_file_at_scale('map_curve.png',
                                                    width=int(FIGSIZE_MAP[0]*DPI),
                                                    height=int(FIGSIZE_MAP[1]*DPI),
                                                    preserve_aspect_ratio=False)
    map_curve_image.set_from_pixbuf(pixbuf)
    return map_curve_image


def update_evaluation_plot(eval_data):
    # Define categories for new and old data
    plt_categories = ['fp32_before', 'fp32_after', 'uint8_after']
    new_categories = ['new_before', 'new_after', 'quant_new']
    old_categories = ['old_before', 'old_after', 'quant_old']

    # Extract values for new and old data
    new_values = [float(eval_data.get(key, 0) or 0) for key in new_categories]
    old_values = [float(eval_data.get(key, 0) or 0) for key in old_categories]

    print(new_values)
    print(old_values)

    # Define colors for new and old data
    new_data_color = '#FFD200'  # Yellow color for new data
    old_data_color = '#03234b'  # Gray color for old data

    # Plot for new data
    plt.figure(figsize=FIGSIZE_EVAL)
    plt.bar(plt_categories, new_values, color=new_data_color)
    plt.ylabel('mAP (%)', fontsize=14)
    plt.title('Evaluation Metrics - New Data', fontsize=18, fontweight="bold", color='#03234b')
    plt.xticks(rotation=0, fontsize=16)
    plt.ylim(0, 100)
    for bar, value in zip(plt.bar(plt_categories, new_values), new_values):
        if value > 0:
            plt.text(bar.get_x() + bar.get_width() / 2, bar.get_height() / 2, f'{value:.2f}%', ha='center', va='center', color='white', fontsize=14, fontweight="bold")
    plt.tight_layout(rect=[0, 0, 1, 1])
    plt.savefig('evaluation_bar_plot_new.png', dpi=DPI)
    plt.close()

    # Plot for old data
    plt.figure(figsize=FIGSIZE_EVAL)
    plt.bar(plt_categories, old_values, color=old_data_color)
    plt.ylabel('mAP (%)', fontsize=14)
    plt.title('Evaluation Metrics - Old Data', fontsize=18, fontweight="bold", color='#03234b')
    plt.xticks(rotation=0, fontsize=16)
    plt.ylim(0, 100)
    for bar, value in zip(plt.bar(plt_categories, old_values), old_values):
        if value > 0:
            plt.text(bar.get_x() + bar.get_width() / 2, bar.get_height() / 2, f'{value:.2f}%', ha='center', va='center', color='white', fontsize=14, fontweight="bold")
    plt.tight_layout(rect=[0, 0, 1, 1])
    plt.savefig('evaluation_bar_plot_old.png', dpi=DPI)
    plt.close()