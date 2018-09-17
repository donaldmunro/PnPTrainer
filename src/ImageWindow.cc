#include "ImageWindow.hh"

#include <iostream>

#include <opencv2/imgcodecs.hpp>

#include <QList>
#include <QAction>
#include <QGuiApplication>
#include <QScreen>
#include <QFont>
#include <QFontMetrics>
#include <QMenuBar>
#include <QMenu>
#include <QIntValidator>
#include <QDoubleValidator>
#include <QImageReader>
#include <QFileDialog>
#include <QStandardPaths>
#include <QImageWriter>
#include <QLabel>
#include <QAbstractItemView>
#include <QFrame>
#include <QSplitter>
#include <QDebug>

#include "tinyformat.h"

ImageWindow::ImageWindow(MatchWin* matchWindow, QWidget *parent) : QMainWindow(parent), match_window(matchWindow),
                        panel_layout(new QVBoxLayout(this)), image_holder(new CVQtScrollableImage(this))
//-----------------------------------------------------------------------------------------------------------------
{
   resize(QGuiApplication::primaryScreen()->availableSize() * 4 / 5);

   tabs.addTab(image_holder, "Image");
   QWidget* panel = new QWidget();
   setup_features_control(panel_layout);
   panel->setLayout(panel_layout);
   panel->updateGeometry();
   tabs.addTab(panel, "Feature Controls");
   setCentralWidget(&tabs);

   std::function<void(cv::Mat&, cv::Rect&, bool, bool, bool)> f = std::bind(&ImageWindow::on_region_selected, this,
                                                                            std::placeholders::_1, std::placeholders::_2,
                                                                            std::placeholders::_3, std::placeholders::_4,
                                                                            std::placeholders::_5);
   image_holder->set_roi_callback(f);

//   open_img_action = new QAction(tr("&Open Image"), this);
//   connect(open_img_action, &QAction::triggered, this, &ImageWindow::load_image_menu);
//   open_3d_action = new QAction(tr("Open &3D"), this);
//   connect(open_3d_action, &QAction::triggered, this, &ImageWindow::load3d_menu);
   exit_action = new QAction("E&xit", this);
   connect(exit_action, &QAction::triggered, this, &ImageWindow::exit);

   file_menu = menuBar()->addMenu("&File");
//   file_menu->addAction(open_img_action);
//   file_menu->addAction(open_3d_action);
//   file_menu->addSeparator();
   file_menu->addAction(exit_action);
}

void ImageWindow::open()
//----------------------
{
//   QFileDialog dialog(this, tr("Select Directory"), QString("."));
//   dialog.setOption(QFileDialog::ShowDirsOnly);
//   std::string imgfile, plyfile;
//   while (dialog.exec() == QDialog::Accepted &&
//          !open_dir(dialog.selectedFiles().first().toStdString(), imgfile, plyfile)) {}
//   if (! imgfile.empty())
//      load(imgfile);
}

void ImageWindow::on_region_selected(cv::Mat& roi, cv::Rect& roirect, bool is_shift, bool is_ctrl, bool is_alt)
//-------------------------------------------------------------------------------------------------------------
{
   region_features.clear();
   for (size_t i=0; i<current_keypts.size(); i++)
   {
      cv::KeyPoint kp = current_keypts[i];
      if (roirect.contains(kp.pt))
      {
         cv::KeyPoint kp2(kp);
         kp2.pt.x -= roirect.x;
         kp2.pt.y -= roirect.y;
         cv::Mat descriptor = current_descriptors.row(i);
         region_features.emplace_back(i, kp2, descriptor, false);
      }
   }
//   match_window->update_image(roi, roirect);
   const cv::Mat& image = image_holder->get_image();
   cv::Mat img(roirect.size(), image.type());
   image(roirect).copyTo(img);
   match_window->update_image(img, roirect, &region_features, false);
   match_window->request_focus();
}

