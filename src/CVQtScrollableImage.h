#ifndef _CVQTSCROLLABLEIMAGE_H_
#define _CVQTSCROLLABLEIMAGE_H_

#include <memory>

#include <opencv2/core/core.hpp>
#include <opencv2/imgproc/imgproc.hpp>

#include <QWidget>
#include <QLabel>
#include <QScrollArea>
#include <QtWidgets/QRubberBand>

class CVQtScrollableImage : public QWidget
//====================================
{
   Q_OBJECT
//   Q_PROPERTY(cv::Mat image READ get_image WRITE set_image)

public:
   explicit CVQtScrollableImage(QWidget* parent = nullptr);

   ~CVQtScrollableImage() override
   //-----------------------------
   {
      scrollArea.setWidget(nullptr);
      if (! image.empty())
         image.release();
   }

   void set_image(cv::Mat& img);
   const cv::Mat& get_image() const { return image; }
   void set_roi_callback(std::function<void(cv::Mat&, cv::Rect&, bool, bool, bool)>& callback) { on_roi_selection = callback; }

   QSize sizeHint() const override { return ((! image.empty()) ? QSize(image.cols, image.rows) : QSize(1024, 768)); }

protected:
   void mousePressEvent(QMouseEvent *event) override;
   void mouseMoveEvent(QMouseEvent *event) override;
   void mouseReleaseEvent(QMouseEvent *event) override;
   void resizeEvent(QResizeEvent *event) override;
//   void  keyPressEvent(QKeyEvent *event) override;

private:
   QLabel imageLabel;
   QScrollArea scrollArea;
   cv::Mat image;
   QPoint drag_start;
   QRect drag_rect;
   std::unique_ptr<QRubberBand> drag_show;
   std::function<void(cv::Mat&, cv::Rect&, bool, bool, bool)> on_roi_selection;
};


#endif //PNPTRAINER_CVQTSCROLLABLEIMAGE_H
