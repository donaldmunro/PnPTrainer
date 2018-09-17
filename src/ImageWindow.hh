#ifndef _IMAGE_WINDOW_HH_
#define _IMAGE_WINDOW_HH_
#include <memory>
#include <functional>
#include <array>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <limits>
#include <exception>
#ifdef FILESYSTEM_EXPERIMENTAL
#include <experimental/filesystem>
namespace filesystem = std::experimental::filesystem;
#else
#include <filesystem>
namespace filesystem = std::filesystem;
#endif

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d/features2d.hpp>
#ifdef HAVE_OPENCV_XFEATURES2D
#include <opencv2/xfeatures2d.hpp>
#endif

#include <QApplication>
#include <QTimer>
#include <QMainWindow>
#include <QImage>
#include <QLabel>
#include <QAction>
#include <QMenu>
#include <QVBoxLayout>
#include <QWidget>
#include <QTabWidget>
#include <QPoint>
#include <QRect>
#include <QRadioButton>
#include <QCheckBox>
#include <QButtonGroup>
#include <QListWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>

#include "main.hh"
#include "MatchWin.h"
#include "CVQtScrollableImage.h"
#include "types.h"

//class MatchWin;

class ImageWindow : public QMainWindow
//=====================================
{
Q_OBJECT

public:
   ImageWindow(MatchWin* matchWindow, QWidget* parent = nullptr);
   virtual ~ImageWindow() {  }

   void setApplication(QApplication *a) { app = a; }
   bool load(std::string imagefile, bool isColor=true);
   const DetectorInfo& detector() const { return detector_info; };
   std::string save_dialog_name() { return save_dialog_result.toStdString(); }
   bool yes_no() { return yes_no_result; }

protected:
   virtual bool create_detector(std::string detectorName, cv::Ptr<cv::Feature2D>& detector);

public slots:
   void open();
   void exit() { stop(); std::exit(1); }
   void msgbox(QString s)
   {
      QMessageBox msgBox;
      msgBox.setText(s);
      msgBox.exec();
   }
   void yesnobox(QString title, QString msg)
   {
      QMessageBox::StandardButton reply;
      reply = QMessageBox::question(this, title, msg, QMessageBox::Yes | QMessageBox::No);
      yes_no_result =  (reply == QMessageBox::Yes);
   }
   void save_file_dialog(QString title, QString default_file, QString filter) //"Images (*.png *.xpm *.jpg);;Text files (*.txt);;XML files (*.xml)"
   {
      save_dialog_result = QFileDialog::getSaveFileName(this, title, default_file, filter);
      match_window->request_focus();
   }

private slots:
   void on_detect();

private:
   MatchWin* match_window;
   QTabWidget tabs;
   QVBoxLayout* panel_layout;
   QAction *open_img_action, *open_3d_action, *exit_action;
   QMenu* file_menu;
   QApplication *app;
   QString save_dialog_result;
   bool yes_no_result = false;

   QButtonGroup feature_detectors;
   QListWidget *listOrbWTA, *listOrbScore, *listAkazeDescriptors, *listAkazeDiffusivity;
   QLineEdit *editOrbNoFeatures, *editOrbScale, *editOrbPatchSize,
             *editSiftNoFeatures, *editSiftOctaveLayers, *editSiftContrast, *editSiftEdge, *editSiftSigma,
             *editSurfThreshold, *editSurfOctaves, *editSurfOctaveLayers,
             *editAkazeThreshold, *editAkazeOctaves, *editAkazeOctaveLayers,
             *editBriskThreshold, *editBriskOctaves, *editBriskScale, *editBest;
   QCheckBox *chkSurfExtended, *chkRichKps, *chkDelDups;
   QPushButton *detectButton, *selectedColorButton, *unselectedColorButton;
   QColor selected_colour{255, 0, 0}, unselected_colour{0, 255, 0};
   const cv::Scalar red = cv::Scalar(0, 0, 255), green = cv::Scalar(0, 255, 0), yellow = cv::Scalar(0, 255, 255);
   CVQtScrollableImage* image_holder;

   cv::Mat pre_detect_image;
   std::vector<cv::KeyPoint> current_keypts;
   cv::Mat current_descriptors;
   std::vector<features_t> region_features;
   DetectorInfo detector_info;

   void on_region_selected(cv::Mat& roi, cv::Rect& roirect, bool is_shift, bool is_ctrl, bool is_alt);

   void setup_features_control(QLayout *layout);

   void load_image_menu() {}
   void load3d_menu() {}
};
#endif // MAINWINDOW_HH
