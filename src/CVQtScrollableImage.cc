#include <iostream>

#include "CVQtScrollableImage.h"

#include <QScrollBar>
#include <QMouseEvent>

CVQtScrollableImage::CVQtScrollableImage(QWidget *parent) : QWidget(parent), imageLabel(this), scrollArea(this)
//-----------------------------------------------------------------------------------------------------
{
//   setAttribute(Qt::WA_StaticContents);
   setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

   QImage qimg(1024, 768, QImage::Format_RGB888);
   qimg.fill(qRgba(0, 0, 0, 0));
   imageLabel.setBackgroundRole(QPalette::Base);
//   imageLabel.setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
   QPixmap pixels = QPixmap::fromImage(qimg);
   imageLabel.setPixmap(pixels);
   imageLabel.adjustSize();
   //imageLabel.setScaledContents(true);

   scrollArea.setBackgroundRole(QPalette::Dark);
   scrollArea.setWidget(&imageLabel);
   scrollArea.setWidgetResizable(true);
   scrollArea.setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
//   scrollArea->setVisible(false);
   //  setCentralWidget(scrollArea.get());
}


void CVQtScrollableImage::set_image(cv::Mat& img)
//----------------------------------------------
{
   image = img;
   if (img.empty())
      return;
   cv::cvtColor(image, image, CV_BGR2RGB);
   QImage qimg(image.data, image.cols, image.rows, QImage::Format_RGB888);
   QSize sze = this->parentWidget()->size();
   QPixmap pixels = QPixmap::fromImage(qimg);
   imageLabel.setPixmap(pixels);
   imageLabel.adjustSize();
   scrollArea.setVisible(true);
   scrollArea.setFixedSize(sze);
   scrollArea.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
   scrollArea.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
   updateGeometry();
   std::cout << scrollArea.size().width() << "x" << scrollArea.size().height() << " "
             << imageLabel.size().width() << "x" << imageLabel.size().height() << " "
             << pixels.size().width() << "x" << pixels.size().height() << " "
             << image.cols << "x" << image.rows << std::endl;
}

void CVQtScrollableImage::mousePressEvent(QMouseEvent *event)
//-------------------------------------------------------
{
   QWidget::mousePressEvent(event);
   drag_start = event->pos();
   drag_show.reset(new QRubberBand(QRubberBand::Rectangle, this));
   drag_show->setGeometry(QRect(drag_start, QSize()));
   drag_show->show();
}

void CVQtScrollableImage::mouseMoveEvent(QMouseEvent *event)
//------------------------------------------------------
{
   QWidget::mouseMoveEvent(event);
   drag_rect = QRect(drag_start, event->pos()).normalized();
   drag_show->setGeometry(drag_rect);
}

void CVQtScrollableImage::mouseReleaseEvent(QMouseEvent *event)
//---------------------------------------------------------
{
   QWidget::mouseReleaseEvent(event);
   drag_show->hide();
   drag_show.reset(nullptr);
   QScrollBar* hbar = scrollArea.horizontalScrollBar();
   int x = hbar->value();
   QScrollBar* vbar = scrollArea.verticalScrollBar();
   int y = vbar->value();
   if ( (on_roi_selection) && (drag_rect.width() > 2) && (drag_rect.height() > 2) )
   {
      cv::Rect roi;
      roi.x = x + drag_rect.left();
      roi.y = y + drag_rect.top();
      roi.width = drag_rect.width();
      roi.height = drag_rect.height();
      cv::Mat m(image, roi);
      bool is_shift = static_cast<bool>(event->modifiers() & Qt::ShiftModifier);
      bool is_ctrl  = static_cast<bool>(event->modifiers() & Qt::ControlModifier);
      bool is_alt  = static_cast<bool>(event->modifiers() & Qt::AltModifier);
      on_roi_selection(m, roi, is_shift, is_ctrl, is_alt);
   }
}

void CVQtScrollableImage::resizeEvent(QResizeEvent *event)
//-----------------------------------------------------
{
   QWidget::resizeEvent(event);
   QSize sze = this->parentWidget()->size();
   scrollArea.setFixedSize(sze);
}