void ImageWindow::setup_features_control(QLayout *layout)
//------------------------------------------------------
{
   int buttonNo = 0;
   QFrame* frame = new QFrame(this);
   QHBoxLayout* frame_layout = new QHBoxLayout(this);
   QRadioButton *radio = new QRadioButton(tr("&ORB"), this);
   radio->setChecked(true);
   frame_layout->addWidget(radio);
   feature_detectors.addButton(radio, buttonNo++);
   frame_layout->addSpacing(4);
   editOrbNoFeatures = new QLineEdit("1000", this);
   editOrbNoFeatures->setValidator(new QIntValidator);
   frame_layout->addWidget(new QLabel("Max Features:")); frame_layout->addWidget(editOrbNoFeatures);
   frame_layout->addSpacing(4);
   editOrbScale = new QLineEdit("1.1", this);
   editOrbScale->setValidator(new QDoubleValidator);
   frame_layout->addWidget(new QLabel("Scale Factor:")); frame_layout->addWidget(editOrbScale);
   frame_layout->addSpacing(4);
   listOrbWTA = new QListWidget(this);
   listOrbWTA->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
   listOrbWTA->setSelectionMode(QAbstractItemView::SelectionMode::SingleSelection);
   listOrbWTA->setFixedHeight(QFontMetrics(listOrbWTA->font()).height()*4 + 5);
   new QListWidgetItem(tr("2"), listOrbWTA);
   new QListWidgetItem(tr("3"), listOrbWTA);
   new QListWidgetItem(tr("4"), listOrbWTA);
   listOrbWTA->item(0)->setSelected(true);
   frame_layout->addWidget(new QLabel("WTA:")); frame_layout->addWidget(listOrbWTA);
   frame_layout->addSpacing(4);
   listOrbScore = new QListWidget(this);
   listOrbScore->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
   listOrbScore->setFixedHeight(QFontMetrics(listOrbScore->font()).height()*3);
   new QListWidgetItem(tr("HARRIS"), listOrbScore);
   new QListWidgetItem(tr("FAST"), listOrbScore);
   listOrbScore->item(0)->setSelected(true);
   frame_layout->addWidget(new QLabel("Score")); frame_layout->addWidget(listOrbScore);
   frame_layout->addSpacing(4);
   editOrbPatchSize = new QLineEdit("31", this);
   editOrbPatchSize->setValidator(new QIntValidator);
   frame_layout->addWidget(new QLabel("Patch Size:")); frame_layout->addWidget(editOrbPatchSize);
   frame->setLayout(frame_layout);
   frame->updateGeometry();
   layout->addWidget(frame);

#ifdef HAVE_OPENCV_XFEATURES2D
   frame = new QFrame(this);
   frame_layout = new QHBoxLayout(this);
   radio = new QRadioButton("&SIFT", this);
   frame_layout->addWidget(radio);
   feature_detectors.addButton(radio, buttonNo++);
   frame_layout->addSpacing(4);
   editSiftNoFeatures = new QLineEdit("1000", this);
   editSiftNoFeatures->setValidator(new QIntValidator);
   frame_layout->addWidget(new QLabel("Max Features:")); frame_layout->addWidget(editSiftNoFeatures);
   frame_layout->addSpacing(4);
   editSiftOctaveLayers = new QLineEdit("3", this);
   editSiftOctaveLayers->setValidator(new QIntValidator);
   frame_layout->addWidget(new QLabel("Octave Layers:")); frame_layout->addWidget(editSiftOctaveLayers);
   frame_layout->addSpacing(4);
   editSiftContrast = new QLineEdit("0.04", this);
   editSiftContrast->setValidator(new QDoubleValidator);
   frame_layout->addWidget(new QLabel("Contr. Thresh:")); frame_layout->addWidget(editSiftContrast);
   frame_layout->addSpacing(4);
   editSiftEdge = new QLineEdit("10", this);
   editSiftEdge->setValidator(new QIntValidator);
   frame_layout->addWidget(new QLabel("Edge Thresh:")); frame_layout->addWidget(editSiftEdge);
   frame_layout->addSpacing(4);
   editSiftSigma = new QLineEdit("1.6", this);
   editSiftSigma->setValidator(new QDoubleValidator);
   frame_layout->addWidget(new QLabel("\u03C3")); frame_layout->addWidget(editSiftSigma);
   frame->setLayout(frame_layout);
   frame->updateGeometry();
   layout->addWidget(frame);

   frame = new QFrame(this);
   frame_layout = new QHBoxLayout(this);
   radio = new QRadioButton("S&URF", this);
   frame_layout->addWidget(radio);
   feature_detectors.addButton(radio, buttonNo++);
   frame_layout->addSpacing(4);
   editSurfThreshold = new QLineEdit("100", this);
   editSurfThreshold->setValidator(new QIntValidator);
   frame_layout->addWidget(new QLabel("Hessian Thresh:")); frame_layout->addWidget(editSurfThreshold);
   frame_layout->addSpacing(4);
   editSurfOctaves = new QLineEdit("4", this);
   editSurfOctaves->setValidator(new QIntValidator);
   frame_layout->addWidget(new QLabel("Octaves:")); frame_layout->addWidget(editSurfOctaves);
   frame_layout->addSpacing(4);
   editSurfOctaveLayers = new QLineEdit("3", this);
   editSurfOctaveLayers->setValidator(new QIntValidator);
   frame_layout->addWidget(new QLabel("Octave Layers:")); frame_layout->addWidget(editSurfOctaveLayers);
   frame_layout->addSpacing(4);
   chkSurfExtended = new QCheckBox("Ext. Desc", this);
   chkSurfExtended->setChecked(false);
   frame_layout->addWidget(chkSurfExtended);
   frame->setLayout(frame_layout);
   frame->updateGeometry();
   layout->addWidget(frame);
#endif

   frame = new QFrame(this);
   frame_layout = new QHBoxLayout(this);
   radio = new QRadioButton("&Akaze", this);
   frame_layout->addWidget(radio);
   feature_detectors.addButton(radio, buttonNo++);
   frame_layout->addSpacing(4);
   listAkazeDescriptors = new QListWidget(this);
   listAkazeDescriptors->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
   listAkazeDescriptors->setFixedHeight(QFontMetrics(listAkazeDescriptors->font()).height()*5 + 5);
   frame_layout->addWidget(new QLabel("Descriptor:"));
   new QListWidgetItem(tr("DESCRIPTOR_MLDB"), listAkazeDescriptors);
   new QListWidgetItem(tr("DESCRIPTOR_MLDB_UPRIGHT"), listAkazeDescriptors);
   new QListWidgetItem(tr("DESCRIPTOR_KAZE"), listAkazeDescriptors);
   new QListWidgetItem(tr("DESCRIPTOR_KAZE_UPRIGHT"), listAkazeDescriptors);
   listAkazeDescriptors->item(0)->setSelected(true);
   frame_layout->addWidget(listAkazeDescriptors);
   frame_layout->addSpacing(4);
   frame_layout->addWidget(new QLabel("Threshold:"));
   editAkazeThreshold = new QLineEdit("0.001", this);
   editAkazeThreshold->setValidator(new QDoubleValidator);
   frame_layout->addWidget(editAkazeThreshold);
   frame_layout->addSpacing(4);
   editAkazeOctaves = new QLineEdit("4", this);
   editAkazeOctaves->setValidator(new QIntValidator);
   frame_layout->addWidget(new QLabel("Octaves:")); frame_layout->addWidget(editAkazeOctaves);
   frame_layout->addSpacing(4);
   editAkazeOctaveLayers = new QLineEdit("4", this);
   editAkazeOctaveLayers->setValidator(new QIntValidator);
   frame_layout->addWidget(new QLabel("Octave Layers:")); frame_layout->addWidget(editAkazeOctaveLayers);
   frame_layout->addSpacing(4);
   frame_layout->addWidget(new QLabel("Diffusivity:"));
   listAkazeDiffusivity  = new QListWidget(this);
   listAkazeDiffusivity->setSelectionBehavior(QAbstractItemView::SelectionBehavior::SelectRows);
   listAkazeDiffusivity->setFixedHeight(QFontMetrics(listAkazeDiffusivity->font()).height()*5+5);
   new QListWidgetItem(tr("DIFF_PM_G2"), listAkazeDiffusivity);
   new QListWidgetItem(tr("DIFF_PM_G1"), listAkazeDiffusivity);
   new QListWidgetItem(tr("DIFF_WEICKERT"), listAkazeDiffusivity);
   new QListWidgetItem(tr("DIFF_CHARBONNIER"), listAkazeDiffusivity);
   listAkazeDiffusivity->item(0)->setSelected(true);
   frame_layout->addWidget(listAkazeDiffusivity);
   frame->setLayout(frame_layout);
   frame->updateGeometry();
   layout->addWidget(frame);

   frame = new QFrame(this);
   frame_layout = new QHBoxLayout(this);
   radio = new QRadioButton("&BRISK", this);
   frame_layout->addWidget(radio);
   feature_detectors.addButton(radio, buttonNo++);
   frame_layout->addSpacing(4);
   editBriskThreshold = new QLineEdit("30", this);
   editBriskThreshold->setValidator(new QIntValidator);
   frame_layout->addWidget(new QLabel("Threshold:")); frame_layout->addWidget(editBriskThreshold);
   frame_layout->addSpacing(4);
   editBriskOctaves = new QLineEdit("3", this);
   editBriskOctaves->setValidator(new QIntValidator);
   frame_layout->addWidget(new QLabel("Octaves:")); frame_layout->addWidget(editBriskOctaves);
   frame_layout->addSpacing(4);
   editBriskScale = new QLineEdit("1.0", this);
   editBriskScale->setValidator(new QDoubleValidator);
   frame_layout->addWidget(new QLabel("Pattern Scale:")); frame_layout->addWidget(editBriskScale);
   frame->setLayout(frame_layout);
   frame->updateGeometry();
   layout->addWidget(frame);

   frame = new QFrame(this);
   frame_layout = new QHBoxLayout(this);
   frame_layout->setAlignment(Qt::AlignLeft);
   chkDelDups = new QCheckBox("Delete Duplicates", this);
   chkDelDups->setChecked(false);
   frame_layout->addWidget(chkDelDups);
   editBest = new QLineEdit("-1", this);
   editBest->setValidator(new QIntValidator);
   editBest->setFixedWidth(QFontMetrics(editBest->font()).width('X')*5);
   QLabel *label = new QLabel("Top n:");
   label->setFixedWidth(QFontMetrics(label->font()).width('X')*7+5);
   frame_layout->addWidget(label); frame_layout->addWidget(editBest);
   frame_layout->addSpacing(4);
   chkRichKps = new QCheckBox("Display Rich Keypoints", this);
   chkRichKps->setChecked(false);
   frame_layout->addWidget(chkRichKps);
   detectButton = new QPushButton("&Detect", this);
   detectButton->setDefault(true);
   detectButton->setFixedWidth(QFontMetrics(detectButton->font()).width('X')*6+5);
   connect(detectButton, SIGNAL (released()), this, SLOT (on_detect()));
   frame_layout->addWidget(detectButton);
   frame->setLayout(frame_layout);
   frame->updateGeometry();
   layout->addWidget(frame);

   frame = new QFrame(this);
   frame_layout = new QHBoxLayout(this);
   selectedColorButton = new QPushButton("Selected Color", this);
   QColor col = QColor(selected_colour);
   QString qss = QString("background-color: %1").arg(col.name());
   selectedColorButton->setStyleSheet(qss);
   selectedColorButton->setFixedWidth(QFontMetrics(selectedColorButton->font()).width('X')*14+5);
   connect(selectedColorButton, SIGNAL (released()), this, SLOT (on_selected_color_change()));
   frame_layout->addWidget(selectedColorButton);
   unselectedColorButton = new QPushButton("Unselected Color", this);
   col = QColor(unselected_colour);
   qss = QString("background-color: %1").arg(col.name());
   unselectedColorButton->setStyleSheet(qss);
   unselectedColorButton->setFixedWidth(QFontMetrics(selectedColorButton->font()).width('X')*15+5);
   connect(unselectedColorButton, SIGNAL (released()), this, SLOT (on_unselected_color_change()));
   frame_layout->addWidget(unselectedColorButton);
   frame->setLayout(frame_layout);
   frame->updateGeometry();
   layout->addWidget(frame);
}

