/***************************************************************************
                        krender.cpp  -  description
                           -------------------
  begin                : Fri Nov 22 2002
  copyright            : (C) 2002 by Jason Wood
  email                : jasonwood@blueyonder.co.uk
  copyright            : (C) 2005 Lcio Fl�io Corr�
  email                : lucio.correa@gmail.com
  copyright            : (C) Marco Gittler
  email                : g.marco@freenet.de

***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kthumb.h"
#include "clipmanager.h"
#include "renderer.h"
#include "kdenlivesettings.h"

#include <mlt++/Mlt.h>

#include <kio/netaccess.h>
#include <kdebug.h>
#include <klocale.h>
#include <kfileitem.h>
#include <kmessagebox.h>
#include <KStandardDirs>

#include <qxml.h>
#include <QImage>
#include <QApplication>
#include <QtConcurrentRun>
#include <QVarLengthArray>
#include <QPainter>
#include <QGLWidget>

static QMutex m_intraMutex;

KThumb::KThumb(ClipManager *clipManager, const KUrl &url, const QString &id, const QString &hash, QObject * parent) :
    QObject(parent),
    m_url(url),
    m_thumbFile(),
    m_dar(1),
    m_ratio(1),
    m_producer(NULL),
    m_clipManager(clipManager),
    m_id(id)
{
    m_thumbFile = clipManager->projectFolder() + "/thumbs/" + hash + ".thumb";
}

KThumb::~KThumb()
{
    if (m_producer) m_clipManager->stopThumbs(m_id);
    m_producer = NULL;
    m_intraFramesQueue.clear();
    m_intra.waitForFinished();
}

void KThumb::setProducer(Mlt::Producer *producer)
{
    if (m_producer) m_clipManager->stopThumbs(m_id);
    m_intraFramesQueue.clear();
    m_intra.waitForFinished();
    QMutexLocker lock(&m_mutex);
    m_producer = producer;
    // FIXME: the profile() call leaks an object, but trying to free
    // it leads to a double-free in Profile::~Profile()
    if (producer) {
        Mlt::Profile *profile = producer->profile();
        m_dar = profile->dar();
        m_ratio = (double) profile->width() / profile->height();
    }
}

void KThumb::clearProducer()
{
    if (m_producer) setProducer(NULL);
}

bool KThumb::hasProducer() const
{
    return m_producer != NULL;
}

void KThumb::updateThumbUrl(const QString &hash)
{
    m_thumbFile = m_clipManager->projectFolder() + "/thumbs/" + hash + ".thumb";
}

void KThumb::updateClipUrl(const KUrl &url, const QString &hash)
{
    m_url = url;
    m_thumbFile = m_clipManager->projectFolder() + "/thumbs/" + hash + ".thumb";
}

//static
QPixmap KThumb::getImage(const KUrl& url, int width, int height)
{
    if (url.isEmpty()) return QPixmap();
    return getImage(url, 0, width, height);
}

void KThumb::extractImage(const QList<int> &frames)
{
    if (!KdenliveSettings::videothumbnails() || m_producer == NULL) return;
    m_clipManager->slotRequestThumbs(m_id, frames);
}


void KThumb::getThumb(int frame)
{
    const int theight = Kdenlive::DefaultThumbHeight;
    const int swidth = (int)(theight * m_ratio + 0.5);
    const int dwidth = (int)(theight * m_dar + 0.5);
    QImage img = getProducerFrame(frame, swidth, dwidth, theight);
    emit thumbReady(frame, img);
}

void KThumb::getGenericThumb(int frame, int height, int type)
{
    const int swidth = (int)(height * m_ratio + 0.5);
    const int dwidth = (int)(height * m_dar + 0.5);
    QImage img = getProducerFrame(frame, swidth, dwidth, height);
    m_clipManager->projectTreeThumbReady(m_id, frame, img, type);
}

QImage KThumb::extractImage(int frame, int width, int height)
{
    if (m_producer == NULL) {
        QImage img(width, height, QImage::Format_ARGB32_Premultiplied);
        img.fill(QColor(Qt::black).rgb());
        return img;
    }
    return getProducerFrame(frame, (int) (height * m_ratio + 0.5), width, height);
}

//static
QPixmap KThumb::getImage(const KUrl& url, int frame, int width, int height)
{
    Mlt::Profile profile(KdenliveSettings::current_profile().toUtf8().constData());
    QPixmap pix(width, height);
    if (url.isEmpty()) return pix;

    Mlt::Producer *producer = new Mlt::Producer(profile, url.path().toUtf8().constData());
    double swidth = (double) profile.width() / profile.height();
    pix = QPixmap::fromImage(getFrame(producer, frame, (int) (height * swidth + 0.5), width, height));
    delete producer;
    return pix;
}


QImage KThumb::getProducerFrame(int framepos, int frameWidth, int displayWidth, int height)
{
    if (m_producer == NULL || !m_producer->is_valid()) {
        QImage p(displayWidth, height, QImage::Format_ARGB32_Premultiplied);
        p.fill(QColor(Qt::red).rgb());
        return p;
    }
    if (m_producer->is_blank()) {
        QImage p(displayWidth, height, QImage::Format_ARGB32_Premultiplied);
        p.fill(QColor(Qt::black).rgb());
        return p;
    }
    QMutexLocker lock(&m_mutex);
    m_producer->seek(framepos);
    Mlt::Frame *frame = m_producer->get_frame();
    /*frame->set("rescale.interp", "nearest");
    frame->set("deinterlace_method", "onefield");
    frame->set("top_field_first", -1 );*/
    QImage p = getFrame(frame, frameWidth, displayWidth, height);
    delete frame;
    return p;
}

