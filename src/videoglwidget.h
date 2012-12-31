
#ifndef VIDEOGLWIDGET_H
#define VIDEOGLWIDGET_H

#include <QGLWidget>

class VideoGLWidget : public QGLWidget
{
    Q_OBJECT

public:
    VideoGLWidget(QWidget *parent = 0);
    ~VideoGLWidget();
    void activateMonitor();
    QSize minimumSizeHint() const;
    QSize sizeHint() const;
    void setImageAspectRatio(double ratio) {
        m_display_ratio = ratio;
    }
    void setBackgroundColor(QColor color) {
        m_backgroundColor = color;
    }

private:
    int x, y, w, h;
    int m_image_width, m_image_height;
    GLuint m_texture;
    double m_display_ratio;
    QColor m_backgroundColor;
    Qt::WindowFlags m_baseFlags;

public slots:
    void showImage(QImage image);

protected:
    void initializeGL();
    void resizeGL(int width, int height);
    void resizeEvent(QResizeEvent* event);
    void paintGL();
    void mouseDoubleClickEvent(QMouseEvent * event);
};

#endif