inline bool valInt(QLineEdit *edit, int& v, const std::string& errmsg)
{
   bool ok;
   v = edit->text().trimmed().toInt(&ok);
   if (! ok)
   {
      std::stringstream errs;
      errs <<  errmsg << " (" << edit->text().toStdString() << ")";
      msg(errs);
      return false;
   }
   return true;
}

inline float valFloat(QLineEdit *edit, float& v, const std::string& errmsg)
{
   bool ok;
   v = edit->text().trimmed().toFloat(&ok);
   if (! ok)
   {
      std::stringstream errs;
      errs <<  errmsg << " (" <<  edit->text().toStdString() << ")";
      msg(errs);
      return false;
   }
   return true;
}

QString listItem(const QListWidget *list)
//---------------------------------------
{
   QListWidgetItem *item = list->currentItem();
   if ( (item == nullptr) || (item->text().isEmpty()) )
   {
      const QList<QListWidgetItem *> &sel = list->selectedItems();
      if (sel.size() > 0)
         item = sel.at(0);
   }
   if ( (item == nullptr) || (item->text().isEmpty()) )
      item = list->item(0);
   if (item == nullptr)
      return QString("");
   return item->text();
}

void ImageWindow::on_detect()
//---------------------------
{
   std::string s = feature_detectors.checkedButton()->text().toStdString();
   cv::Ptr<cv::Feature2D> detector;
   if (! create_detector(s, detector))
   {
      std::stringstream errs;
      errs << "Could not create an OpenCV feature detector of type " << s;
      msg(errs);
      return;
   }

   int n;
   if (! valInt(editBest, n, "Invalid top n"))
      n = -1;
   current_keypts.clear();
   current_descriptors.release();
   if ( (chkDelDups->isChecked()) || (n > 0) )
   {
      detector->detect(pre_detect_image, current_keypts);
      if (chkDelDups->isChecked())
         cv::KeyPointsFilter::removeDuplicated(current_keypts);
      if (n > 0)
         cv::KeyPointsFilter::retainBest(current_keypts, n);
      if (! current_keypts.empty())
         detector->compute(pre_detect_image, current_keypts, current_descriptors);
   }
   else
      detector->detectAndCompute(pre_detect_image, cv::noArray(), current_keypts, current_descriptors);

   cv::Mat img;
   cv::drawKeypoints(pre_detect_image, current_keypts, img, cv::Scalar(0, 255, 255),
                     (chkRichKps->isChecked()) ? cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS : 0);
   image_holder->set_image(img);
   tabs.setCurrentIndex(0);
}

