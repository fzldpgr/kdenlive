/**************************1*************************************************
                          DocClipRef.cpp  -  description
                             -------------------
    begin                : Fri Apr 12 2002
    copyright            : (C) 2002 by Jason Wood
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

#include <kdebug.h>

#include "clipmanager.h"
#include "docclipbase.h"
#include "docclipavfile.h"
#include "docclipproject.h"
#include "doctrackbase.h"
#include "docclipbase.h"

DocClipRef::DocClipRef(DocClipBase *clip) :
	m_trackStart(0.0),
	m_cropStart(0.0),
	m_trackEnd(0.0),
	m_parentTrack(0),
	m_trackNum(-1),
	m_clip(clip)
{
	if(!clip) {
		kdError() << "Creating a DocClipRef with no clip - not a very clever thing to do!!!" << endl;
	}
}

DocClipRef::~DocClipRef()
{
}

const GenTime &DocClipRef::trackStart() const {
  return m_trackStart;
}

void DocClipRef::setTrackStart(const GenTime time)
{
	m_trackStart = time;

	if(m_parentTrack) {
		m_parentTrack->clipMoved(this);
	}
}

const QString &DocClipRef::name() const
{
	return m_clip->name();
}

const QString &DocClipRef::description() const
{
	return m_clip->description();
}

void DocClipRef::setCropStartTime(const GenTime &time)
{	
	m_cropStart = time;
	if(m_parentTrack) {
		m_parentTrack->clipMoved(this);
	}	
}

const GenTime &DocClipRef::cropStartTime() const
{
	return m_cropStart;	
}

void DocClipRef::setTrackEnd(const GenTime &time)
{
	m_trackEnd = time;
  
	if(m_parentTrack) {    
		m_parentTrack->clipMoved(this);
	}	
}

GenTime DocClipRef::cropDuration() const
{
	return m_trackEnd - m_trackStart;
}

DocClipRef *DocClipRef::createClip(ClipManager &clipManager, const QDomElement &element)
{
	DocClipRef *clip = 0;
	GenTime trackStart;
	GenTime cropStart;
	GenTime cropDuration;
	GenTime trackEnd;
	QString description;
	int trackNum = 0;

	QDomNode node = element;
	node.normalize();

	if(element.tagName() != "clip") {
		kdWarning()	<< "DocClipRef::createClip() element has unknown tagName : " << element.tagName() << endl;
		return 0;
	}

	DocClipBase *baseClip = clipManager.findClip(element);

	if(baseClip) {
		clip = new DocClipRef(baseClip);
	}

	QDomNode n = element.firstChild();

	while(!n.isNull()) {
		QDomElement e = n.toElement();
		if(!e.isNull()) {
			/**if(e.tagName() == "avfile") {
				clip = DocClipAVFile::createClip(e);
			} else if(e.tagName() == "project") {
				clip = DocClipProject::createClip(e);
			} else */
			if(e.tagName() == "position") {
				trackNum = e.attribute("track", "-1").toInt();
				trackStart = GenTime(e.attribute("trackstart", "0").toDouble());
				cropStart = GenTime(e.attribute("cropstart", "0").toDouble());
				cropDuration = GenTime(e.attribute("cropduration", "0").toDouble());
				trackEnd = GenTime(e.attribute("trackend", "-1").toDouble());
			}		
		}/* else {
			QDomText text = n.toText();
			if(!text.isNull()) {
				description = text.nodeValue();
			}
		}*/
		
		n = n.nextSibling();
	}

	if(clip==0) {
	  kdWarning()	<< "DocClipRef::createClip() unable to create clip" << endl;
	} else {
		// setup DocClipRef specifics of the clip.
		clip->setTrackStart(trackStart);
		clip->setCropStartTime(cropStart);
		if(trackEnd.seconds() != -1) {
			clip->setTrackEnd(trackEnd);
		} else {
			clip->setTrackEnd(trackStart + cropDuration);
		}
		clip->setParentTrack(0, trackNum);
		//clip->setDescription(description);
	}

	return clip;
}

/** Sets the parent track for this clip. */
void DocClipRef::setParentTrack(DocTrackBase *track, const int trackNum)
{
	m_parentTrack = track;
	m_trackNum = trackNum;
}

/** Returns the track number. This is a hint as to which track the clip is on, or should be placed on. */
int DocClipRef::trackNum() const
{
	return m_trackNum;
}

/** Returns the end of the clip on the track. A convenience function, equivalent
to trackStart() + cropDuration() */
GenTime DocClipRef::trackEnd() const
{
	return m_trackEnd;
}

/** Returns the parentTrack of this clip. */
DocTrackBase * DocClipRef::parentTrack()
{
	return m_parentTrack;
}

/** Move the clips so that it's trackStart coincides with the time specified. */
void DocClipRef::moveTrackStart(const GenTime &time)
{	
	m_trackEnd = m_trackEnd + time - m_trackStart;
	m_trackStart = time;
}

DocClipRef *DocClipRef::clone(ClipManager &clipManager)
{
	return createClip(clipManager, toXML().documentElement());
}


