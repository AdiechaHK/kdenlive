/***************************************************************************
                          timecode  -  description
                             -------------------
    begin                : Wed Dec 17 2003
    copyright            : (C) 2003 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "timecode.h"

#include <kdebug.h>
#include <klocale.h>

Timecode::Timecode(Formats format, int framesPerSecond,
    bool dropFrame):m_format(format), m_dropFrame(dropFrame),
m_displayedFramesPerSecond(framesPerSecond)
{
}

Timecode::~Timecode()
{
}

int Timecode::getFrameNumber(const QString duration, double fps) const
{
    if (m_dropFrame) {
	// calculate how many frames need to be dropped every minute.
	int frames;
    	int toDrop = (int) floor (600.0 * (m_displayedFramesPerSecond - fps)  + 0.5);

    	int perMinute = toDrop / 9;
    	int tenthMinute = toDrop % 9;

    	// calculate how many frames are in a normal minute, and how many are in a tenth minute.
    	int normalMinuteFrames = (m_displayedFramesPerSecond * 60) - perMinute;
    	int tenthMinuteFrames = (m_displayedFramesPerSecond * 60) - tenthMinute;;

    	// Number of actual frames in a 10 minute interval :
    	int tenMinutes = (normalMinuteFrames * 9) + tenthMinuteFrames;
	frames = 6 * duration.section(":",0,0).toInt() * tenMinutes;
	int minutes = duration.section(":",1,1).toInt();
	frames += ((int) minutes / 10) * tenMinutes;
	int mins = minutes % 10;
	if (mins > 0) {
	    frames += tenthMinuteFrames;
	    mins--;
	    if (mins > 0) frames += mins * normalMinuteFrames;
	}
	if (minutes % 10 > 0) frames -= perMinute;
	frames += duration.section(":",2,2).toInt() * m_displayedFramesPerSecond + duration.section(":",3,3).toInt();
	return frames;	
    }
    return (int) ((duration.section(":",0,0).toInt()*3600.0 + duration.section(":",1,1).toInt()*60.0 + duration.section(":",2,2).toInt()) * fps + duration.section(":",3,3).toInt());
}

QString Timecode::getTimecode(const GenTime & time, double fps) const
{
    switch (m_format) {
    case HH_MM_SS_FF:
	return getTimecodeHH_MM_SS_FF(time, fps);
	break;
    case HH_MM_SS_HH:
	return getTimecodeHH_MM_SS_HH(time);
	break;
    case Frames:
	return getTimecodeFrames(time, fps);
	break;
    case Seconds:
	return getTimecodeSeconds(time);
	break;
    default:
	kdWarning() <<
	    "Unknown timecode format specified, defaulting to HH_MM_SS_FF"
	    << endl;
	return getTimecodeHH_MM_SS_FF(time, fps);
    }
}

QString Timecode::getTimecodeFromFrames(int frames)
{
    return getTimecodeHH_MM_SS_FF(frames);
}

//static 
QString Timecode::getEasyTimecode(const GenTime & time, const double &fps)
{
    // Returns the timecode in an easily read display, like 3 min. 5 sec.
    int frames = (int)time.frames(fps);
    int seconds = frames / (int) floor(fps + 0.5);
    frames = frames % ((int) fps);

    int minutes = seconds / 60;
    seconds = seconds % 60;
    int hours = minutes / 60;
    minutes = minutes % 60;

    QString text;
    bool trim = false;

    if (hours!= 0) {
        text.append(QString::number(hours).rightJustify(2, '0', FALSE));
        text.append(" " + i18n("hour") + " ");
        trim = true;
    }
    if (minutes!= 0 || trim) {
        if (!trim) {
            text.append(QString::number(minutes));
        }
        else
            text.append(QString::number(minutes).rightJustify(2, '0', FALSE));
        text.append(" " + i18n("min.") + " ");
        trim = true;
    }
    if (seconds!= 0 || trim) {
        if (!trim) {
            text.append(QString::number(seconds));
        }
        else 
            text.append(QString::number(seconds).rightJustify(2, '0', FALSE));
        text.append(" " + i18n("sec."));
        trim = true;
    }
    if (!trim) {
            text.append(QString::number(frames));
            text.append(" " + i18n("frames"));
    }

    return text;
}


QString Timecode::getTimecodeHH_MM_SS_FF(const GenTime & time, double fps) const
{
    if (m_dropFrame)
	return getTimecodeDropFrame(time, fps);

    return getTimecodeHH_MM_SS_FF((int)time.frames(fps));
}

QString Timecode::getTimecodeHH_MM_SS_FF(int frames) const
{
    int seconds = frames / m_displayedFramesPerSecond;
    frames = frames % m_displayedFramesPerSecond;

    int minutes = seconds / 60;
    seconds = seconds % 60;
    int hours = minutes / 60;
    minutes = minutes % 60;

    QString text;

    text.append(QString::number(hours).rightJustify(2, '0', FALSE));
    text.append(":");
    text.append(QString::number(minutes).rightJustify(2, '0', FALSE));
    text.append(":");
    text.append(QString::number(seconds).rightJustify(2, '0', FALSE));
    text.append(":");
    text.append(QString::number(frames).rightJustify(2, '0', FALSE));

    return text;
}

QString Timecode::getTimecodeHH_MM_SS_HH(const GenTime & time) const
{
    int hundredths = (int)(time.seconds() * 100);
    int seconds = hundredths / 100;
    hundredths = hundredths % 100;
    int minutes = seconds / 60;
    seconds = seconds % 60;
    int hours = minutes / 60;
    minutes = minutes % 60;

    QString text;

    text.append(QString::number(hours).rightJustify(2, '0', FALSE));
    text.append(":");
    text.append(QString::number(minutes).rightJustify(2, '0', FALSE));
    text.append(":");
    text.append(QString::number(seconds).rightJustify(2, '0', FALSE));
    text.append(":");
    text.append(QString::number(hundredths).rightJustify(2, '0', FALSE));

    return text;
}

QString Timecode::getTimecodeFrames(const GenTime & time, double fps) const
{
    return QString::number(time.frames(fps));
}

QString Timecode::getTimecodeSeconds(const GenTime & time) const
{
    return QString::number(time.seconds());
}

QString Timecode::getTimecodeDropFrame(const GenTime & time, double fps) const
{
    // Calculate the timecode using dropframes to remove the difference in fps. Note that this algorithm should work
    // for NTSC times, but is untested for any others - it is in no way an "official" algorithm, unless it's by fluke.
    int frames = (int)time.frames(fps);

    // calculate how many frames need to be dropped every minute.
    int toDrop = (int) floor (600.0 * (m_displayedFramesPerSecond - fps)  + 0.5);

    int perMinute = toDrop / 9;
    int tenthMinute = toDrop % 9;

    // calculate how many frames are in a normal minute, and how many are in a tenth minute.
    int normalMinuteFrames = (m_displayedFramesPerSecond * 60) - perMinute;
    int tenthMinuteFrames = (m_displayedFramesPerSecond * 60) - tenthMinute;;

    // Number of actual frames in a 10 minute interval :
    int tenMinutes = (normalMinuteFrames * 9) + tenthMinuteFrames;

    int tenMinuteIntervals = frames / tenMinutes;
    frames = frames % tenMinutes;

    int hours = tenMinuteIntervals / 6;
    tenMinuteIntervals = tenMinuteIntervals % 6;

    // At the point, we have figured out HH:M?:??:??

    int numMinutes;

    if (frames < tenthMinuteFrames) {
	// tenth minute logic applies.
	numMinutes = 0;
    } else {
	// normal minute logic applies.
	numMinutes = 1 + (frames - tenthMinuteFrames) / normalMinuteFrames;
	frames = (frames - tenthMinuteFrames) % normalMinuteFrames;
	frames +=  tenthMinute + perMinute;
    }
    // We now have HH:MM:??:??

    int seconds = frames / m_displayedFramesPerSecond;
    frames = frames % m_displayedFramesPerSecond;

    // We now have HH:MM:SS:FF

    // THANK FUCK FOR THAT.

    QString text;
    text.append(QString::number(hours).rightJustify(2, '0', FALSE));
    text.append(":");
    text.append(QString::number(tenMinuteIntervals));
    text.append(QString::number(numMinutes));
    text.append(":");
    text.append(QString::number(seconds).rightJustify(2, '0', FALSE));
    text.append(":");
    text.append(QString::number(frames).rightJustify(2, '0', FALSE));

    return text;
}
