/*
Copyright (C) 2012  Simon A. Eugster (Granjow)  <simon.eu@gmail.com>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "audioStreamInfo.h"

#include <QString>
#include <iostream>
#include <cstdlib>

AudioStreamInfo::AudioStreamInfo(Mlt::Producer *producer, int audioStreamIndex) :
    m_audioStreamIndex(audioStreamIndex)
{

    QByteArray key;

    key = QString("meta.media.%1.codec.sample_fmt").arg(audioStreamIndex).toLocal8Bit();
    m_samplingFormat = QString(producer->get(key.data()));

    key = QString("meta.media.%1.codec.sample_rate").arg(audioStreamIndex).toLocal8Bit();
    m_samplingRate = atoi(producer->get(key.data()));

    key = QString("meta.media.%1.codec.bit_rate").arg(audioStreamIndex).toLocal8Bit();
    m_bitRate = atoi(producer->get(key.data()));

    key = QString("meta.media.%1.codec.channels").arg(audioStreamIndex).toLocal8Bit();
    m_channels = atoi(producer->get(key.data()));

    key = QString("meta.media.%1.codec.name").arg(audioStreamIndex).toLocal8Bit();
    m_codecName = QString(producer->get(key.data()));

    key = QString("meta.media.%1.codec.long_name").arg(audioStreamIndex).toLocal8Bit();
    m_codecLongName = QString(producer->get(key.data()));
}
AudioStreamInfo::~AudioStreamInfo()
{
}

int AudioStreamInfo::streamIndex() const { return m_audioStreamIndex; }
int AudioStreamInfo::samplingRate() const { return m_samplingRate; }
int AudioStreamInfo::channels() const { return m_channels; }
int AudioStreamInfo::bitrate() const { return m_bitRate; }
const QString& AudioStreamInfo::codecName(bool longName) const
{
    if (longName) {
        return m_codecLongName;
    } else {
        return m_codecName;
    }
}

void AudioStreamInfo::dumpInfo() const
{
    std::cout << "Info for audio stream " << m_audioStreamIndex << std::endl
              << "\tCodec: " << m_codecLongName.toLocal8Bit().data() << " (" << m_codecName.toLocal8Bit().data() << ")" << std::endl
              << "\tChannels: " << m_channels << std::endl
              << "\tSampling rate: " << m_samplingRate << std::endl
              << "\tBit rate: " << m_bitRate << std::endl
                 ;

}