//static
QImage KThumb::getFrame(Mlt::Producer *producer, int framepos, int frameWidth, int displayWidth, int height)
{
    if (producer == NULL || !producer->is_valid()) {
        QImage p(displayWidth, height, QImage::Format_ARGB32_Premultiplied);
        p.fill(QColor(Qt::red).rgb());
        return p;
    }
    if (producer->is_blank()) {
        QImage p(displayWidth, height, QImage::Format_ARGB32_Premultiplied);
        p.fill(QColor(Qt::black).rgb());
        return p;
    }

    producer->seek(framepos);
    Mlt::Frame *frame = producer->get_frame();
    /*frame->set("rescale.interp", "nearest");
    frame->set("deinterlace_method", "onefield");
    frame->set("top_field_first", -1 );*/
    const QImage p = getFrame(frame, frameWidth, displayWidth, height);
    delete frame;
    return p;
}


//static
QImage KThumb::getFrame(Mlt::Frame *frame, int frameWidth, int displayWidth, int height)
{
    QImage p(displayWidth, height, QImage::Format_ARGB32_Premultiplied);
    if (frame == NULL || !frame->is_valid()) {
        p.fill(QColor(Qt::red).rgb());
        return p;
    }

    int ow = displayWidth;//frameWidth;
    int oh = height;
    mlt_image_format format = mlt_image_rgb24a;
    //frame->set("progressive", "1");
    if (ow % 2 == 1) ow++;
    QImage image(ow, oh, QImage::Format_ARGB32_Premultiplied);
    const uchar* imagedata = frame->get_image(format, ow, oh);
    if (imagedata == NULL) {
        p.fill(QColor(Qt::red).rgb());
        return p;
    }
    memcpy(image.bits(), imagedata, ow * oh * 4);//.byteCount());

    //const uchar* imagedata = frame->get_image(format, ow, oh);
    //QImage image(imagedata, ow, oh, QImage::Format_ARGB32_Premultiplied);
    return image.rgbSwapped();
    if (!image.isNull()) {
        if (ow > (2 * displayWidth)) {
            // there was a scaling problem, do it manually
            image = image.scaled(displayWidth, height).rgbSwapped();
        } else {
            image = image.scaled(displayWidth, height, Qt::IgnoreAspectRatio).rgbSwapped();
        }
        p.fill(QColor(100, 100, 100, 70));
        QPainter painter(&p);
        painter.drawImage(p.rect(), image);
        painter.end();
    } else
        p.fill(QColor(Qt::red).rgb());
    return p;
}

//static
uint KThumb::imageVariance(const QImage &image )
{
    uint delta = 0;
    uint avg = 0;
    uint bytes = image.numBytes();
    uint STEPS = bytes/2;
    QVarLengthArray<uchar> pivot(STEPS);
    const uchar *bits=image.bits();
    // First pass: get pivots and taking average
    for( uint i=0; i<STEPS ; ++i ){
        pivot[i] = bits[2 * i];
        avg+=pivot.at(i);
    }
    if (STEPS)
        avg=avg/STEPS;
    // Second Step: calculate delta (average?)
    for (uint i=0; i<STEPS; ++i)
    {
        int curdelta=abs(int(avg - pivot.at(i)));
        delta+=curdelta;
    }
    if (STEPS)
        return delta/STEPS;
    else
        return 0;
}