bool ImageWindow::create_detector(std::string detectorName, cv::Ptr<cv::Feature2D>& detector)
//--------------------------------------------------------------------------------------------------
{
   detector.release();
   detectorName.erase(std::remove(detectorName.begin(), detectorName.end(), '&'), detectorName.end());
   std::transform(detectorName.begin(), detectorName.end(), detectorName.begin(), ::toupper);
   if (detectorName == "ORB")
   {
      int cfeatures, edgeThreshold=31, patchSize = 31;
      float scale;
      if (! valInt(editOrbNoFeatures, cfeatures, "ORB invalid number of features")) return false;
      if (! valFloat(editOrbScale, scale, "ORB invalid scale factor")) return false;
      QString score = listItem(listOrbScore);
      int scoreType = cv::ORB::HARRIS_SCORE;
      if (score.toStdString() == "FAST")
         scoreType = cv::ORB::FAST_SCORE;
      if (! valInt(editOrbPatchSize, patchSize, "ORB invalid patch size")) return false;
      edgeThreshold = patchSize;
      QString wta = listItem(listOrbWTA);
      detector = cv::ORB::create(cfeatures, scale, 8, edgeThreshold, 0, wta.toInt(), scoreType,
                                 patchSize);
      detector_info.set("ORB", { { "nfeatures", std::to_string(cfeatures) }, { "scaleFactor", std::to_string(scale) },
                                 { "nlevels",  "8"}, { "firstLevel",  "0" }, { "WTA_K", wta.toStdString() },
                                 { "scoreType", std::to_string(scoreType) }, { "patchSize", std::to_string(patchSize) },
                                 { "edgeThreshold", std::to_string(patchSize) } });
   }
#ifdef HAVE_OPENCV_XFEATURES2D
   if (detectorName == "SIFT")
   {
      int noFeatures, layers, edge;
      float contrast, sigma;
      if (! valInt(editSiftNoFeatures, noFeatures, "SIFT invalid max no. features")) return false;
      if (! valInt(editSiftOctaveLayers, layers, "SIFT invalid octave layers")) return false;
      if (! valFloat(editSiftContrast, contrast, "SIFT invalid contrast")) return false;
      if (! valInt(editSiftEdge, edge, "Sift edge threshold")) return false;
      if (! valFloat(editSiftSigma, sigma, "SIFT \u03C3 (sigma)")) return false;
      detector = cv::xfeatures2d::SIFT::create(noFeatures, layers, contrast, edge, sigma);
      detector_info.set("SIFT", { { "nfeatures", std::to_string(noFeatures) }, { "nOctaveLayers", std::to_string(layers) },
                                  { "contrastThreshold", std::to_string(contrast) },
                                  { "edgeThreshold", std::to_string(edge) }, { "sigma", std::to_string(sigma) } });
   }
   if (detectorName == "SURF")
   {
      int hessian, octaves, octaveLayers;
      if (! valInt(editSurfThreshold, hessian, "SURF Hessian threshold")) return false;
      if (! valInt(editSurfOctaves, octaves, "SURF octaves")) return false;
      if (! valInt(editSurfOctaveLayers, octaveLayers, "SURF octave layers")) return false;
      detector = cv::xfeatures2d::SURF::create(hessian, octaves, octaveLayers, chkSurfExtended->isChecked());
      detector_info.set("SURF", { { "hessianThreshold", std::to_string(hessian) }, { "nOctaves", std::to_string(octaves) },
                                  { "nOctaveLayers", std::to_string(octaveLayers) },
                                  { "extended", std::to_string(chkSurfExtended->isChecked()) },
                                  { "upright", "false" } });
   }
#endif
   if (detectorName == "AKAZE")
   {
      float threshold;
      int descriptor, diffusivity, octaves, octaveLayers;
      std::string descriptorName = listItem(listAkazeDescriptors).toStdString();
      if (descriptorName == "DESCRIPTOR_MLDB_UPRIGHT")
         descriptor = cv::AKAZE::DESCRIPTOR_MLDB_UPRIGHT;
      else if (descriptorName == "DESCRIPTOR_KAZE")
         descriptor = cv::AKAZE::DESCRIPTOR_KAZE;
      else if (descriptorName == "DESCRIPTOR_KAZE_UPRIGHT")
         descriptor = cv::AKAZE::DESCRIPTOR_KAZE_UPRIGHT;
      else
         descriptor = cv::AKAZE::DESCRIPTOR_MLDB;
      if (! valFloat(editAkazeThreshold, threshold, "AKAZE threshold")) return false;
      if (! valInt(editAkazeOctaves, octaves, "SURF octaves")) return false;
      if (! valInt(editAkazeOctaveLayers, octaveLayers, "SURF octave layers")) return false;
      std::string diffusivityName = listItem(listAkazeDiffusivity).toStdString();
      if (diffusivityName == "DIFF_PM_G1")
         diffusivity = cv::KAZE::DIFF_PM_G1;
      else if (diffusivityName == "DIFF_WEICKERT")
         diffusivity = cv::KAZE::DIFF_WEICKERT;
      else if (diffusivityName == "DIFF_CHARBONNIER")
         diffusivity = cv::KAZE::DIFF_CHARBONNIER;
      else
         diffusivity = cv::KAZE::DIFF_PM_G2;
      detector = cv::AKAZE::create(descriptor, 0, 3, threshold, octaves, octaveLayers, diffusivity);
      detector_info.set("AKAZE", { { "descriptor_type", std::to_string(descriptor) }, { "descriptor_size", "0" },
                                   { "descriptor_channels",  "3"}, { "threshold",  std::to_string(threshold) },
                                   { "nOctaves", std::to_string(octaves) },
                                   { "nOctaveLayers", std::to_string(octaveLayers) },
                                   { "diffusivity", std::to_string(diffusivity) } });
   }
   if (detectorName == "BRISK")
   {
      int threshold, octaves;
      float scale;
      if (! valInt(editBriskThreshold, threshold, "BRISK invalid threshold")) return false;
      if (! valInt(editBriskOctaves, octaves, "BRISK invalid octaves")) return false;
      if (! valFloat(editBriskScale, scale, "BRISK invalid scale")) return false;
      detector = cv::BRISK::create(threshold, octaves, scale);
      detector_info.set("BRISK", { { "thresh", std::to_string(threshold) }, { "octaves", std::to_string(octaves) },
                                   { "patternScale",  std::to_string(scale)} });
   }
   return (! detector.empty());
}

bool ImageWindow::load(std::string imagepath, bool isColor)
//--------------------------------------------------------
{
   cv::Mat img = cv::imread(imagepath, (isColor) ? cv::IMREAD_COLOR : cv::IMREAD_GRAYSCALE);
   if (img.empty())
   {
      std::cerr << "Error reading image from " << imagepath << std::endl;
      return false;
   }
   img.copyTo(pre_detect_image);
   image_holder->set_image(img);
   return true;
}
