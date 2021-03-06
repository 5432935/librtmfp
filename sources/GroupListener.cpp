/*
Copyright 2016 Thomas Jammet
mathieu.poux[a]gmail.com
jammetthomas[a]gmail.com

This file is part of Librtmfp.

Librtmfp is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Librtmfp is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Librtmfp.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "GroupListener.h"
#include "Publisher.h"
#include "RTMFP.h"
#include "Mona/Logs.h"

using namespace std;
using namespace Mona;

GroupListener::GroupListener(Publisher& publication, const string& identifier) : Listener(publication, identifier), _firstTime(true),
_seekTime(0), _dataInitialized(false), _reliable(true), _startTime(0), _lastTime(0), _codecInfosSent(false) {

}

GroupListener::~GroupListener() {
	
}

void GroupListener::startPublishing() {

}

void GroupListener::stopPublishing() {

	if (firstTime())
		return;

	_seekTime = _lastTime;
	_firstTime = true;
	_codecInfosSent = false;
	_startTime = 0;
}


void GroupListener::pushVideo(UInt32 time, const UInt8* data, UInt32 size) {

	if (!_codecInfosSent) {
		if (!pushVideoInfos(time, data, size)) {
			DEBUG("Video frame dropped to wait first key frame");
			return;
		}
	}
	// Send codec infos periodically (ffmpeg issue)
	else if (_lastCodecsTime.isElapsed(5000) && pushVideoInfos(time, data, size)) {
		// TODO: make the time configurable
		_lastCodecsTime.update();
	}

	if (_firstTime) {
		_startTime = time;
		_firstTime = false;

		// for audio sync (audio is usually the reference track)
		if (pushAudioInfos(time))
			pushAudio(time, NULL, 0); // push a empty audio packet to avoid a video which waits audio tracks!
	}
	time -= _startTime;
	//TRACE("Video time(+seekTime) => ", time, "(+", _seekTime, "), size : ", size);

	OnMedia::raise(RTMFP::IsKeyFrame(data, size) || _reliable, AMF::VIDEO, _lastTime = (time + _seekTime), data, size);
}

bool GroupListener::pushVideoInfos(UInt32 time, const UInt8* data, UInt32 size) {
	if (RTMFP::IsKeyFrame(data, size)) {
		_codecInfosSent = true;
		if (!publication.videoCodecBuffer().empty() && !RTMFP::IsH264CodecInfos(data, size)) {
			INFO("H264 codec infos sent to one listener of ", publication.name(), " publication")
			pushVideo(time, publication.videoCodecBuffer()->data(), publication.videoCodecBuffer()->size());
		}
		return true;
	}
	return false;
}


void GroupListener::pushAudio(UInt32 time, const UInt8* data, UInt32 size) {

	if (_firstTime) {
		_firstTime = false;
		_startTime = time;
		pushAudioInfos(time);
	}
	time -= _startTime;
	//TRACE("Audio time(+seekTime) => ", time, "(+", _seekTime, ")");

	OnMedia::raise(RTMFP::IsAACCodecInfos(data, size) || _reliable, AMF::AUDIO, _lastTime = (time + _seekTime), data, size);
}

bool GroupListener::pushAudioInfos(UInt32 time) {
	if (publication.audioCodecBuffer().empty())
		return false;
	INFO("AAC codec infos sent to one listener of ", publication.name(), " publication")
	pushAudio(time, publication.audioCodecBuffer()->data(), publication.audioCodecBuffer()->size());
	return true;
}

void GroupListener::flush() {

}
