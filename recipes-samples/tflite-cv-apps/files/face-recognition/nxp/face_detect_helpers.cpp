/* Copyright (C) 2019 NXP Semiconductors
* See LICENSE file in the project root for full license information.
* Author: 
* Xiyue Shi xiyue_shi@163.com
* Devin Jiao bin.jiao@nxp.com
* Any questions, please contact with bin.jiao@nxp.com
* 19/06/2019
*
* SPDX-License-Identifier:    Apache-2.0
*
*/

#include "face_detect_helpers.h"

#define LOG(x) std::cerr

namespace tflite
{
namespace facerec
{

CascadeClassifier face_cascade;
VideoCapture cap;
Settings *s;
Scalar line_color;
Scalar text_color;
Mat white_img;

const int display_width = 780;
const int display_height = 460;
Mat display;
const int rescaled_width = 320;

Rect left_region;
Rect right_region;
Rect label_region;
Rect similarity_region;
Rect add_button;
Rect stop_button;
std::vector<Rect> input_buttons;
std::vector<std::string> keyboard =
    {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0",
     "q", "w", "e", "r", "t", "y", "u", "i", "o", "p",
     "a", "s", "d", "f", "g", "h", "j", "k", "l",
     "z", "x", "c", "v", "b", "n", "m"};

String button_text("Add new person");
String stop_text("Stop");

std::string new_label;
Mat last_frame;
bool pause;
String empty = "";
String similarity_text = "Similarity: ";

void init_face_detect(Settings *setting)
{
  String face_cascade_name = "haarcascade_frontalface_alt.xml";
  if( !face_cascade.load(face_cascade_name) )
    printf("--(!)Error loading haarcascade_frontalface_alt.xml\n");
  s = setting;
}

void close_camera()
{
  cap.release();
  destroyAllWindows();
}

void add_image(Mat &img, Rect ROI)
{
  Mat temp;
  resize(img, temp, Size(ROI.width, ROI.height));
  temp.copyTo(display(ROI));
}

void change_label(String &label)
{
  add_image(white_img, label_region);
  if (label.empty())
  {
    return;
  }
  putText(display(label_region), label, 
          Point(label_region.width * 0.2, label_region.height * 0.7), 
          FONT_HERSHEY_SIMPLEX, 0.8, text_color);
}

void change_similarity(float similarity)
{
  add_image(white_img, similarity_region);
  if (similarity == 0)
  {
    return;
  }
  putText(display(similarity_region), similarity_text + std::to_string(similarity), 
          Point(label_region.width * 0.2, label_region.height * 0.7), 
          FONT_HERSHEY_SIMPLEX, 0.8, text_color);
}

void callBackFunc(int event, int x, int y, int flags, void *userdata)
{
  if (event != EVENT_LBUTTONDOWN)
  {
    return;
  }
  if (add_button.contains(Point(x, y)))
  {
    Mat new_img;
    if (pause)
    {
      new_img = last_frame;
      pause = false;
    }
    else
    {
      cap >> new_img;
    }
    add_new_face(new_img, new_label, s);
    new_label.clear();
  }
  else if (stop_button.contains(Point(x, y)))
  {
    close_camera();
    exit(-1);
  }
  else
  {
    for (int i = 0; i < input_buttons.size(); i++)
    {
      if (input_buttons[i].contains(Point(x, y)))
      {
        new_label += keyboard[i];
        change_label(new_label);
        pause = true;
        return;
      }
    }
  }
}

void init_display()
{
  display = Mat(display_height, display_width,
                CV_8UC3, Scalar(255, 255, 255));
  String window_name("Display window");
  namedWindow(window_name, WINDOW_AUTOSIZE);
  setMouseCallback(window_name, callBackFunc);

  line_color = Scalar(255, 0, 0);
  text_color = Scalar(0, 0, 0);
  white_img = Mat(display_width, display_width,
                  CV_8UC3, Scalar(255, 255, 255));
}

void init_camera()
{
  cap.open(s->camera);
  if (!cap.isOpened())
  {
    LOG(FATAL) << "Capture from camera #" << s->camera << " didn't work"
               << "\n\r";
    exit(-1);
  }
  cap.set(CAP_PROP_FRAME_WIDTH, 320);
  cap.set(CAP_PROP_FRAME_HEIGHT, 240);
}

void init_regions()
{
  Mat frame;
  cap >> frame;
  int width = frame.cols;
  int height = frame.rows;

  LOG(INFO) << "VAB::: " << width << "x" << height << "\n\r";
  LOG(INFO) << "VAB::: display " << display_width << "x" << display_height << "\n\r";

  int x = (int)(display_width * 0.02);
  int y = (int)((display_height - height) / 2) + 20;
  left_region = Rect(x, y, width, height);

  width = (int)(display_width * 0.3);
  height = width;
  x = (int)(display_width * 0.1 + frame.cols);
  y = (int)((display_height - height) / 2) - 100;
  right_region = Rect(x, y, width, height);

  label_region = Rect(20, 20, width, 50);
  similarity_region = Rect(20, 70, width, 50);

  LOG(INFO) << "VAB::: left_region " << left_region.width << "x" << left_region.height << " [x=" << left_region.x << ", y=" << left_region.y << "]\n\r";
  LOG(INFO) << "VAB::: right_region " << right_region.width << "x" << right_region.height << " [x=" << right_region.x << ", y=" << right_region.y << "]\n\r";
  LOG(INFO) << "VAB::: label_region " << label_region.width << "x" << label_region.height << " [x=" << label_region.x << ", y=" << label_region.y << "]\n\r";
  LOG(INFO) << "VAB::: similarity_region " << similarity_region.width << "x" << similarity_region.height << " [x=" << similarity_region.x << ", y=" << similarity_region.y << "]\n\r";
}

void init_add_button()
{
  LOG(INFO) << "VAB::: init_add_button in\n\r";
  add_button = Rect(left_region.x + 120, left_region.height + left_region.y + 10, 200, 35);
  Scalar bg_color = Scalar(80, 160, 255);
  rectangle(display, add_button, bg_color, FILLED);
  putText(display(add_button), button_text,
          Point(add_button.width * 0.1, add_button.height * 0.7),
          FONT_HERSHEY_SIMPLEX, 0.6, text_color);
  LOG(INFO) << "VAB::: init_add_button out\n\r";
}

void init_stop_button()
{
  LOG(INFO) << "VAB::: init_stop_button in\n\r";
  Scalar bg_color = Scalar(0, 0, 255);
  stop_button = Rect(left_region.x, left_region.height + left_region.y + 10, 100, 35);
  rectangle(display, stop_button, bg_color, FILLED);
  putText(display(stop_button), stop_text,
          Point(add_button.width * 0.1, add_button.height * 0.7),
          FONT_HERSHEY_SIMPLEX, 0.6, text_color);
  LOG(INFO) << "VAB::: init_stop_button out\n\r";
}

void add_input_buttons(std::vector<std::string> characters, float offset, int row_id)
{
  LOG(INFO) << "VAB::: add_input_buttons in\n\r";
  int x = right_region.x + offset * 30;
  int y = right_region.y + right_region.height + 0 + 30 * row_id;
  Scalar bg_color = Scalar(0, 255, 255);
  for (int i = 0; i < characters.size(); i++)
  {
    input_buttons.push_back(Rect(x + i * 30, y, 20, 20));
    rectangle(display, input_buttons.back(), bg_color, FILLED);
    putText(display(input_buttons.back()), characters[i],
            Point(10, 20), FONT_HERSHEY_SIMPLEX, 0.6, text_color);
  }
  LOG(INFO) << "VAB::: add_input_buttons out\n\r";
}

void init_input_buttons()
{
  std::vector<std::string> row_1 =
      {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
  std::vector<std::string> row_2 =
      {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"};
  std::vector<std::string> row_3 =
      {"a", "s", "d", "f", "g", "h", "j", "k", "l"};
  std::vector<std::string> row_4 =
      {"z", "x", "c", "v", "b", "n", "m"};

  add_input_buttons(row_1, 0, 0);
  add_input_buttons(row_2, 0, 1);
  add_input_buttons(row_3, 0.5, 2);
  add_input_buttons(row_4, 1.5, 3);
}

void init_window()
{
  init_display();
  init_camera();
  init_regions();
  init_add_button();
  init_stop_button();
  init_input_buttons();
}

Mat &get_display()
{
  return display;
}

void show_matched_face(Mat &face, String &label, float similarity)
{
  add_image(face, right_region);
  change_label(label);
  change_similarity(similarity);
}

void no_matched_face()
{
  add_image(white_img, right_region);
  change_label(empty);
  change_similarity(0);
}

Mat preprocess(Mat &img, float scale)
{
  Mat gray_img, small_img;
  cvtColor(img, gray_img, COLOR_BGR2GRAY);
  resize(gray_img, small_img, Size(), scale, scale);
  equalizeHist(small_img, small_img);
  return small_img;
}

Rect detect_face(Mat &img, float scale)
{
  std::vector<Rect> faces;
  face_cascade.detectMultiScale(img, faces, 1.3, 3,
                                CASCADE_FIND_BIGGEST_OBJECT, Size(65, 65));
  Rect faceRef;
  if (faces.empty())
  {
    return faceRef;
  }
  for (int i = 0; i < faces.size(); i++)
  {
    if (faces[i].area() > faceRef.area())
    {
      faceRef = faces[i];
    }
  }
  faceRef = Rect(Point(cvRound(faceRef.x / scale),
                       cvRound(faceRef.y / scale)),
                 Point(cvRound((faceRef.x + faceRef.width - 1) / scale),
                       cvRound((faceRef.y + faceRef.height - 1) / scale)));
  return faceRef;
}

Mat get_data_face(Mat &img)
{
  float scale = (float)rescaled_width / img.cols;
  if (scale > 1)
    scale = 1;
  Mat small_img = preprocess(img, scale);
  Rect faceRef = detect_face(small_img, scale);
  Mat face;
  if (!faceRef.empty())
  {
    cvtColor(img(faceRef), face, COLOR_BGR2RGB);
  }
  return face;
}

Mat draw_test_face(Mat frame, Rect faceRef)
{
  Mat face;
  if (!faceRef.empty())
  {
    rectangle(frame, faceRef, line_color, 3);
    cvtColor(frame(faceRef), face, COLOR_BGR2RGB);
  }
  add_image(frame, left_region);
  return face;
}

Mat get_test_face()
{
  Mat frame;
  cap >> frame;
  last_frame = frame.clone();

  float scale = (float)rescaled_width / frame.cols;
  if (scale > 1)
    scale = 1;

  Mat small_img = preprocess(frame, scale);
  Rect faceRef = detect_face(small_img, scale);
  return draw_test_face(frame, faceRef);
}

bool is_pause()
{
  return pause;
}

} // namespace facerec
} // namespace tflite