/*DocClipBase *DocClipRef::createClip(const QDomElement element)
{
	DocClipBase *clip = 0;
	GenTime trackStart;
	GenTime cropStart;
	GenTime cropDuration;
	GenTime trackEnd;
	QString description;
	int trackNum = 0;

	QDomNode node = element;
	node.normalize();
	
	if(element.tagName() != "clip") {
		kdWarning()	<< "DocClipBase::createClip() element has unknown tagName : " << element.tagName() << endl;
		return 0;
	}	

	QDomNode n = element.firstChild();

	while(!n.isNull()) {
		QDomElement e = n.toElement();
		if(!e.isNull()) {
			if(e.tagName() == "avfile") {
				clip = DocClipAVFile::createClip(doc, e);
			} else if(e.tagName() == "project") {
				clip = DocClipProject::createClip(doc, e);
			} else if(e.tagName() == "position") {
				trackNum = e.attribute("track", "-1").toInt();
				trackStart = GenTime(e.attribute("trackstart", "0").toDouble());
				cropStart = GenTime(e.attribute("cropstart", "0").toDouble());
				cropDuration = GenTime(e.attribute("cropduration", "0").toDouble());
				trackEnd = GenTime(e.attribute("trackend", "-1").toDouble());
			}		
		} else {
			QDomText text = n.toText();
			if(!text.isNull()) {
				description = text.nodeValue();
			}
		}
		
		n = n.nextSibling();
	}

	if(clip==0) {
		kdWarning() << "DocClipBase::createClip() unable to create clip" << endl;
	} else {
		// setup DocClipBase specifics of the clip.
		clip->setTrackStart(trackStart);
		clip->setCropStartTime(cropStart);
		if(trackEnd.seconds() != -1) {
			clip->setTrackEnd(trackEnd);
		} else {
			clip->setTrackEnd(trackStart + cropDuration);
		}
		clip->setParentTrack(0, trackNum);
		clip->setDescription(description);
	}

	return clip;
}*/

bool DocClipRef::referencesClip(DocClipBase *clip) const
{
	return m_clip->referencesClip(clip);
}

uint DocClipRef::numReferences() const
{
	return m_clip->numReferences();
}

double DocClipRef::framesPerSecond() const
{
	if(m_parentTrack) {
		return m_parentTrack->framesPerSecond();
	} else {
		return m_clip->framesPerSecond();
	}
}

QDomDocument DocClipRef::toXML() const
{
	QDomDocument doc = m_clip->toXML();

	QDomElement clip = doc.documentElement();
	if(clip.tagName() != "clip") {
		kdError() << "Expected tagname of 'clip' in DocClipRef::toXML(), expect things to go wrong!" << endl;
	}

	QDomElement position = doc.createElement("position");
	position.setAttribute("track", QString::number(trackNum()));
	position.setAttribute("trackstart", QString::number(trackStart().seconds(), 'f', 10));
	position.setAttribute("cropstart", QString::number(cropStartTime().seconds(), 'f', 10));
	position.setAttribute("cropduration", QString::number(cropDuration().seconds(), 'f', 10));
	position.setAttribute("trackend", QString::number(trackEnd().seconds(), 'f', 10));

	clip.appendChild(position);

	doc.appendChild(clip);
	
	return doc;
}

bool DocClipRef::matchesXML(const QDomElement &element) const
{
	bool result = false;
	if(element.tagName() == "clip")
	{
		QDomNodeList nodeList = element.elementsByTagName("position");
		if(nodeList.length() > 0) {
			if(nodeList.length() > 1) {
				kdWarning() << "More than one position in XML for a docClip? Only matching first one" << endl;
			}
			QDomElement positionElement = nodeList.item(0).toElement();

			if(!positionElement.isNull()) {
				result = true;
				if(positionElement.attribute("track").toInt() != trackNum()) result = false;
				if(positionElement.attribute("trackstart").toInt() != trackStart().seconds()) result = false;
				if(positionElement.attribute("cropstart").toInt() != cropStartTime().seconds()) result = false;
				if(positionElement.attribute("cropduration").toInt() != cropDuration().seconds()) result = false;
				if(positionElement.attribute("trackend").toInt() != trackEnd().seconds()) result = false;
			}
		}
	}

	return result;
}

GenTime DocClipRef::duration() const
{
	return m_clip->duration();
}

bool DocClipRef::durationKnown() const
{
	return m_clip->durationKnown();
}

QDomDocument DocClipRef::generateSceneList()
{
#warning - this may not be correct.
	return m_clip->generateSceneList();
}

const KURL &DocClipRef::fileURL() const
{
	return m_clip->fileURL();
}

uint DocClipRef::fileSize() const
{
	return m_clip->fileSize();
}
	
void DocClipRef::populateSceneTimes(QValueVector<GenTime> &toPopulate)
{
	QValueVector<GenTime> sceneTimes;
	
	m_clip->populateSceneTimes(sceneTimes);

	QValueVector<GenTime>::iterator itt = sceneTimes.begin();
	while(itt != sceneTimes.end())
	{
		GenTime convertedTime = (*itt) - cropStartTime();
		if((convertedTime >= GenTime(0)) && (convertedTime <= cropDuration())) {
			convertedTime += trackStart();
			toPopulate.append(convertedTime);
		}

		++itt;
	}
}

QDomDocument DocClipRef::sceneToXML(const GenTime &startTime, const GenTime &endTime)
{
	return m_clip->sceneToXML(startTime - trackStart() + cropStartTime(),
				  endTime - trackStart() + cropStartTime());
}
