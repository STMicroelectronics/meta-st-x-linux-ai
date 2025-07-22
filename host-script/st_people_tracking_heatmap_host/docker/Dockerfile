# Use the official ubuntu 20.04 image from the Docker Hub
FROM ubuntu:20.04

# Setup environment
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Paris

# Set the working directory in the container
WORKDIR /app


# Install libjpeg and other necessary libraries
RUN apt-get update && \
    apt-get install -y python3.8 python3.8-dev python3-pip python3-tk  \
                       libgtk-3.0 ffmpeg openssh-client \
                       gstreamer1.0-plugins-base gstreamer1.0-plugins-bad gstreamer1.0-plugins-good gstreamer1.0-tools gstreamer1.0-libav

# Copy the requirements file into the container at /app
COPY requirements.txt .
COPY opencv_python-4.11.0.86-cp38-cp38-linux_x86_64.whl .
COPY libjpeg.so.8 /usr/lib/x86_64-linux-gnu/libjpeg.so.8

# Install any dependencies specified in requirements.txt
RUN python3 -m pip install --no-cache-dir -r requirements.txt

# Copy the rest of the application code into the container at /app
COPY st_people_tracking_heatmap_host.py .
COPY xlinuxai.png .

# Command to run the application
CMD ["python3", "st_people_tracking_heatmap_host.py"]