/*
void KThumb::getImage(KUrl url, int frame, int width, int height)
{
    if (url.isEmpty()) return;
    QPixmap image(width, height);
    Mlt::Producer m_producer(url.path().toUtf8().constData());
    image.fill(Qt::black);

    if (m_producer.is_blank()) {
 emit thumbReady(frame, image);
 return;
    }
    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    m_producer.attach(m_convert);
    m_producer.seek(frame);
    Mlt::Frame * m_frame = m_producer.get_frame();
    mlt_image_format format = mlt_image_rgb24a;
    width = width - 2;
    height = height - 2;
    if (m_frame && m_frame->is_valid()) {
     uint8_t *thumb = m_frame->get_image(format, width, height);
     QImage tmpimage(thumb, width, height, 32, NULL, 0, QImage::IgnoreEndian);
     if (!tmpimage.isNull()) bitBlt(&image, 1, 1, &tmpimage, 0, 0, width + 2, height + 2);
    }
    if (m_frame) delete m_frame;
    emit thumbReady(frame, image);
}

void KThumb::getThumbs(KUrl url, int startframe, int endframe, int width, int height)
{
    if (url.isEmpty()) return;
    QPixmap image(width, height);
    Mlt::Producer m_producer(url.path().toUtf8().constData());
    image.fill(QColor(Qt::black).rgb());

    if (m_producer.is_blank()) {
 emit thumbReady(startframe, image);
 emit thumbReady(endframe, image);
 return;
    }
    Mlt::Filter m_convert("avcolour_space");
    m_convert.set("forced", mlt_image_rgb24a);
    m_producer.attach(m_convert);
    m_producer.seek(startframe);
    Mlt::Frame * m_frame = m_producer.get_frame();
    mlt_image_format format = mlt_image_rgb24a;
    width = width - 2;
    height = height - 2;

    if (m_frame && m_frame->is_valid()) {
     uint8_t *thumb = m_frame->get_image(format, width, height);
     QImage tmpimage(thumb, width, height, 32, NULL, 0, QImage::IgnoreEndian);
     if (!tmpimage.isNull()) bitBlt(&image, 1, 1, &tmpimage, 0, 0, width - 2, height - 2);
    }
    if (m_frame) delete m_frame;
    emit thumbReady(startframe, image);

    image.fill(Qt::black);
    m_producer.seek(endframe);
    m_frame = m_producer.get_frame();

    if (m_frame && m_frame->is_valid()) {
     uint8_t *thumb = m_frame->get_image(format, width, height);
     QImage tmpimage(thumb, width, height, 32, NULL, 0, QImage::IgnoreEndian);
     if (!tmpimage.isNull()) bitBlt(&image, 1, 1, &tmpimage, 0, 0, width - 2, height - 2);
    }
    if (m_frame) delete m_frame;
    emit thumbReady(endframe, image);
}
*/

void KThumb::slotCreateAudioThumbs()
{
    m_clipManager->askForAudioThumb(m_id);
}

void KThumb::queryIntraThumbs(const QSet <int> &missingFrames)
{
    m_intraMutex.lock();
    if (m_intraFramesQueue.length() > 20) {
        // trim query list
        int maxsize = m_intraFramesQueue.length() - 10;
        if (missingFrames.contains(m_intraFramesQueue.first())) {
            for( int i = 0; i < maxsize; i ++ )
                m_intraFramesQueue.removeLast();
        }
        else if (missingFrames.contains(m_intraFramesQueue.last())) {
            for( int i = 0; i < maxsize; i ++ )
                m_intraFramesQueue.removeFirst();
        }
        else m_intraFramesQueue.clear();
    }
    QSet<int> set = m_intraFramesQueue.toSet();
    set.unite(missingFrames);
    m_intraFramesQueue = set.toList();
    qSort(m_intraFramesQueue);
    m_intraMutex.unlock();
    if (!m_intra.isRunning()) {
        m_intra = QtConcurrent::run(this, &KThumb::slotGetIntraThumbs);
    }
}

void KThumb::slotGetIntraThumbs()
{
    // We are in a new thread, so we need a new OpenGL context for the remainder of the function.
    QGLWidget ctx(0, m_clipManager->getMainContext());
    kDebug()<<"// STARTING INTR THB FOR: "<<m_intraFramesQueue;
    const int theight = KdenliveSettings::trackheight();
    const int frameWidth = (int)(theight * m_ratio + 0.5);
    const int displayWidth = (int)(theight * m_dar + 0.5);
    QString path = m_url.path() + '_';
    bool addedThumbs = false;
    int pos = 0;
    while (true) {
        m_intraMutex.lock();
        if (m_intraFramesQueue.isEmpty()) {
            m_intraMutex.unlock();
            break;
        }
        pos = m_intraFramesQueue.takeFirst();
        m_intraMutex.unlock();
        const QString key = path + QString::number(pos);
        if (!m_clipManager->pixmapCache->contains(key)) {
            ctx.makeCurrent();
            QImage img = getProducerFrame(pos, frameWidth, displayWidth, theight);
            if (m_clipManager->pixmapCache->insertImage(key, img)) {
                addedThumbs = true;
            }
            else kDebug()<<"// INSERT FAILD FOR: "<<pos;
        }

    }
    if (addedThumbs) emit thumbsCached();
}

QImage KThumb::findCachedThumb(int pos)
{
    QImage img;
    const QString path = m_url.path() + '_' + QString::number(pos);
    m_clipManager->pixmapCache->findImage(path, &img);
    return img;
}

#include "kthumb.moc"

